 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Target specific stuff, *nix version
  *
  * Copyright 1997 Bernd Schmidt
  */

#define TARGET_NAME "unix"

#define TARGET_ROM_PATH         "~/"
#define TARGET_FLOPPY_PATH      "~/"
#define TARGET_HARDFILE_PATH    "~/"

#ifndef OPTIONSFILENAME
# ifdef __APPLE__
#  define OPTIONSFILENAME "default.uaerc"
# else
#  define OPTIONSFILENAME ".uaerc"
# endif
#endif
#define OPTIONS_IN_HOME

#define DEFPRTNAME "lpr"
#define DEFSERNAME "/dev/ttyS1"

#define write_log write_log_standard
#define flush_log flush_log_standard
