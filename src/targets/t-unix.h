 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Target specific stuff, *nix version
  *
  * Copyright 1997 Bernd Schmidt
  */

#define TARGET_NAME "unix"

#ifndef OPTIONSFILENAME
#define OPTIONSFILENAME ".uaerc"
#endif
#define OPTIONS_IN_HOME

#define DEFPRTNAME "lpr"
#define DEFSERNAME "/dev/ttyS1"

#define write_log write_log_standard
