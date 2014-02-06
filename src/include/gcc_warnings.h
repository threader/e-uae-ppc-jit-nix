#ifndef GCC_WARNINGS_H
#define GCC_WARNINGS_H

/* The following pragma chain can be used to temporarily silence gcc warnings.
 * It has been written by Jonathan Wakely and modified by Patrick Horgan.
 * See http://dbp-consulting.com/tutorials/SuppressingGCCWarnings.html for
 * further information on this.
*/

#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#define GCC_DIAG_STR(s) #s
#define GCC_DIAG_JOINSTR(x,y) GCC_DIAG_STR(x ## y)
# define GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
# define GCC_DIAG_PRAGMA(x) GCC_DIAG_DO_PRAGMA(GCC diagnostic x)
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#  define GCC_DIAG_OFF(x) GCC_DIAG_PRAGMA(push) \
	GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
#  define GCC_DIAG_ON(x) GCC_DIAG_PRAGMA(pop)
# else
#  define GCC_DIAG_OFF(x) GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,x))
#  define GCC_DIAG_ON(x)  GCC_DIAG_PRAGMA(warning GCC_DIAG_JOINSTR(-W,x))
# endif // 4.06
#else
# define GCC_DIAG_OFF(x)
# define GCC_DIAG_ON(x)
#endif // 4.02

#endif // GCC_WARNINGS_H
