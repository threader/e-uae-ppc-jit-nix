#ifndef SRC_MISC_H_INCLUDED
#define SRC_MISC_H_INCLUDED 1

/*
 * PUAE - The Un*x Amiga Emulator
 *
 * Misc
 *
 * Copyright 2010-2011 Mustafa TUFAN
 */

extern int ispressed (int key);

extern int D3D_goodenough (void);
extern int DirectDraw_CurrentRefreshRate (void);
extern int DirectDraw_GetVerticalBlankStatus (void);
extern double getcurrentvblankrate (void);
extern int isfullscreen (void);
extern void fetch_configurationpath (TCHAR *out, int size);
extern TCHAR* buf_out (TCHAR *buffer, int *bufsize, const TCHAR *format, ...);
extern TCHAR *au (const char *s);
extern char *ua (const TCHAR *s);
extern char *uutf8 (const char *s);
extern char *utf8u (const char *s);
extern int my_existsdir (const char *name);
extern bool target_graphics_buffer_update (void);
extern bool show_screen_maybe (bool show);
extern bool render_screen (bool immediate);
extern void show_screen (void);
extern TCHAR *au_copy (TCHAR *dst, int maxlen, const char *src);
extern FILE *my_opentext (const TCHAR *name);
extern int my_existsfile (const char *name);
extern void fetch_statefilepath (TCHAR *out, int size);
extern void fetch_datapath (TCHAR *out, int size);
extern void fetch_inputfilepath (TCHAR *out, int size);

#endif /* SRC_MISC_H_INCLUDED */
