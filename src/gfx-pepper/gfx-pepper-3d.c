/*
  * UAE - The Un*x Amiga Emulator
  *
  * Pepper 3D graphics to be used for Native Client builds.
  *
  * Copyright 2013 Christian Stefansen
  *
  */
#include <sys/time.h>
#include <unistd.h>

#include "GLES2/gl2.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/gles2/gl2ext_ppapi.h"

/* guidep == gui-html is currently the only way to build with Pepper. */
#include "guidep/ppapi.h"
#include "options.h"
#include "writelog.h"
#include "xwin.h"

static PP_Instance pp_instance;
static PP_Resource graphics_context;
static PPB_Graphics3D *ppb_g3d_interface;

/* Hard-coding for GL_UNSIGNED_SHORT_5_6_5 for now. */
static const GLenum pixel_data_type = GL_UNSIGNED_SHORT_5_6_5;

static struct timeval tv_now, tv_prev;

int graphics_3d_subinit(uint32_t *Rmask, uint32_t *Gmask, uint32_t *Bmask,
                        uint32_t *Amask);

/* This function get call to notify us that the resolution of the embed
 * has changed. In 3D mode Chrome automatically stretches the embed tag to take
 * up the whole screen, so there is nothing for us to do.
 */
void screen_size_changed_3d(int32_t width, int32_t height) {}

STATIC_INLINE void pepper_graphics3d_flush_screen(
        struct vidbuf_description *gfxinfo,
        int first_line, int last_line) {
    glTexSubImage2D (GL_TEXTURE_2D, 0 /* level */, 0, 0,
                     gfxvidinfo.width, gfxvidinfo.height, GL_RGB,
                     pixel_data_type, gfxinfo->bufmem);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    ppb_g3d_interface->SwapBuffers(graphics_context, PP_BlockUntilComplete());

    /* TODO(cstefansen): Properly throttle 3D graphics to 50 Hz in PAL mode. */
    /* Currently, the frame rate is throttled by finish_sound_buffer
     * in sd-pepper/sound.c, but this is less than ideal, as it only
     * provides throttling when sound is playing. It does, however,
     * mean that scrolling is noticably smoother when no music is
     * playing because the emulator can go to 60 Hz. */
}

static GLuint compileShader(GLenum type, const char *data) {
    const char *shaderStrings[1];
    shaderStrings[0] = data;

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, shaderStrings, NULL );
    glCompileShader(shader);
    return shader;
}

static int round_up_to_power_of_2 (int value)
{
    int result = 1;
    while (result < value)
        result *= 2;
    return result;
}


int graphics_3d_subinit(uint32_t *Rmask, uint32_t *Gmask, uint32_t *Bmask,
                        uint32_t *Amask) {
    /* Pepper Graphics3D setup. */
    PPB_Instance *ppb_instance_interface =
        (PPB_Instance *) NaCl_GetInterface(PPB_INSTANCE_INTERFACE);
    if (!ppb_instance_interface) {
        write_log("Could not acquire PPB_Instance interface.\n");
        return 0;
    }
    ppb_g3d_interface =
        (PPB_Graphics3D *) NaCl_GetInterface(PPB_GRAPHICS_3D_INTERFACE);
    if (!ppb_g3d_interface) {
        write_log("Could not acquire PPB_Graphics3D interface.\n");
        return 0;
    }
    pp_instance = NaCl_GetInstance();
    if (!pp_instance) {
        write_log("Could not find current Pepper instance.\n");
        return 0;
    }

    int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
      PP_GRAPHICS3DATTRIB_SAMPLES, 0,
      PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
      PP_GRAPHICS3DATTRIB_WIDTH, gfxvidinfo.width,
      PP_GRAPHICS3DATTRIB_HEIGHT, gfxvidinfo.height,
      PP_GRAPHICS3DATTRIB_GPU_PREFERENCE,
          PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_PERFORMANCE,
      PP_GRAPHICS3DATTRIB_NONE
    };

    graphics_context = ppb_g3d_interface->Create(
            pp_instance,
            0 /* share_context */,
            attribs);
    if (!graphics_context) {
        write_log("Could not obtain a PPB_Graphics3D context.\n");
        return 0;
    }
    if (!ppb_instance_interface->BindGraphics(pp_instance, graphics_context)) {
        write_log("Failed to bind context to instance.\n");
        return 0;
    }
    glSetCurrentContextPPAPI(graphics_context);

    /* UAE gfxvidinfo setup. */
    /* TODO(cstefansen): Implement gfx double-buffering if perf. mandates it. */
    gfxvidinfo.pixbytes = 2; /* 16-bit graphics */
    gfxvidinfo.rowbytes = gfxvidinfo.width * gfxvidinfo.pixbytes;
    gfxvidinfo.bufmem =
            (uae_u8 *) calloc(round_up_to_power_of_2(gfxvidinfo.rowbytes),
                              round_up_to_power_of_2(gfxvidinfo.height));
    gfxvidinfo.flush_screen = pepper_graphics3d_flush_screen;
    *Rmask = 0x0000F800, *Gmask = 0x000007E0, *Bmask = 0x0000001F, *Amask = 0;

    /* OpenGL ES setup. */
    glViewport(0, 0, gfxvidinfo.width, gfxvidinfo.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);

    /* Uniform index. */
    enum {
        UNIFORM_VIDEOFRAME,
        UNIFORM_INPUTCOLOR,
        UNIFORM_THRESHOLD,
        NUM_UNIFORMS
    };
    GLint uniforms[NUM_UNIFORMS];

    /* Attribute index. */
    enum {
        ATTRIB_VERTEX,
        ATTRIB_TEXTUREPOSITON,
        NUM_ATTRIBUTES
    };

    static GLuint g_programObj;
    static GLuint g_textureID;

    char *g_VShaderData =
            "attribute vec4 position;"
            "attribute vec4 inputTextureCoordinate;"
            "varying vec2 textureCoordinate;"
            "void main()"
            "{"
            "gl_Position = position;"
            "textureCoordinate = inputTextureCoordinate.xy;"
            "}";

    char *g_FShaderData =
            "varying highp vec2 textureCoordinate;"
            "uniform sampler2D videoFrame;"
            "void main()"
            "{"
            "gl_FragColor = texture2D(videoFrame, textureCoordinate);"
            "}";

    GLuint g_vertexShader;
    GLuint g_fragmentShader;

    g_vertexShader = compileShader(GL_VERTEX_SHADER, g_VShaderData);
    g_fragmentShader = compileShader(GL_FRAGMENT_SHADER, g_FShaderData);

    g_programObj = glCreateProgram();
    glAttachShader(g_programObj, g_vertexShader);
    glAttachShader(g_programObj, g_fragmentShader);

    glBindAttribLocation(g_programObj, ATTRIB_VERTEX, "position");
    glBindAttribLocation(g_programObj, ATTRIB_TEXTUREPOSITON,
                         "inputTextureCoordinate");

    glLinkProgram(g_programObj);

    uniforms[UNIFORM_VIDEOFRAME] = glGetUniformLocation(g_programObj,
                                                        "videoFrame");
    uniforms[UNIFORM_INPUTCOLOR] = glGetUniformLocation(g_programObj,
                                                        "inputColor");
    uniforms[UNIFORM_THRESHOLD] = glGetUniformLocation(g_programObj,
                                                       "threshold");

    glGenTextures(1, &g_textureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint textureWidth = round_up_to_power_of_2(gfxvidinfo.width);
    GLint textureHeight = round_up_to_power_of_2(gfxvidinfo.height);
    glTexImage2D(GL_TEXTURE_2D, 0 /* level */, GL_RGB,
            textureWidth, textureHeight,
                 0, GL_RGB, pixel_data_type, gfxvidinfo.bufmem);

    static const GLfloat squareVertices[] = {
            -1.0f, -1.0,
             1.0f, -1.0,
            -1.0f,  1.0,
             1.0f,  1.0,
    };

    GLfloat xFraction = ((GLfloat) gfxvidinfo.width)  / (GLfloat) textureWidth;
    GLfloat yFraction = ((GLfloat) gfxvidinfo.height) / (GLfloat) textureHeight;
    static GLfloat textureVertices[] = {
            0.0f,                 0.0f /* yFraction */,
            0.0f /* xFraction */, 0.0f /* yFraction */,
            0.0f,                 0.0f,
            0.0f /* xFraction */, 0.0f,
    };
    textureVertices[1] = yFraction;
    textureVertices[2] = xFraction;
    textureVertices[3] = yFraction;
    textureVertices[6] = xFraction;

    glUseProgram(g_programObj);
    glUniform1i(uniforms[UNIFORM_VIDEOFRAME], 0);
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0,
                          textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);

    DEBUG_LOG("Shaders compiled and program linked.\n");

    GLenum glErrorStatus = glGetError();
    if (glErrorStatus) {
        write_log("GL error %d while initializing.\n", glErrorStatus);
        return 0;
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    ppb_g3d_interface->SwapBuffers(graphics_context, PP_BlockUntilComplete());

    gettimeofday(&tv_prev, NULL);

    return 1; /* Success! */
}


