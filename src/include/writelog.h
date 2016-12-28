/*
 * E-UAE - The portable Amiga Emulator
 *
 * Logging functions
 *
 * (c) 2003-2007 Richard Drummond
 *
 * Based on code from UAE.
 * (c) 1995-2002 Bernd Schmidt
 */

#ifndef WRITELOG_H
#define WRITELOG_H

#if __GNUC__ - 1 > 1 || __GNUC_MINOR__ - 1 > 6
# define PRINTF_FORMAT __attribute__ ((format (printf, 1, 2)));
#else
# define PRINTF_FORMAT
#endif

#if defined(__cplusplus)
extern "C" {
#endif
extern void write_log   (const char *, ...) PRINTF_FORMAT;

//JIT debug logging is available only if JIT_DEBUG define was set
#ifdef JIT_DEBUG
  extern void write_jit_log   (const char *, ...) PRINTF_FORMAT;
#else
# define write_jit_log(x, ...) do {} while (0)
#endif
extern void flush_log   (void);
extern void set_logfile (const char *logfile_name);
#if defined(__cplusplus)
}
#endif

#endif /* WRITELOG_H */
