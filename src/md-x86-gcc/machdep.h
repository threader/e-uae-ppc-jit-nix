/*
 * E-UAE - The portable Amiga Emulator
 *
 * Processor-specific definitions
 *
 * Copyright 2005 Richard Drummond
 */

#ifndef MACHDEP_MACHDEP_H
#define MACHDEP_MACHDEP_H

#define MACHDEP_X86
#ifdef __x86_64__
#define MACHDEP_NAME    "amd64"
#else
#define MACHDEP_NAME    "x86"
#endif

#ifdef __native_client__
#undef HAVE_MACHDEP_TIMER
#else
#define HAVE_MACHDEP_TIMER
#endif

typedef uae_s64 frame_time_t;
#define MAX_FRAME_TIME 9223372036854775807LL

#endif
