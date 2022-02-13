 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Save/restore emulator state
  *
  * (c) 1999-2001 Toni Wilen
  */


/* functions to save byte,word or long word
 * independent of CPU's endianess */

extern void save_u16_func (uae_u8 **, uae_u16);
extern void save_u32_func (uae_u8 **, uae_u32);
extern void save_u8_func (uae_u8 **, uae_u8);

extern uae_u16 restore_u16_func (uae_u8 **);
extern uae_u32 restore_u32_func (uae_u8 **);
extern uae_u8 restore_u8_func (uae_u8 **);

extern void save_string_func (uae_u8 **, char*);
extern char *restore_string_func (uae_u8 **);

#define save_u16(x) save_u16_func (&dst, (x))
#define save_u32(x) save_u32_func (&dst, (x))
#define save_u8(x) save_u8_func (&dst, (x))

#define restore_u16() restore_u16_func (&src)
#define restore_u32() restore_u32_func (&src)
#define restore_u8() restore_u8_func (&src)

#define save_string(x) save_string_func (&dst, (x))
#define restore_string() restore_string_func (&src)

/* save, restore and initialize routines for Amiga's subsystems */

extern uae_u8 *restore_cpu (uae_u8 *);
extern uae_u8 *save_cpu (int *, uae_u8 *);

extern uae_u8 *restore_fpu (uae_u8 *);
extern uae_u8 *save_fpu (int *, uae_u8 *);

extern uae_u8 *restore_disk (int, uae_u8 *);
extern uae_u8 *save_disk (int, int *, uae_u8 *);
extern uae_u8 *restore_floppy (uae_u8 *src);
extern uae_u8 *save_floppy (int *len, uae_u8 *);
extern void DISK_save_custom  (uae_u32 *pdskpt, uae_u16 *pdsklen, uae_u16 *pdsksync, uae_u16 *pdskbytr);
extern void DISK_restore_custom  (uae_u32 pdskpt, uae_u16 pdsklength, uae_u16 pdskbytr);

extern uae_u8 *restore_custom (uae_u8 *);
extern uae_u8 *save_custom (int *, uae_u8 *, int);

extern uae_u8 *restore_custom_sprite (int num, uae_u8 *src);
extern uae_u8 *save_custom_sprite (int num, int *len, uae_u8 *);

extern uae_u8 *restore_custom_agacolors (uae_u8 *src);
extern uae_u8 *save_custom_agacolors (int *len, uae_u8 *);

extern uae_u8 *restore_blitter (uae_u8 *src);
extern uae_u8 *save_blitter (int *len, uae_u8 *);

extern uae_u8 *restore_audio (int, uae_u8 *);
extern uae_u8 *save_audio (int, int *, uae_u8 *);

extern uae_u8 *restore_cia (int, uae_u8 *);
extern uae_u8 *save_cia (int, int *, uae_u8 *);

extern uae_u8 *restore_expansion (uae_u8 *);
extern uae_u8 *save_expansion (int *, uae_u8 *);

extern uae_u8 *restore_p96 (uae_u8 *);
extern uae_u8 *save_p96 (int *, uae_u8 *);

extern void restore_cram (int, long);
extern void restore_bram (int, long);
extern void restore_fram (int, long);
extern void restore_zram (int, long);
extern void restore_pram (int, long);
extern void restore_ram (long, uae_u8*);

extern uae_u8 *save_cram (int *);
extern uae_u8 *save_bram (int *);
extern uae_u8 *save_fram (int *);
extern uae_u8 *save_zram (int *);
extern uae_u8 *save_pram (int *);

extern uae_u8 *restore_rom (uae_u8 *);
extern uae_u8 *save_rom (int, int *, uae_u8 *);

extern uae_u8 *restore_action_replay (uae_u8 *);
extern uae_u8 *save_action_replay (int *, uae_u8 *);

extern void savestate_initsave (char *filename, int docompress);
extern void save_state (char *filename, char *description);
extern void restore_state (char *filename);
extern void savestate_restore_finish (void);

extern void custom_save_state (void);
extern void custom_prepare_savestate (void);

#define STATE_SAVE 1
#define STATE_RESTORE 2
#define STATE_DOSAVE 4
#define STATE_DORESTORE 8
#define STATE_REWIND 16
#define STATE_DOREWIND 32

extern int savestate_state;
extern char savestate_fname[MAX_PATH];
extern struct zfile *savestate_file;

extern uae_u32 CRC32(uae_u32 crc, const uae_u8 *buf, int len);

extern void savestate_quick (int slot, int save);

extern void savestate_capture (int);
extern void savestate_free (void);
extern void savestate_init (void);
extern void savestate_rewind (void);
extern int savestate_dorewind (int);
extern void savestate_listrewind (void);

