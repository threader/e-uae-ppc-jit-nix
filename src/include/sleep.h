 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Sleeping for *nix systems
  *
  * Copyright 2003 Richard Drummond
  */

#ifdef __BEOS__
#include <be/kernel/OS.h>
#endif 

/*
 * Locate an appropriate sleep routine for POSIX-like systems.
 * The Win32 port does things differently and doesn't need this.
 */
#ifdef __BEOS__
# define my_usleep(usecs) snooze(usecs)
#else
# if !defined _WIN32
#  ifdef HAVE_NANOSLEEP
#   define my_usleep(usecs) \
           { \
	       if (usecs<1000000) { \
	           struct timespec t = { 0, (usecs)*1000 }; \
                   nanosleep (&t, 0); \
	       } else { \
                   int secs   = usecs/1000000; \
		   int musecs = usecs%1000000; \
	           struct timespec t = { secs, musecs*1000 }; \
		   nanosleep (&t, 0); \
               } \
	   }
#  else
#   ifdef HAVE_USLEEP
#    define my_usleep(usecs) usleep (usecs)
#   else
#    ifdef USE_SDL
#     define my_usleep(usecs) SDL_Delay ((usecs)/1000);
#    endif
#   endif
#  endif
# endif
#endif

void sleep_millis (int ms);
void sleep_millis_busy (int ms);

void sleep_test (void);
