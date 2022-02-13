 /*
  * UAE - The Un*x Amiga Emulator
  *
  * disk support
  *
  * (c) 1995 Bernd Schmidt
  */

typedef enum { DRV_35_DD, DRV_35_HD, DRV_525_SD } drive_type;

extern void DISK_init (void);
extern void DISK_free (void);
extern void DISK_select (uae_u8 data);
extern uae_u8 DISK_status (void);
extern void disk_eject (int num);
extern int disk_empty (int num);
extern void disk_insert (int num, const char *name);
extern void DISK_check_change (void);
extern struct zfile *DISK_validate_filename (const char *, int, int *);
extern void DISK_handler (void);
extern void DISK_update (int hpos);
extern void DISK_reset (void);
extern int disk_getwriteprotect (const char *name);
extern int disk_setwriteprotect (int num, const char *name, int protect);
extern void disk_creatediskfile (char *name, int type, drive_type adftype);
extern void dumpdisk (void);

extern void DSKLEN (uae_u16 v, int hpos);
extern uae_u16 DSKBYTR (int hpos);
extern void DSKDAT (uae_u16);
extern void DSKSYNC (int, uae_u16);
extern void DSKPTL (uae_u16);
extern void DSKPTH (uae_u16);

