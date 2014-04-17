/*
 * PUAE - The Un*x Amiga Emulator
 *
 * Interface to the SDL GUI
 * (initially was for GP2X)
 *
 * Copyright 2006 Mustafa TUFAN
 *
 */

#ifndef MENU_H
#define MENU_H
#include <stdio.h>
#include <SDL/SDL.h>
#ifdef __APPLE__
#include <SDL_ttf.h>
#else
#include <SDL/SDL_ttf.h>
#endif

#define iconsizex 100
#define iconsizey 120
#define bosluk 10
#define TITLE_X 52
#define TITLE_Y 9

extern SDL_Surface *display;
#ifdef USE_GL
extern struct gl_buffer_t glbuffer;
extern void flush_gl_buffer (const struct gl_buffer_t *buffer, int first_line, int last_line);
extern void render_gl_buffer (const struct gl_buffer_t *buffer, int first_line, int last_line);
#endif

void write_text(int x, int y, const char* txt);
void blit_image(SDL_Surface* img, int x, int y);
void selected_hilite (int ix, int iy, int mx, int my, SDL_Surface* img, int hangi);

enum { menu_sel_foo, menu_sel_expansion, menu_sel_prefs, menu_sel_keymaps, menu_sel_floppy, menu_sel_reset, menu_sel_storage, menu_sel_run, menu_sel_exit, menu_sel_tweaks };
void menu_load_surface(SDL_Surface *newmenu);
void menu_restore_surface(void);

extern SDL_Surface* pMouse_Pointer;
extern SDL_Surface* pMenu_Surface;
extern SDL_Surface* icon_expansion;
extern SDL_Surface* icon_preferences;
extern SDL_Surface* icon_keymaps;
extern SDL_Surface* icon_floppy;
extern SDL_Surface* icon_reset;
extern SDL_Surface* icon_storage;
extern SDL_Surface* icon_run;
extern SDL_Surface* icon_exit;
//extern SDL_Surface* icon_tweaks;

extern TTF_Font *amiga_font;
extern SDL_Color text_color;
extern SDL_Rect rect;

#endif
