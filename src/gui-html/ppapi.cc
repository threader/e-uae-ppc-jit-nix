/*
 * UAE - The Un*x Amiga Emulator
 *
 * PPAPI module and instance for running in Chrome.
 *
 * Copyright 2012, 2013 Christian Stefansen
 */

#include "ppapi.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <istream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "GLES2/gl2.h"

#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef ENABLE_BUILD_KEY
#include "build.key.c"
#endif

#ifdef USE_SDL
#include "SDL/SDL.h"
#include "SDL/SDL_nacl.h"
#endif /* USE_SDL */

// The main function of the emulator thread.
extern "C" void real_main(int argc, const char **argv);

// We call this function to relay messages from the HTML UI to the messaging
// pipe that the emulator reads.
extern "C" int handle_message(const char* msg);

// We call this function to queue up input events. The emulator handles these
// in handle_events().
extern "C" int push_event(PP_Resource event);

class UAEInstance : public pp::Instance, public pp::MouseLock {
 private:
  static UAEInstance* the_instance_;  // Ensure we only create one instance.
  pthread_t uae_main_thread_;         // This thread will run real_main().
  bool first_changed_view_;           // Ensure we initialize an instance only
                                      // once.
  int width_; int height_;            // Dimension of the video screen.
  bool mouse_locked_;                 // Whether mouse is locked or not.
  pp::Fullscreen fullscreen_;
  pp::CompletionCallbackFactory<UAEInstance> cc_factory_;

  // This function allows us to delay game start until all
  // resources are ready.
  void StartUAEInNewThread(int32_t dummy) {
    pthread_create(&uae_main_thread_, NULL, &UAEInstance::LaunchUAE, this);
  }

  // Launches the actual game, e.g., by calling real_main().
  static void* LaunchUAE(void* data) {
      // Get access to instance object.
      UAEInstance* this_instance = reinterpret_cast<UAEInstance*>(data);
      pp::MessageLoop msg_loop = pp::MessageLoop(this_instance);
      if (PP_OK != msg_loop.AttachToCurrentThread()) {
          printf("Failed to attach a message loop to the UAE thread.\n");
      }

      // Call the UAE 'real_main' defined in main.c
      const char *argv[] = { "", "-f", "default.uaerc" };
      int argc = 3;
      real_main(argc, argv);
      return 0;
  }

  void DidLockMouse(int32_t result) {
      mouse_locked_ = result == PP_OK;
      if (result != PP_OK) {
          std::stringstream ss;
          ss << "Mouselock failed with failed with error number " << result;
          PostMessage(pp::Var(ss.str()));
      }
  }

 public:
  explicit UAEInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::MouseLock(this),
      uae_main_thread_(NULL),
      first_changed_view_(true),
      width_(720), height_(568),
      mouse_locked_(false),
      cc_factory_(this),
      fullscreen_(this) {
    assert(the_instance_ == NULL);
    the_instance_ = this;
  }

  virtual ~UAEInstance() {
    // Wait for game thread to finish.
    if (uae_main_thread_) { pthread_join(uae_main_thread_, NULL); }
  }

  // This function is called with the HTML attributes of the embed tag,
  // which can be used in lieu of command line arguments.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
      // UAE requires mouse and keyboard events; add more if necessary.
      RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE|
                         PP_INPUTEVENT_CLASS_KEYBOARD);

      printf("Received %d arguments from HTML:\n", argc);
    for (unsigned int i = 0; i < argc; i++) {
      printf("    [%d] %s=\"%s\"\n", i, argn[i], argv[i]);
    }
    // From here we could pass the arguments to the emulator, if it was
    // needed.

    return true;
  }

  // This crucial function forwards PPAPI events to the emulator.
  virtual bool HandleInputEvent(const pp::InputEvent& event) {
      switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
          if (!mouse_locked_) {
              LockMouse(cc_factory_.NewCallback(
                      &UAEInstance::DidLockMouse));
              return true;
          }
          break;
      }

      case PP_INPUTEVENT_TYPE_KEYDOWN: {
          pp::KeyboardInputEvent key_event(event);
          uint32_t modifiers = event.GetModifiers();
          uint32_t keycode = key_event.GetKeyCode();
          if (keycode == 70 /* 'f' */ &&
              (modifiers & PP_INPUTEVENT_MODIFIER_SHIFTKEY) &&
              (modifiers & PP_INPUTEVENT_MODIFIER_CONTROLKEY)) {
              printf("Fullscreen toggle.\n");
              if (fullscreen_.IsFullscreen()) {
                  // Leaving full-screen mode also unlocks the mouse if it was
                  // locked.
                  if (!fullscreen_.SetFullscreen(false)) {
                      printf("Could not leave fullscreen mode.\n");
                  }
              } else {
                  if (!fullscreen_.SetFullscreen(true)) {
                      printf("Could not set fullscreen mode.\n");
                  } else {
                      printf("Fullscreen mode set.\n");
                  }
              }
              return true;
          }
          break;
      }
      default:
          // Ignore all other events when not in mouse lock mode.
          if (!mouse_locked_) return false;
          break;
      }
/* TODO(cstefansen): remove ifdef; this should just call a 'push_event'
 * function in the current gfx-dep or something.
 */
#ifdef USE_SDL
      SDL_NACL_PushEvent(event);
#else
      if (!push_event(event.pp_resource())) return false;
#endif /* USE_SDL */
      return true;
  }

  virtual void HandleMessage(const pp::Var &message) {
      if (!message.is_string()) {
          PostMessage("Warning: non-string message posted to NaCl module "
                      "- ignoring.\n");
          return;
      }
      handle_message(message.AsString().c_str());
      return;
  }

  // This function is called for various reasons, e.g. visibility and page
  // size changes. We ignore these calls except for the first
  // invocation, which we use to start the game.
  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
      if (first_changed_view_) {
          first_changed_view_ = false;
#ifdef USE_SDL
          // Make SDL aware of the Pepper instance.
          // TODO(cstefansen): do not ifdef; this should just call an 'init'
          // function in the current gfx-dep or something.
          SDL_NACL_SetInstance(pp_instance(), width_, height_);
#endif
          StartUAEInNewThread(0);
          return;
      }

      // Send resize message to emulator.
      std::stringstream msg;
      msg << "resize " << position.width() << " " << position.height();
      HandleMessage(pp::Var(msg.str()));
  }

  virtual void MouseLockLost() {
      if (mouse_locked_) {
          PostMessage(pp::Var(std::string("Mouse lock lost.\n")));
          mouse_locked_ = false;
      }
  }

  static UAEInstance* GetInstance() { return the_instance_; }

  // To work with UAE this function returns the number of bytes read, or 0
  // if anything went wrong.
  size_t LoadUrl(const char* name, char** data) {
      // This function is using blocking calls, so it should not be used (and
      // wouldn't work) on the main thread.
      if (pp::Module::Get()->core()->IsMainThread()) {
          *data = NULL;
          return 0;
      }

      pp::URLLoader loader(this);
      pp::URLRequestInfo request(this);
      request.SetURL(name);
      request.SetMethod("GET");
      request.SetFollowRedirects(true);
      request.SetAllowCredentials(true);
      request.SetAllowCrossOriginRequests(true);

      int32_t result =
          loader.Open(request, pp::CompletionCallback::CompletionCallback());
      if (PP_OK != result) {
          printf("Could not open file %s. PP error %d.\n", name, result);
          return 0;
      }

      pp::URLResponseInfo response = loader.GetResponseInfo();
      int32_t status_code = response.GetStatusCode();

      std::vector<char> dst;
      static const int32_t BUFFER_SIZE = 65536;
      char buf[BUFFER_SIZE];
      do {
          result = loader.ReadResponseBody(buf,
                                          BUFFER_SIZE,
                                          pp::CompletionCallback
                                              ::CompletionCallback());
          if (result > 0) {
              std::vector<char>::size_type pos = dst.size();
              dst.resize(pos + result);
              memcpy(&(dst)[pos], buf, result);
          }
      } while (result > 0);

      printf("%s: got HTTP status code %d and %d bytes while loading %s.\n",
             (status_code == 200 ? "Success" : "Failed"), status_code,
             dst.size(), name);

      if (status_code == 200) {
#ifdef ENABLE_BUILD_KEY
          // Detect naive XOR encryption and decrypt if needed.
          char encryption_marker[] = "AMIGA_RULEZ_XORENC";
          const int kEncryptionMarkerLen =
              sizeof(encryption_marker) - 1; /* No \0 terminator. */
          int adjusted_len = dst.size() - kEncryptionMarkerLen;

          if (adjusted_len >= 0 &&
              !memcmp(encryption_marker, dst.data(), kEncryptionMarkerLen)) {
              *data = reinterpret_cast<char*>(malloc(sizeof(char) *
                                                     adjusted_len));
              for(std::vector<char>::size_type i = 0;
                  i < adjusted_len; ++i) {
                  (*data)[i] = dst[i + kEncryptionMarkerLen] ^
                      build_key[i % build_key_len];
              }
              return adjusted_len;
          }
#endif  // ENABLE_BUILD_KEY

          *data = reinterpret_cast<char*>(malloc(sizeof(char) *
                                                 dst.size()));
          std::copy(dst.begin(), dst.end(), *data);
          return dst.size();

      } else {
          *data = NULL;
          return 0;
      }
  }

  const void *GetInterface(const char *interface_name) {
      PPB_GetInterface get_interface_function =
          pp::Module::Get()->get_browser_interface();
      return get_interface_function(interface_name);
  }
};

UAEInstance* UAEInstance::the_instance_ = NULL;

// Helper to allow calls from C to LoadUrl.
size_t NaCl_LoadUrl(const char* name, char** data) {
  UAEInstance* instance = UAEInstance::GetInstance();
  assert(instance);
  return instance->LoadUrl(name, data);
}

// Helper to allow C code to get PPB interfaces.
const void *NaCl_GetInterface(const char *interface_name) {
    UAEInstance* instance = UAEInstance::GetInstance();
    assert(instance);
    return instance->GetInterface(interface_name);
}

PP_Instance NaCl_GetInstance() {
    UAEInstance* instance = UAEInstance::GetInstance();
    assert(instance);
    return instance->pp_instance();
}

class UAEModule : public pp::Module {
 public:
    UAEModule() : pp::Module() {}
    virtual ~UAEModule() {
        glTerminatePPAPI();
    }

    virtual bool Init() {
        // Force stdout to be line-buffered (not the default in glibc).
        setvbuf(stdout, NULL, _IOLBF, 0);
        return glInitializePPAPI(get_browser_interface()) == GL_TRUE;
    }

    virtual pp::Instance* CreateInstance(PP_Instance instance) {
        return new UAEInstance(instance);
    }
};

namespace pp {
Module* CreateModule() {
    return new UAEModule();
}
}  // namespace pp
