/*
 * UAE - The Un*x Amiga Emulator
 *
 * A2065 ZorroII Ethernet Card
 *
 * Copyright 2009 Toni Wilen
 *
 */

#ifdef A2065

extern void a2065_init (void);
extern void a2065_free (void);
extern void a2065_reset (void);
extern void a2065_hsync_handler (void);

extern void rethink_a2065 (void);

#endif

