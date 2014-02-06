/*
  * UAE - The Un*x Amiga Emulator
  *
  * Pepper 2D graphics to be used for Native Client builds.
  *
  * Copyright 2013 Christian Stefansen
  *
  */

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"

/* guidep == gui-html is currently the only way to build with Pepper. */
#include "guidep/ppapi.h"
#include "options.h"
#include "writelog.h"
#include "xwin.h"

static PPB_Core *ppb_core_interface;
static PPB_Graphics2D *ppb_g2d_interface;
static PPB_Instance *ppb_instance_interface;
static PPB_ImageData *ppb_image_data_interface;

static PP_ImageDataFormat preferredFormat;
static PP_Instance pp_instance;
static PP_Resource image_data;
static PP_Resource graphics_context;

static int graphics_initialized = 0;

static const struct PP_Point origo = {0, 0};
static struct PP_Point top = {0, 0};
static struct PP_Rect src_rect;
static struct PP_Size canvasSize; /* The canvas available to the Amiga. */
static struct PP_Size screenSize; /* The actual resolution of the embed. */
static uint32_t alpha_mask;

void screen_size_changed_2d(int32_t width, int32_t height) {
    if (!graphics_initialized ||
        (screenSize.width == width && screenSize.height == height) ||
        width < canvasSize.width || height < canvasSize.height) {
        return;
    }
    screenSize.width = width;
    screenSize.height = height;

    /* Unbind and release old graphics context. */
    if (!ppb_instance_interface->BindGraphics(pp_instance, 0)) {
        write_log("Failed to unbind old context from instance.\n");
        return;
    }
    ppb_core_interface->ReleaseResource(graphics_context);

    /* Create new graphics context. */
    graphics_context = ppb_g2d_interface->Create(
            pp_instance,
            &screenSize,
            PP_TRUE /* is_always_opaque */);
    if (!graphics_context) {
        write_log("Could not obtain a PPB_Graphics2D context.\n");
        return;
    }
    if (!ppb_instance_interface->BindGraphics(pp_instance, graphics_context)) {
        write_log("Failed to bind context to instance.\n");
        return;
    }

    /* Give the screen around the emulator a dark gray background. */
    PP_Resource temp_image_data =
      ppb_image_data_interface->Create(pp_instance, preferredFormat,
                                       &screenSize,
                                       /* init_to_zero */ PP_FALSE);
    if (!temp_image_data) {
        write_log("Could not create image data.\n");
        return;
    }
    uint32_t* pixels = ppb_image_data_interface->Map(temp_image_data);
    uint32_t* end = pixels + screenSize.width * screenSize.height;
    for (uint32_t* p = pixels; p < end; ++p) {
        /* This assumes each color channel is 8-bits  */
        *p = alpha_mask | 0x33333333 /* dark grey background */;
    }
    ppb_image_data_interface->Unmap(temp_image_data);
    ppb_g2d_interface->ReplaceContents(graphics_context, temp_image_data);
    ppb_g2d_interface->Flush(graphics_context, PP_BlockUntilComplete());
    ppb_core_interface->ReleaseResource(temp_image_data);

    /* Set the top corner for the canvas on the new screen. */
    top.x = (screenSize.width - canvasSize.width) / 2;
    top.y = (screenSize.height - canvasSize.height) / 2;
}


STATIC_INLINE void pepper_graphics2d_flush_screen(
        struct vidbuf_description *gfxinfo,
        int first_line, int last_line) {
    /* We can't use Graphics2D->ReplaceContents here because the emulator
     * only draws partial updates in the buffer and expects the remaining lines
     * to be unchanged from the previous frame. ReplaceContents would thus
     * require a memcpy of the complete canvas for every frame, as the
     * buffer given by Chrome when using ReplaceContents followed by
     * Create and Map will generally not contain the previous frame,
     * but the one before that (or garbage).
     */
    ppb_g2d_interface->PaintImageData(graphics_context, image_data,
                                      &top, &src_rect);
    ppb_g2d_interface->Flush(graphics_context, PP_BlockUntilComplete());
    /* TODO(cstefansen): Properly throttle 2D graphics to 50 Hz in PAL mode. */
}

int graphics_2d_subinit(uint32_t *Rmask, uint32_t *Gmask, uint32_t *Bmask,
                        uint32_t *Amask) {
    /* Pepper Graphics2D setup. */
    ppb_instance_interface = (PPB_Instance *)
        NaCl_GetInterface(PPB_INSTANCE_INTERFACE);
    if (!ppb_instance_interface) {
        write_log("Could not acquire PPB_Instance interface.\n");
        return 0;
    }
    ppb_g2d_interface = (PPB_Graphics2D *)
        NaCl_GetInterface(PPB_GRAPHICS_2D_INTERFACE);
    if (!ppb_g2d_interface) {
        write_log("Could not acquire PPB_Graphics2D interface.\n");
        return 0;
    }
    ppb_core_interface = (PPB_Core *) NaCl_GetInterface(PPB_CORE_INTERFACE);
    if (!ppb_core_interface) {
        write_log("Could not acquire PPB_Core interface.\n");
        return 0;
    }
    ppb_image_data_interface = (PPB_ImageData *)
        NaCl_GetInterface(PPB_IMAGEDATA_INTERFACE);
    if (!ppb_image_data_interface) {
        write_log("Could not acquire PPB_ImageData interface.\n");
        return 0;
    }
    pp_instance = NaCl_GetInstance();
    if (!pp_instance) {
        write_log("Could not find current Pepper instance.\n");
        return 0;
    }

    screenSize.width = canvasSize.width = gfxvidinfo.width;
    screenSize.height = canvasSize.height = gfxvidinfo.height;
    src_rect.point = origo;
    src_rect.size = canvasSize;
    graphics_context =
            ppb_g2d_interface->Create(pp_instance,
                                      &screenSize,
                                      PP_TRUE /* is_always_opaque */);
    if (!graphics_context) {
        write_log("Could not obtain a PPB_Graphics2D context.\n");
        return 0;
    }
    if (!ppb_instance_interface->BindGraphics(pp_instance, graphics_context)) {
        write_log("Failed to bind context to instance.\n");
        return 0;
    }

    preferredFormat = ppb_image_data_interface->GetNativeImageDataFormat();
    switch (preferredFormat) {
    case PP_IMAGEDATAFORMAT_BGRA_PREMUL:
        *Rmask = 0x00FF0000, *Gmask = 0x0000FF00, *Bmask = 0x000000FF;
        *Amask = 0xFF000000;
        alpha_mask = *Amask;
        break;
    case PP_IMAGEDATAFORMAT_RGBA_PREMUL:
        *Rmask = 0x000000FF, *Gmask = 0x0000FF00, *Bmask = 0x00FF0000;
        *Amask = 0xFF000000;
        alpha_mask = *Amask;
        break;
    default:
        write_log("Unrecognized preferred image data format: %d.\n",
                  preferredFormat);
        return 0;
    }
    image_data = ppb_image_data_interface->Create(pp_instance, preferredFormat,
            &canvasSize, /* init_to_zero = */ PP_FALSE);
     if (!image_data) {
         write_log("Could not create image data.\n");
         return 0;
     }

    /* UAE gfxvidinfo setup. */
    gfxvidinfo.pixbytes = 4; /* 32-bit graphics */
    gfxvidinfo.rowbytes = gfxvidinfo.width * gfxvidinfo.pixbytes;
    gfxvidinfo.bufmem = (uae_u8 *) ppb_image_data_interface->Map(image_data);
    gfxvidinfo.emergmem = 0;
    gfxvidinfo.flush_screen = pepper_graphics2d_flush_screen;

    graphics_initialized = 1;
    return 1;
}
