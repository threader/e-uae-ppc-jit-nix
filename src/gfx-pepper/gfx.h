 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Pepper graphics to be used in for Native Client builds.
  *
  * Copyright 2013 Christian Stefansen
  */

#ifndef UAE_GFX_PEPPER_GFX_H
#define UAE_GFX_PEPPER_GFX_H

#define GFX_NAME "pepper"

struct uae_prefs; /* Defined in options.h. */

/* Call this function to have the Pepper graphics subsystem update itself
 * when the screen size changes. */
void screen_size_changed(unsigned int width, unsigned int height);

/* The following functions must be provided by any graphics system and hence
 * also by Pepper.
 */
void setmaintitle(void);
int gfx_parse_option(struct uae_prefs *p, const char *option, const char *value);
void gfx_default_options(struct uae_prefs *p);

// TODO(cstefansen): Remove these defines. We don't support Picasso, but the
// codebase has unguarded PICASSO96-specific code all over so it's hard to
// compile without these.
#define PICASSO96_SUPPORTED
#define PICASSO96

#ifdef PICASSO96
/* TODO(cstefansen): Cyclic inclusion issue: sysdeps.h includes this file. If
 * we include picasso96.h, it will not know TCHAR which is defined later in sysdeps.h.
 * Should be a simple matter of cleaning up sysdeps.h. */
//#include "picasso96.h"
//int DX_Fill(int dstx, int dsty, int width, int height, uae_u32 color,
//            RGBFTYPE rgbtype);
// void DX_Invalidate(int first, int last);
int WIN32GFX_IsPicassoScreen(void);
int DX_FillResolutions (uae_u16 *ppixel_format);
#endif /* PICASSO96 */

#endif /* UAE_GFX_PEPPER_GFX_H */
