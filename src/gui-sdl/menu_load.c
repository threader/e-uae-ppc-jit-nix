/*
 * PUAE - The Un*x Amiga Emulator
 *
 * Interface to the SDL GUI
 * (initially was for GP2X)
 *
 * Copyright 2006 Mustafa TUFAN
 *
 */

#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include <sys/stat.h>
#ifdef __APPLE__
#define st_mtim st_mtimespec
#undef _DARWIN_C_SOURCE
#endif

#include <sys/time.h>
#include <unistd.h>
#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include <SDL/SDL.h>
#ifdef __APPLE__
#include <SDL_image.h>
#include <SDL_ttf.h>
#else
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#endif
#include "button_mappings.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"


extern void blit_image(SDL_Surface* img, int x, int y);
extern SDL_Surface *display;
#ifdef USE_GL
#define NO_SDL_GLEXT
# include <SDL/SDL_opengl.h>
/* These are not defined in the current version of SDL_opengl.h. */
# ifndef GL_TEXTURE_STORAGE_HINT_APPLE
#  define GL_TEXTURE_STORAGE_HINT_APPLE 0x85BC
#  endif
# ifndef GL_STORAGE_SHARED_APPLE
#  define GL_STORAGE_SHARED_APPLE 0x85BF
# endif
extern struct gl_buffer_t glbuffer;
extern void render_gl_buffer (const struct gl_buffer_t *buffer, int first_line, int last_line);
extern void flush_gl_buffer (const struct gl_buffer_t *buffer, int first_line, int last_line);
#endif
extern SDL_Surface* tmpSDLScreen;
extern SDL_Surface* pMenu_Surface;
extern SDL_Color text_color;

#define MAX_FILES 1024
#define TITLE_X 52
#define TITLE_Y 9
#define STATUS_X 30
#define STATUS_Y 460

extern char launchDir[];
extern char yol[];
extern char msg[];
extern char msg_status[];

#include <sys/stat.h>
#include <fcntl.h>
#include "savestate.h"
static void touch(const char* fn) {
	int fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if(fd != -1) close(fd);
}

extern int my_existsfile (const char *name);
static void populate_savedir(const char* dir) {
	char buf[512];
	int i = 0;
	for(i = 0; i < 18; i++) {
		snprintf(buf, sizeof buf, "%s/save%.2d.uss", dir, i);
		if(!my_existsfile(buf)) touch(buf);
	}
}

static int isempty(const char *file) {
	struct stat s;
	return stat(file, &s) || !s.st_size;
}

static int filez_comp_name(const void* a, const void* b) {
	const char * const*sa = a, *const* sb = b;
	return strcmp(*sa, *sb);
}

static int ts_cmp(const struct timespec *a, const struct timespec *b) {
	#define TSCMP_B -1
	#define TSCMP_S 1
	return a->tv_sec > b->tv_sec ? TSCMP_B :
	       ( a->tv_sec < b->tv_sec ? TSCMP_S :
	       ( a->tv_nsec > b->tv_nsec ? TSCMP_B :
	       ( a->tv_nsec < b->tv_nsec ? TSCMP_S : 0)));
}

static int filez_comp_date(const void* a, const void* b) {
	const char * const*sa = a, *const* sb = b;
	struct stat sta, stb;
	char ba[512], bb[512];
	snprintf(ba, sizeof(ba), "%s/saves/%s", launchDir, *sa);
	snprintf(bb, sizeof(bb), "%s/saves/%s", launchDir, *sb);
	if(!stat(ba, &sta) && !stat(bb, &stb)) return ts_cmp(&sta.st_mtim, &stb.st_mtim);
	return strcmp(*sa, *sb);
}

static void dirz_restore_savestate(const char *fn) {
#ifdef SAVESTATE
	savestate_initsave(fn, 0, 0, 0);
	savestate_state = STATE_DORESTORE;
#endif
}
static void dirz_save_savestate(const char *fn) {
#ifdef SAVESTATE
	savestate_initsave(fn, 0, 0, 0);
	save_state(fn, "puae");
#endif
}

static SDL_Surface *pDirzMenu_Surface;
enum dirz_param { dz_floppy = 0, dz_rom, dz_saves };
int dirz (int parameter) {
	SDL_Event event;
	int getdir = 1;
	int loadloopdone = 0;
	int num_of_files = 0;
	int selected_item = 0;
	int q;
	int bas = 0;
	int ka = 0;
	int kb = 0;
	char **filez = malloc(MAX_FILES*sizeof(char *));
	int i;
	int paging = 18;

	if (!pDirzMenu_Surface) pDirzMenu_Surface = SDL_LoadBMP("guidep/images/menu_load.bmp");
	if (!pDirzMenu_Surface) {
		write_log ("SDLUI: Failed to load menu image\n");
		abort();
	}
	menu_load_surface(pDirzMenu_Surface);

	DIR *d;
	d = opendir(yol);
	struct dirent *ep;

	if (d == NULL) {
		write_log ("SDL_UI: opendir %s failed, trying current path\n", yol);
		strcpy(yol, "./");
		d = opendir(yol);
	}
	if (d == NULL) {
		write_log ("SDL_UI: opendir %s failed\n", yol);
	} else {
		if(parameter == dz_saves) populate_savedir(yol);
		for(i=0; i<MAX_FILES; i++) {
			ep = readdir(d);
			if (ep == NULL) {
				write_log ("SDL_UI: readdir %s failed\n", yol);
				break;
			} else if (ep->d_name[0] != '.')  {
				filez[num_of_files++] = strdup(ep->d_name);
			}
		}
		closedir(d);
		qsort(filez, num_of_files, sizeof(char**), parameter == dz_saves ? filez_comp_date : filez_comp_name);
	}
	if (num_of_files<18) {
		paging = num_of_files;
	}

	int need_redraw = 1;
	while (!loadloopdone) {
		while (SDL_PollEvent(&event)) {
			need_redraw = 1;
			if (event.type == SDL_QUIT) {
				loadloopdone = 1;
			}
			if (event.type == SDL_JOYBUTTONDOWN) {
				switch (event.jbutton.button) {
					case PLATFORM_BUTTON_UP: selected_item -= 1; break;
					case PLATFORM_BUTTON_DOWN: selected_item += 1; break;
					case PLATFORM_BUTTON_A: ka = 1; break;
					case PLATFORM_BUTTON_B: kb = 1; break;
					case PLATFORM_BUTTON_SELECT: loadloopdone = 1; break;
				}
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:	loadloopdone = 1; break;
				 	case SDLK_UP:		selected_item -= 1; break;
					case SDLK_DOWN:		selected_item += 1; break;
					case SDLK_a:		ka = 1; break;
					case SDLK_b:		kb = 1; break;
					default: break;
				}
			}
		}
		if (!need_redraw) { SDL_Delay(20); continue; }
		if (selected_item < 0) { selected_item = 0; }
		if (selected_item >= num_of_files) { selected_item = num_of_files-1; }

		loadloopdone = (loadloopdone || kb || ka);

		size_t l;
		const char *dir = 0;
		char* dest, buf[512];
		void (*action)(const char*);
		if (ka || kb) {
			switch(parameter) {
				case dz_floppy:
					dir = "disks";
					dest = changed_prefs.floppyslots[ka ? 1 : 0].df;
					l = sizeof(changed_prefs.floppyslots[ka ? 1 : 0].df);
					action = 0;
					break;
				case dz_rom:
					dir = "roms";
					dest = changed_prefs.romfile;
					l = sizeof(changed_prefs.romfile);
					action = 0;
					break;
				case dz_saves:
					dir = "saves";
					dest = buf;
					l = sizeof(buf);
					action = ka ? dirz_restore_savestate : dirz_save_savestate;
					break;
				default:
					loadloopdone = 0;
			}
			ka = kb = 0;
		}

		if(dir) {
			snprintf(dest, l, "%s/%s/%s", launchDir, dir, filez[selected_item]);
			if(action) action(dest);
		}

		if (selected_item > (bas + paging -1)) { bas += 1; }
		if (selected_item < bas) { bas -= 1; }
		if ((bas+paging) > num_of_files) { bas = (num_of_files - paging); }

	// background
		SDL_BlitSurface (pMenu_Surface,NULL,tmpSDLScreen,NULL);

	// texts
		int sira = 0;
		for (q=bas; q < (bas + paging); q++) {
#define RGB(x,u,v,w) x.r = u, x.g = v, x.b = w
			if (selected_item == q) {
				RGB(text_color, 255, 100, 100);
			} else {
				char buf[512];
				snprintf(buf, sizeof buf, "%s/saves/%s", launchDir, filez[q]);
				if(parameter == dz_saves && isempty(buf)) {
					RGB(text_color, 200, 200, 200);
				} else {
					RGB(text_color, 0, 0, 0);
				}
			}
			write_text (20, 50 + (sira * 20),filez[q]); //
			RGB(text_color, 0,0,0);
			sira++;
		}

		write_text (TITLE_X, TITLE_Y, msg);
		write_text (STATUS_X, STATUS_Y, msg_status);

		SDL_BlitSurface (tmpSDLScreen, NULL, display, NULL);
#ifdef USE_GL
		flush_gl_buffer (&glbuffer, 0, display->h - 1);
		render_gl_buffer (&glbuffer, 0, display->h - 1);
		glFlush ();
		SDL_GL_SwapBuffers ();
#else
		SDL_Flip (display);
#endif
		SDL_Delay(20);
		need_redraw = 0;
	}
	for(i = 0; i < num_of_files; i++) free(filez[i]);
	free(filez);
	menu_restore_surface();
	return 0;
}
