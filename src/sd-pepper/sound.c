 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Pepper audio to be used in Native Client builds.
  *
  * Copyright 1997 Bernd Schmidt
  * Copyright 2003 Richard Drummond
  * Copyright 2013 Christian Stefansen
  *
  */

#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"

#include "sysconfig.h"
#include "sysdeps.h"

#include "custom.h"
#include "gui.h"
#include "options.h"
#include "gensound.h"
#include "sounddep/sound.h"
#include "threaddep/thread.h"
#include "writelog.h"

/* guidep == gui-html is currently the only way to build with Pepper. */
#include "guidep/ppapi.h"

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

static PPB_Audio *ppb_audio_interface;
static PPB_AudioConfig *ppb_audio_config_interface;
static PP_Resource audio_resource;
static PP_Resource audio_config;
static PP_Instance pp_instance;

int have_sound = 0;

uae_u16 *paula_sndbuffer;
uae_u16 *paula_sndbuffer_front_buffer;
uae_u16 *paula_sndbufpt;
int paula_sndbufsize;

static uint32_t sample_rate_to_int(PP_AudioSampleRate sample_rate) {
    switch (sample_rate) {
    case PP_AUDIOSAMPLERATE_44100: return 44100;
    case PP_AUDIOSAMPLERATE_48000: return 48000;
    case PP_AUDIOSAMPLERATE_NONE:
        write_log("Audio sample rate unavailable (PP_AUDIOSAMPLERATE_NONE).\n");
        return 0;
    default:
        write_log("Unknown PP_AudioSampleRate enum %d.\n", sample_rate);
        return 0;
    }
}

static uint32_t audio_config_is_ok(PP_Resource audio_config) {
    return sample_rate_to_int(ppb_audio_config_interface->
            GetSampleRate(audio_config));
}

static void adjust_prefs(PP_Resource audio_config) {
    currprefs.sound_freq =
            sample_rate_to_int(ppb_audio_config_interface->
                               GetSampleRate(audio_config));
    currprefs.sound_latency =
            ppb_audio_config_interface->GetSampleFrameCount(audio_config) *
            1000 / currprefs.sound_freq;
    currprefs.sound_stereo = 1;
}


int setup_sound(void)
{
    ppb_audio_interface = (PPB_Audio *) NaCl_GetInterface(PPB_AUDIO_INTERFACE);
    ppb_audio_config_interface = (PPB_AudioConfig *)
        NaCl_GetInterface(PPB_AUDIO_CONFIG_INTERFACE);
    pp_instance = NaCl_GetInstance();

    if (!ppb_audio_interface) {
        write_log("Could not acquire PPB_Audio interface.\n");
        return 0;
    }
    if (!ppb_audio_config_interface) {
        write_log("Could not acquire PPB_AudioConfig interface.\n");
        return 0;
    }
    if (!pp_instance) {
        write_log("Could not find current Pepper instance.\n");
        return 0;
    }

    if (!init_sound()) return 0;
    close_sound();

    write_log("Pepper audio successfully set up.\n");
    write_log("Frequency: %d\n", currprefs.sound_freq);
    write_log("Stereo   : %d\n", currprefs.sound_stereo);
    write_log("Latency  : %d\n", currprefs.sound_latency);

    init_sound_table16();
    sample_handler = sample16s_handler;
    obtainedfreq = currprefs.sound_freq;
    have_sound = 1;
    sound_available = 1;
    update_sound (fake_vblank_hz, 1, currprefs.ntscmode);

    return sound_available;
}

static volatile int ready_to_swap = 1;

/* The renderer (Chrome) calls this function to refill the
 * audio buffer. We should never block here.
 */
static void sound_callback(void* samples,
                           uint32_t buffer_size,
                           void* data) {
    memcpy(samples, paula_sndbuffer_front_buffer,
            min(paula_sndbufsize, buffer_size));
    ready_to_swap = 1;
}

void finish_sound_buffer (void)
{
    if (currprefs.turbo_emulation)
        return;
#ifdef DRIVESOUND
    driveclick_mix ((uae_s16 *) paula_sndbuffer, paula_sndbufsize / 2,
            currprefs.dfxclickchannelmask);
#endif
    if (!have_sound)
        return;

    if (gui_data.sndbuf_status == 3)
        gui_data.sndbuf_status = 0;

    /* Doing a busy wait to stall the emulator if it's ahead. */
    /* TODO(cstefansen): Determine if audio busy wait can be avoided. Using
     * semaphores has been observed to degrade performance severely. */
    while (!ready_to_swap) { }
    ready_to_swap = 0;
    uae_u16 *temp = paula_sndbuffer;
    paula_sndbuffer = paula_sndbuffer_front_buffer;
    paula_sndbuffer_front_buffer = temp;
}

int init_sound (void)
{
    /* If setup_sound wasn't called or didn't complete successfully. */
    if (!ppb_audio_interface) {
        write_log("init_sound called, but audio not set up yet.\n");
        return 0;
    }
    /* If sound is set to none (0) or interrupts (1). */
    if (currprefs.produce_sound <= 1) {
        write_log("init_sound called, but UAE is configured not to produce"
                  " sound.\n");
        return 0;
    }

    PP_AudioSampleRate sample_rate = ppb_audio_config_interface->
            RecommendSampleRate(pp_instance);
    uint32_t frame_count = ppb_audio_config_interface->
        RecommendSampleFrameCount(pp_instance,
                                  sample_rate,
                                  sample_rate * currprefs.sound_latency / 1000);

    audio_config = ppb_audio_config_interface->
            CreateStereo16Bit(pp_instance,
                              sample_rate,
                              frame_count);
    if (!audio_config) {
        write_log("Could not create Pepper audio config.\n");
        return 0;
    }

    /* Adjust preferences to reflect what the underlying system gave us. */
    if (!audio_config_is_ok(audio_config)) {
        write_log("Pepper AudioConfig failed.\n");
        return 0;
    }
    adjust_prefs(audio_config);

    audio_resource = ppb_audio_interface->Create(
            pp_instance,
            audio_config,
            sound_callback,
            0 /* user_data */);
    if (!audio_resource) {
        write_log("Could not create a Pepper audio resource.\n");
        return 0;
    }

    paula_sndbufsize = frame_count * 2 /* stereo */ * 2 /* 16-bit */;
    paula_sndbufpt = paula_sndbuffer = malloc(paula_sndbufsize);
    paula_sndbuffer_front_buffer = malloc(paula_sndbufsize);
    memset(paula_sndbuffer_front_buffer, 0, paula_sndbufsize);

    clear_sound_buffers();
#ifdef DRIVESOUND
    driveclick_reset();
#endif

    if (!ppb_audio_interface->StartPlayback(audio_resource)) {
        write_log("Could not start Pepper audio playback.\n");
        return 0;
    }
    return 1;
}

void close_sound (void) {
    ppb_audio_interface->StopPlayback(audio_resource);

    /* TODO(cstefansen): Proper audio system shutdown. */
}

void reset_sound (void) {
    clear_sound_buffers();
}

void pause_sound (void) {
    if (!ppb_audio_interface->StopPlayback(audio_resource)) {
        write_log("Could not stop Pepper audio playback.\n");
    }
}

void resume_sound (void) {
    if (!ppb_audio_interface->StartPlayback(audio_resource)) {
        write_log("Could not start Pepper audio playback.\n");
    }
}

void sound_volume (int dir) {
    DEBUG_LOG("Sound volume adjustment not implemented.\n");
}

/* Handle audio specific cfgfile options. */
void audio_default_options (struct uae_prefs *p)
{
}

void audio_save_options (FILE *f, const struct uae_prefs *p)
{
}

int audio_parse_option (struct uae_prefs *p, const char *option,
                        const char *value) {
    return 0;
}

void master_sound_volume (int dir)
{
}

void sound_mute (int newmute)
{
}

void restart_sound_buffer (void)
{
}

