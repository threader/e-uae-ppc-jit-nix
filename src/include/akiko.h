

#define AKIKO_BASE 0xb80000
#define AKIKO_BASE_END 0xb80100 /* ?? */

extern void akiko_reset (void);
extern int akiko_init (void);
extern void akiko_free (void);
extern int cd32_enabled;

extern void akiko_entergui (void);
extern void akiko_exitgui (void);
extern void AKIKO_hsync_handler (void);
