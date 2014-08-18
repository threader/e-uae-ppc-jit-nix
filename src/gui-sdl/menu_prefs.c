/*
 * PUAE - The Un*x Amiga Emulator
 *
 * Interface to the SDL GUI
 * (initially was for GP2X)
 *
 * Copyright 2006 Mustafa TUFAN
 *
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "custom.h"
#include "options.h"
#include "menu.h"
#include <SDL/SDL.h>
#include "button_mappings.h"
#include <stdlib.h>

int prefz (int parameter);

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
extern char msg[50];
extern char msg_status[50];

#define TITLE_X 52
#define TITLE_Y 9
#define STATUS_X 30
#define STATUS_Y 460

enum sound_settings {
	SS_OFF = 0,
	SS_OFF_EMUL,
	SS_ON,
	SS_PERFECT,
};

enum sound_settings prefz_get_sound_settings(void) {
	return changed_prefs.produce_sound;
}

void prefz_set_sound_settings(enum sound_settings s) {
	changed_prefs.produce_sound = s;
	changed_prefs.sound_stereo = changed_prefs.sound_mixed_stereo_delay = 0;
	changed_prefs.sound_filter = FILTER_SOUND_OFF;
	changed_prefs.sound_auto = 0;
	if(!s) return;
	if(s == SS_OFF_EMUL) changed_prefs.sound_filter = FILTER_SOUND_EMUL;
	else {
		changed_prefs.sound_filter = FILTER_SOUND_ON;
		if(s == SS_PERFECT) changed_prefs.sound_stereo = 1;
	}
}

#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
static const signed char masks[] = {0, CSMASK_ECS_AGNUS, CSMASK_ECS_DENISE, CSMASK_AGA};
int prefz_get_chipset(void) {
	int i;
	for(i = ARRAYSIZE(masks) -1; i >= 0; i--) if(changed_prefs.chipset_mask & masks[i]) return i;
	return 0;
}
void prefz_set_chipset(int val) {
	changed_prefs.chipset_mask = 0;
	int i = 0;
	for(; i < ARRAYSIZE(masks) && i <= val; i++) changed_prefs.chipset_mask |= masks[i];
}
/* return index into p_cpu array */
static int prefz_get_cpumodel(void) {
	//static const char* p_cpu[]      = {"68000", "68010", "68020", "68020/68881", "68ec020", "68ec020/68881"};
	switch(changed_prefs.cpu_model) {
		case 68000: return 0;
		case 68010: return 1;
		case 68020:
			if(changed_prefs.fpu_model == 68881 || changed_prefs.fpu_model == 68882) {
				if(changed_prefs.address_space_24) return 5;
				else return 3;
			}
			if(changed_prefs.address_space_24) return 4;
			return 2;
		default: return -1;
	}
}

static void prefz_set_cpumodel(int value) {
	switch (value) {
		case 0:
		case 1:
		case 2:	changed_prefs.cpu_model= 68000 + value*10;
			changed_prefs.fpu_model = 0;
			changed_prefs.address_space_24 = 0;
			break;
		case 3:	changed_prefs.cpu_model=68020;
			changed_prefs.address_space_24 = 0;
			changed_prefs.fpu_model = 68881;
			break;
		case 4:	changed_prefs.cpu_model=68020;
			changed_prefs.fpu_model = 0;
			changed_prefs.address_space_24 = 1;
			break;
		case 5:	changed_prefs.cpu_model=68020;
			changed_prefs.fpu_model = 68881;
			changed_prefs.address_space_24 = 1; break;
		default: break;
	}
}

static SDL_Surface* pPrefzMenu_Surface;
int prefz (int parameter) {
	SDL_Event event;

	if (!pPrefzMenu_Surface) pPrefzMenu_Surface = SDL_LoadBMP("guidep/images/menu_tweak.bmp");
	if (!pPrefzMenu_Surface) {
		write_log ("SDLUI: Failed to load menu image\n");
		abort();
	}
	menu_load_surface(pPrefzMenu_Surface);
	int prefsloopdone = 0;
	int kup = 0;
	int kdown = 0;
	int kleft = 0;
	int kright = 0;
	int selected_item = 0;
	int deger;
	int q;
	int w;
	enum pref_items {
		PI_CPU = 0,
		PI_CPUSPEED,
		PI_CHIPSET,
		PI_CHIPMEM,
		PI_FASTMEM,
		PI_BOGOMEM,
		PI_SOUND,
		PI_FRAMESKIP,
		PI_FLOPPYSPEED,
		PI_MAX
	};

	static const char* prefs[] = { "CPU",  "CPU Speed", "Chipset",
			        "Chip", "Fast", "Bogo",
			        "Sound","Frame Skip", "Floppy Speed" };

	static const char* p_cpu[]	= {"68000", "68010", "68020", "68020/68881", "68ec020", "68ec020/68881"};	//5
	static const char* p_speed[]	= {"max","real"};								//20
	static const char* p_chipset[]	= {"OCS", "ECS (Agnus)", "Full ECS", "AGA"};				//4
	static const char* p_sound[]	= {
		[SS_OFF] = "Off", [SS_OFF_EMUL] ="Off (emulated)",
		[SS_ON] = "On", [SS_PERFECT] = "On (perfect)"
	};
	static const char* p_frame[]	= {"0","1","2","3"};								//3
	static const char* p_ram[]	= {"0","512","1024", "2048"};								//2
	static const char* p_floppy[]  = {"0","100","200","400","800"};							//3
	static const char** prefs_map[] = {
			[PI_CPU] = p_cpu, [PI_CPUSPEED] = p_speed,
			[PI_CHIPSET] = p_chipset, [PI_CHIPMEM] = p_ram,
			[PI_FASTMEM] = p_ram, [PI_BOGOMEM] = p_ram,
			[PI_SOUND] = p_sound, [PI_FRAMESKIP] = p_frame,
			[PI_FLOPPYSPEED] = p_floppy };
	signed char defaults[PI_MAX]= {0};
#define S(X)  ARRAYSIZE(X)-1
	static const unsigned char defaults_max[PI_MAX] = {
		[PI_CPU] = S(p_cpu),  [PI_CPUSPEED] = S(p_speed),
		[PI_CHIPSET] = S(p_chipset), [PI_CHIPMEM] = S(p_ram),
		[PI_FASTMEM] = S(p_ram), [PI_BOGOMEM] = S(p_ram),
		[PI_SOUND] = S(p_sound), [PI_FRAMESKIP] = S(p_frame),
		[PI_FLOPPYSPEED] = S(p_floppy)};
#undef S
	char tmp[32];

#define DEF(IDX, ARR, VAL) do { snprintf(tmp, sizeof tmp, "%d", (int) (VAL)); \
			        int foo = 0; for(;foo<ARRAYSIZE(ARR);foo++) \
				if(!strcmp(tmp, ARR[foo])) { defaults[IDX] = foo; break; } \
				if(foo == ARRAYSIZE(ARR)) defaults[IDX] = 0; } while(0)
/* "" */

	int need_redraw = 1;
	while (!prefsloopdone) {
		while (SDL_PollEvent(&event)) {
			need_redraw = 1;
			if (event.type == SDL_QUIT) {
				prefsloopdone = 1;
			}
			if (event.type == SDL_JOYBUTTONDOWN) {
				switch (event.jbutton.button) {
					case PLATFORM_BUTTON_UP: selected_item--; break;
					case PLATFORM_BUTTON_DOWN: selected_item++; break;
					case PLATFORM_BUTTON_LEFT: kleft = 1; break;
					case PLATFORM_BUTTON_RIGHT: kright = 1; break;
					case PLATFORM_BUTTON_SELECT: prefsloopdone = 1; break;
					case PLATFORM_BUTTON_B: prefsloopdone = 1; break;
				}
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:	prefsloopdone = 1; break;
				 	case SDLK_UP:		selected_item--; break;
					case SDLK_DOWN:		selected_item++; break;
					case SDLK_LEFT:		kleft = 1; break;
					case SDLK_RIGHT:	kright = 1; break;
					case SDLK_b:		prefsloopdone = 1; break;
					default: break;
				}
			}
		}
		if(!need_redraw) { SDL_Delay(20); continue; }
		defaults[PI_SOUND] = prefz_get_sound_settings();
		defaults[PI_CHIPSET] = prefz_get_chipset();
		defaults[PI_CPUSPEED] = changed_prefs.m68k_speed + 1;
		defaults[PI_CPU] = prefz_get_cpumodel();
		//DEF(PI_CPUSPEED, p_speed, currprefs.m68k_speed + 1);
		//DEF(PI_CPU, p_cpu, /*currprefs.m68k_speed*/0);
		DEF(PI_CHIPMEM, p_ram, changed_prefs.chipmem_size / 1024);
		DEF(PI_FASTMEM, p_ram, changed_prefs.fastmem_size / 1024);
		DEF(PI_BOGOMEM, p_ram, changed_prefs.bogomem_size / 1024);
		DEF(PI_FRAMESKIP, p_frame, changed_prefs.gfx_framerate);
		DEF(PI_FLOPPYSPEED, p_floppy, changed_prefs.floppy_speed);


		int i, dir = kleft ? -1 : kright ? 1 : 0;
		defaults[selected_item] += dir;
		for(i = 0; i < PI_MAX; i++)
			if(defaults[i] < 0) defaults[i] = defaults_max[i];
			else if(defaults[i] > defaults_max[i]) defaults[i] = 0;

		if (selected_item < 0) selected_item = PI_MAX - 1;
		else if (selected_item >= PI_MAX) selected_item = 0;

		uae_u32* destmem;
		if(kleft || kright) {
			kleft = kright = 0;
			switch(selected_item) {
				case PI_FLOPPYSPEED:
					changed_prefs.floppy_speed = atoi(p_floppy[defaults[selected_item]]);
					break;
				case PI_CPU:
					prefz_set_cpumodel(defaults[selected_item]);
					break;
				case PI_CPUSPEED:
					/* m68k_speed: -1 : max, 0: real , >0 : "finegrain_cpu_speed" */
					changed_prefs.m68k_speed = defaults[PI_CPUSPEED] - 1;
					break;
				case PI_CHIPSET:
					prefz_set_chipset(defaults[PI_CHIPSET]);
					break;
				case PI_CHIPMEM:
					destmem = &changed_prefs.chipmem_size;
					goto set_mem;
				case PI_FASTMEM:
					destmem = &changed_prefs.fastmem_size;
					goto set_mem;
				case PI_BOGOMEM:
					destmem = &changed_prefs.bogomem_size;
					set_mem:
					*destmem = atoi(prefs_map[selected_item][defaults[selected_item]]) * 1024;
					break;
				case PI_SOUND:
					prefz_set_sound_settings(defaults[PI_SOUND]);
				case PI_FRAMESKIP:
				default:
					break;
			}
		}

	// background
		SDL_BlitSurface (pMenu_Surface, NULL, tmpSDLScreen, NULL);

#define OPTIONS_Y 200
	// texts
		int sira = 0;
		int pos = 0;
		for (q=0; q<9; q++) {
			if (selected_item == q) {
				text_color.r = 150;
				text_color.g = 50;
				text_color.b = 50;
			}

			pos = 50 + (sira * 20);
			write_text (20, pos, prefs[q]); //

			write_text (OPTIONS_Y, pos, prefs_map[q][defaults[q]]);

			text_color.r = 0;
			text_color.g = 0;
			text_color.b = 0;
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
		SDL_Delay(20); /* sleep a bit so the cpu is not on 100% all the time */
		need_redraw = 0;
	} //while done
/*
	if (defaults[0] == 4) { }
	if (defaults[0] == 5) { }
	defaults[1]--;
*/
	menu_restore_surface();
	return 0;
}
