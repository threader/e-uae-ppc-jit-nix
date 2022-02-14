 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Floppy disk emulation
  *
  * Copyright 1995 Hannu Rummukainen
  * Copyright 1995-2001 Bernd Schmidt
  * Copyright 2000-2003 Toni Wilen
  *
  * High Density Drive Handling by Dr. Adil Temel (C) 2001 [atemel1@hotmail.com]
  *
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "options.h"
#include "threaddep/thread.h"
#include "memory.h"
#include "events.h"
#include "custom.h"
#include "ersatz.h"
#include "disk.h"
#include "gui.h"
#include "zfile.h"
#include "autoconf.h"
#include "newcpu.h"
#include "xwin.h"
#include "osemu.h"
#include "execlib.h"
#include "savestate.h"
#include "fdi2raw.h"
#include "catweasel.h"
#ifdef CAPS
#include "caps/caps_win32.h"
#endif

/* writable track length with normal 2us bitcell/300RPM motor (PAL) */
#define FLOPPY_WRITE_LEN (currprefs.ntscmode ? (12798 / 2) : (12668 / 2)) /* 12667 PAL, 12797 NTSC */
/* This works out to 350 */
#define FLOPPY_GAP_LEN (FLOPPY_WRITE_LEN - 11 * 544)
/* (cycles/bitcell) << 8, normal = ((2us/280ns)<<8) = ~1830 */
#define NORMAL_FLOPPY_SPEED (currprefs.ntscmode ? 1810 : 1829)

#define DISK_DEBUG
#define DISK_DEBUG2
#undef DISK_DEBUG
#undef DISK_DEBUG2
#undef DEBUG_DRIVE_ID

/* UAE-1ADF (ADF_EXT2)
 * W	reserved
 * W	number of tracks (default 2*80=160)
 *
 * W	reserved
 * W	type, 0=normal AmigaDOS track, 1 = raw MFM
 * L	available space for track in bytes (must be even)
 * L	track length in bits
 */

static int side, direction, writing;
static uae_u8 selected = 15, disabled;

static uae_u8 *writebuffer[544 * 22];

#define DISK_INDEXSYNC 1
#define DISK_WORDSYNC 2

/* A value of 5 works out to a guaranteed delay of 1/2 of a second
 Higher values are dangerous, e.g. a value of 8 breaks the RSI
 demo.  */
#define DSKREADY_TIME 5

#if 0
#define MAX_DISK_WORDS_PER_LINE 50 /* depends on floppy_speed */
static uae_u32 dma_tab[MAX_DISK_WORDS_PER_LINE + 1];
#endif
static int dskdmaen, dsklength, dsklen;
static uae_u16 dsksync, dskbytr_val;
static uae_u32 dskpt;
static int dma_enable, bitoffset, syncoffset;
static uae_u16 word;
/* Always carried through to the next line.  */
static int disk_hpos;

typedef enum { TRACK_AMIGADOS, TRACK_RAW, TRACK_RAW1 } image_tracktype;
typedef struct {
    uae_u16 len;
    uae_u32 offs;
    int bitlen, track;
    unsigned int sync;
    image_tracktype type;
} trackid;

#define MAX_TRACKS 328

/* We have three kinds of Amiga floppy drives
 * - internal A500/A2000 drive:
 *   ID is always DRIVE_ID_NONE (S.T.A.G expects this)
 * - HD drive (A3000/A4000):
 *   ID is DRIVE_ID_35DD if DD floppy is inserted or drive is empty
 *   ID is DRIVE_ID_35HD if HD floppy is inserted
 * - regular external drive:
 *   ID is always DRIVE_ID_35DD
 */

#define DRIVE_ID_NONE  0x00000000
#define DRIVE_ID_35DD  0xFFFFFFFF
#define DRIVE_ID_35HD  0xAAAAAAAA
#define DRIVE_ID_525SD 0x55555555 /* 40 track 5.25 drive , kickstart does not recognize this */

#define MAX_REVOLUTIONS 5

typedef enum { ADF_NORMAL, ADF_EXT1, ADF_EXT2, ADF_FDI, ADF_IPF, ADF_CATWEASEL } drive_filetype;
typedef struct {
    struct zfile *diskfile;
    struct zfile *writediskfile;
    drive_filetype filetype;
    trackid trackdata[MAX_TRACKS];
    trackid writetrackdata[MAX_TRACKS];
    int buffered_cyl, buffered_side;
    int cyl;
    int motoroff;
    int state;
    int wrprot;
    uae_u16 bigmfmbuf[MAX_REVOLUTIONS * 0x8000];
    int revolutions;
    int current_revolution;
    uae_u16 *trackpointers[MAX_REVOLUTIONS];
    uae_u16 *tracktiming;
    int tracklengths[MAX_REVOLUTIONS];
    int mfmpos;
    int tracklen;
    int prevtracklen;
    int trackspeed;
    int dmalen;
    int num_tracks, write_num_tracks, num_secs;
    int hard_num_cyls;
    int dskchange;
    int dskchange_time;
    int dskready;
    int dskready_time;
    int steplimit;
    int ddhd; /* 1=DD 2=HD */
    int drive_id_scnt; /* drive id shift counter */
    int idbit;
    unsigned long drive_id; /* drive id to be reported */
    char newname[256]; /* storage space for new filename during eject delay */
    FDI *fdi;
    int useturbo;
    int floppybitcounter; /* number of bits left */
#ifdef CATWEASEL
    catweasel_drive *catweasel;
#else
    int catweasel;
#endif
} drive;

static uae_u16 bigmfmbufw[0x8000];
static drive floppy[4];

static uae_u8 exeheader[]={0x00,0x00,0x03,0xf3,0x00,0x00,0x00,0x00};
static uae_u8 bootblock[]={
    0x44,0x4f,0x53,0x00,0xc0,0x20,0x0f,0x19,0x00,0x00,0x03,0x70,0x43,0xfa,0x00,0x18,
    0x4e,0xae,0xff,0xa0,0x4a,0x80,0x67,0x0a,0x20,0x40,0x20,0x68,0x00,0x16,0x70,0x00,
    0x4e,0x75,0x70,0xff,0x60,0xfa,0x64,0x6f,0x73,0x2e,0x6c,0x69,0x62,0x72,0x61,0x72,
    0x79
};

#define FS_OFS_DATABLOCKSIZE 488
#define FS_FLOPPY_BLOCKSIZE 512
#define FS_EXTENSION_BLOCKS 72
#define FS_FLOPPY_TOTALBLOCKS 1760
#define FS_FLOPPY_RESERVED 2

static void writeimageblock (struct zfile *dst, uae_u8 *sector, int offset)
{
    zfile_fseek (dst, offset, SEEK_SET);
    zfile_fwrite (sector, FS_FLOPPY_BLOCKSIZE, 1, dst);
}

static void disk_checksum(uae_u8 *p, uae_u8 *c)
{
    uae_u32 cs = 0;
    int i;
    for (i = 0; i < FS_FLOPPY_BLOCKSIZE; i+= 4) cs += (p[i] << 24) | (p[i+1] << 16) | (p[i+2] << 8) | (p[i+3] << 0);
    cs = -cs;
    c[0] = cs >> 24; c[1] = cs >> 16; c[2] = cs >> 8; c[3] = cs >> 0;
}

static int dirhash (unsigned char *name)
{
    unsigned long hash;
    int i;

    hash = strlen (name);
    for(i = 0; i < strlen (name); i++) {
	hash = hash * 13;
	hash = hash + toupper (name[i]);
	hash = hash & 0x7ff;
    }
    hash = hash % ((FS_FLOPPY_BLOCKSIZE / 4) - 56);
    return hash;
}

static void disk_date (uae_u8 *p)
{
    time_t t;
    struct tm *today;
    int year, days, minutes, ticks;
    char tmp[10];
    time (&t);
    today = localtime( &t );
    strftime (tmp, sizeof(tmp), "%Y", today);
    year = atol (tmp);
    strftime (tmp, sizeof(tmp), "%j", today);
    days = atol (tmp) - 1;
    strftime (tmp, sizeof(tmp), "%H", today);
    minutes = atol (tmp) * 60;
    strftime (tmp, sizeof(tmp), "%M", today);
    minutes += atol (tmp);
    strftime (tmp, sizeof(tmp), "%S", today);
    ticks = atol (tmp) * 50;
    while (year > 1978) {
	if ( !(year % 100) ? !(year % 400) : !(year % 4) ) days++;
	days += 365;
	year--;
    }
    p[0] = days >> 24; p[1] = days >> 16; p[2] = days >> 8; p[3] = days >> 0;
    p[4] = minutes >> 24; p[5] = minutes >> 16; p[6] = minutes >> 8; p[7] = minutes >> 0;
    p[8] = ticks >> 24; p[9] = ticks >> 16; p[10] = ticks >> 8; p[11] = ticks >> 0; 
}

static void createbootblock (uae_u8 *sector, int bootable)
{
    memset (sector, 0, FS_FLOPPY_BLOCKSIZE);
    memcpy (sector, "DOS", 3);
    if (bootable)
	memcpy (sector, bootblock, sizeof(bootblock));
}

static void createrootblock (uae_u8 *sector, uae_u8 *disk_name)
{
    memset (sector, 0, FS_FLOPPY_BLOCKSIZE);
    sector[0+3] = 2;
    sector[12+3] = 0x48;
    sector[312] = sector[313] = sector[314] = sector[315] = (uae_u8)0xff;
    sector[316+2] = 881 >> 8; sector[316+3] = 881 & 255;
    sector[432] = strlen (disk_name);
    strcpy (sector + 433, disk_name);
    sector[508 + 3] = 1;
    disk_date (sector + 420);
    memcpy (sector + 472, sector + 420, 3 * 4);
    memcpy (sector + 484, sector + 420, 3 * 4);
}

static int getblock (uae_u8 *bitmap)
{
    int i = 0;
    while (bitmap[i] != 0xff) {
	if (bitmap[i] == 0) {
	    bitmap[i] = 1;
	    return i;
	}
	i++;
    }
    return -1;
}

static void pl (uae_u8 *sector, int offset, uae_u32 v)
{
    sector[offset + 0] = v >> 24;
    sector[offset + 1] = v >> 16;
    sector[offset + 2] = v >> 8;
    sector[offset + 3] = v >> 0;
}

static int createdirheaderblock (uae_u8 *sector, int parent, char *filename, uae_u8 *bitmap)
{
    int block = getblock (bitmap);

    memset (sector, 0, FS_FLOPPY_BLOCKSIZE);
    pl (sector, 0, 2);
    pl (sector, 4, block);
    disk_date (sector + 512 - 92);
    sector[512 - 80] = strlen (filename);
    strcpy (sector + 512 - 79, filename);
    pl (sector, 512 - 12, parent);
    pl (sector, 512 - 4, 2);
    return block;
}

static int createfileheaderblock (struct zfile *z,uae_u8 *sector, int parent, char *filename, struct zfile *src, uae_u8 *bitmap)
{
    uae_u8 sector2[FS_FLOPPY_BLOCKSIZE];
    uae_u8 sector3[FS_FLOPPY_BLOCKSIZE];
    int block = getblock (bitmap);
    int datablock = getblock (bitmap);
    int datasec = 1;
    int extensions;
    int extensionblock, extensioncounter, headerextension = 1;
    int size;

    zfile_fseek (src, 0, SEEK_END);
    size = zfile_ftell (src);
    zfile_fseek (src, 0, SEEK_SET);
    extensions = (size + FS_OFS_DATABLOCKSIZE - 1) / FS_OFS_DATABLOCKSIZE;

    memset (sector, 0, FS_FLOPPY_BLOCKSIZE);
    pl (sector, 0, 2);
    pl (sector, 4, block);
    pl (sector, 8, extensions > FS_EXTENSION_BLOCKS ? FS_EXTENSION_BLOCKS : extensions);
    pl (sector, 16, datablock);
    pl (sector, FS_FLOPPY_BLOCKSIZE - 188, size);
    disk_date (sector + FS_FLOPPY_BLOCKSIZE - 92);
    sector[FS_FLOPPY_BLOCKSIZE - 80] = strlen (filename);
    strcpy (sector + FS_FLOPPY_BLOCKSIZE - 79, filename);
    pl (sector, FS_FLOPPY_BLOCKSIZE - 12, parent);
    pl (sector, FS_FLOPPY_BLOCKSIZE - 4, -3);
    extensioncounter = 0;
    extensionblock = 0;

    while (size > 0) {
	int datablock2 = datablock;
	int extensionblock2 = extensionblock;
	if (extensioncounter == FS_EXTENSION_BLOCKS) {
	    extensioncounter = 0;
	    extensionblock = getblock (bitmap);
	    if (datasec > FS_EXTENSION_BLOCKS + 1) {
	        pl (sector3, 8, FS_EXTENSION_BLOCKS);
	        pl (sector3, FS_FLOPPY_BLOCKSIZE - 8, extensionblock);
	        pl (sector3, 4, extensionblock2);
	        disk_checksum(sector3, sector3 + 20);
	        writeimageblock (z, sector3, extensionblock2 * FS_FLOPPY_BLOCKSIZE);
	    } else {
	        pl (sector, 512 - 8, extensionblock);
	    }
	    memset (sector3, 0, FS_FLOPPY_BLOCKSIZE);
	    pl (sector3, 0, 16);
	    pl (sector3, FS_FLOPPY_BLOCKSIZE - 12, block);
	    pl (sector3, FS_FLOPPY_BLOCKSIZE - 4, -3);
	}
	memset (sector2, 0, FS_FLOPPY_BLOCKSIZE);
	pl (sector2, 0, 8);
	pl (sector2, 4, block);
	pl (sector2, 8, datasec++);
	pl (sector2, 12, size > FS_OFS_DATABLOCKSIZE ? FS_OFS_DATABLOCKSIZE : size);
	zfile_fread (sector2 + 24, size > FS_OFS_DATABLOCKSIZE ? FS_OFS_DATABLOCKSIZE : size, 1, src);
	size -= FS_OFS_DATABLOCKSIZE;
	datablock = 0;
	if (size > 0) datablock = getblock (bitmap);
	pl (sector2, 16, datablock);
        disk_checksum(sector2, sector2 + 20);
        writeimageblock (z, sector2, datablock2 * FS_FLOPPY_BLOCKSIZE);
	if (datasec <= FS_EXTENSION_BLOCKS + 1)
	    pl (sector, 512 - 204 - extensioncounter * 4, datablock2);
	else
	    pl (sector3, 512 - 204 - extensioncounter * 4, datablock2);
	extensioncounter++;
    }
    if (datasec > FS_EXTENSION_BLOCKS) {
	pl (sector3, 8, extensioncounter);
        disk_checksum(sector3, sector3 + 20);
        writeimageblock (z, sector3, extensionblock * FS_FLOPPY_BLOCKSIZE);
    }
    disk_checksum(sector, sector + 20);
    writeimageblock (z, sector, block * FS_FLOPPY_BLOCKSIZE);
    return block;
}

static void createbitmapblock (uae_u8 *sector, uae_u8 *bitmap)
{
    uae_u8 mask;
    int i, j;
    memset (sector, 0, FS_FLOPPY_BLOCKSIZE);
    for (i = FS_FLOPPY_RESERVED; i < FS_FLOPPY_TOTALBLOCKS; i += 8) {
	mask = 0;
	for (j = 0; j < 8; j++) {
	    if (bitmap[i + j]) mask |= 1 << j;
	}
	sector[4 + i / 8] = mask;
    }
    disk_checksum(sector, sector + 0);
}

static int createimagefromexe (struct zfile *src, struct zfile *dst)
{
    uae_u8 sector1[FS_FLOPPY_BLOCKSIZE], sector2[FS_FLOPPY_BLOCKSIZE];
    uae_u8 bitmap[FS_FLOPPY_TOTALBLOCKS + 8];
    int exesize;
    int blocksize = FS_OFS_DATABLOCKSIZE;
    int blocks, extensionblocks;
    int totalblocks;
    int fblock1, dblock1;
    char *fname1 = "runme.exe";
    char *fname2 = "startup-sequence";
    char *dirname1 = "s";
    struct zfile *ss;

    memset (bitmap, 0, sizeof (bitmap));
    zfile_fseek (src, 0, SEEK_END);
    exesize = zfile_ftell (src);
    blocks = (exesize + blocksize - 1) / blocksize;
    extensionblocks = (blocks + FS_EXTENSION_BLOCKS - 1) / FS_EXTENSION_BLOCKS;
    /* bootblock=2, root=1, bitmap=1, startup-sequence=1+1, exefileheader=1 */
    totalblocks = 2 + 1 + 1 + 2 + 1 + blocks + extensionblocks;
    if (totalblocks > FS_FLOPPY_TOTALBLOCKS)
	return 0;

    bitmap[880] = 1;
    bitmap[881] = 1;
    bitmap[0] = 1;
    bitmap[1] = 1;

    dblock1 = createdirheaderblock (sector2, 880, dirname1, bitmap);
    ss = zfile_fopen_empty (fname1, strlen(fname1));
    zfile_fwrite (fname1, strlen(fname1), 1, ss);
    fblock1 = createfileheaderblock (dst, sector1,  dblock1, fname2, ss, bitmap);
    zfile_fclose (ss);
    pl (sector2, 24 + dirhash (fname2) * 4, fblock1);
    disk_checksum(sector2, sector2 + 20);
    writeimageblock (dst, sector2, dblock1 * FS_FLOPPY_BLOCKSIZE);

    fblock1 = createfileheaderblock (dst, sector1, 880, fname1, src, bitmap);

    createrootblock (sector1, "empty");
    pl (sector1, 24 + dirhash (fname1) * 4, fblock1);
    pl (sector1, 24 + dirhash (dirname1) * 4, dblock1);
    disk_checksum(sector1, sector1 + 20);
    writeimageblock (dst, sector1, 880 * FS_FLOPPY_BLOCKSIZE);
    
    createbitmapblock (sector1, bitmap);
    writeimageblock (dst, sector1, 881 * FS_FLOPPY_BLOCKSIZE);

    createbootblock (sector1, 1);
    writeimageblock (dst, sector1, 0 * FS_FLOPPY_BLOCKSIZE);

    return 1;
}

static int get_floppy_speed (void)
{
    int m = currprefs.floppy_speed;
    if (m <= 10) m = 100;
    m = NORMAL_FLOPPY_SPEED * 100 / m;
    return m;
}

static char *drive_id_name(drive *drv)
{
    switch(drv->drive_id)
    {
    case DRIVE_ID_35HD : return "3.5HD";
    case DRIVE_ID_525SD: return "5.25SD";
    case DRIVE_ID_35DD : return "3.5DD";
    }
    return "UNKNOWN"; 
}

/* Simulate exact behaviour of an A3000T 3.5 HD disk drive. 
 * The drive reports to be a 3.5 DD drive whenever there is no
 * disk or a 3.5 DD disk is inserted. Only 3.5 HD drive id is reported
 * when a real 3.5 HD disk is inserted. -Adil
 */
static void drive_settype_id(drive *drv)
{
    int t = currprefs.dfxtype[drv - &floppy[0]];

    switch (t)
    {
	case DRV_35_HD:
	if (!drv->diskfile || drv->ddhd <= 1)
	    drv->drive_id = DRIVE_ID_35DD;
	else
	    drv->drive_id = DRIVE_ID_35HD;
	break;
	case DRV_35_DD:
	default:
        drv->drive_id = DRIVE_ID_35DD;
	break;
	case DRV_525_SD:
	drv->drive_id = DRIVE_ID_525SD;
	break;
    }
#ifdef DEBUG_DRIVE_ID
    write_log("drive_settype_id: DF%d: set to %s\n", drv-floppy, drive_id_name(drv));
#endif
}

static void drive_image_free (drive *drv)
{
    switch (drv->filetype)
    {
        case ADF_IPF:
#ifdef CAPS
        caps_unloadimage (drv - floppy);
#endif
        break;
        case ADF_FDI:
        fdi2raw_header_free (drv->fdi);
        drv->fdi = 0;
        break;
    }
    drv->filetype = -1;
    zfile_fclose (drv->diskfile);
    drv->diskfile = 0;
    zfile_fclose (drv->writediskfile);
    drv->writediskfile = 0;
}

static int drive_insert (drive * drv, int dnum, const char *fname);

static void reset_drive(int i)
{
    drive *drv = &floppy[i];
    drive_image_free (drv);
    drv->motoroff = 1;
    disabled &= ~(1 << i);
    gui_data.drive_disabled[i] = 0;
    if (currprefs.dfxtype[i] < 0) {
        disabled |= 1 << i;
        gui_data.drive_disabled[i] = 1;
    }
    drv->dskchange_time = 0;
    drv->buffered_cyl = -1;
    drv->buffered_side = -1;
    gui_led (i + 1, 0);
    drive_settype_id (drv);
    if (strlen (drv->newname) > 0)
        strcpy (currprefs.df[i], drv->newname);
    if (!drive_insert (drv, i, currprefs.df[i]))
        disk_eject (i);
}

/* code for track display */
static void update_drive_gui (int num)
{
    drive *drv = floppy + num;

    if (drv->state == gui_data.drive_motor[num]
	&& drv->cyl == gui_data.drive_track[num]
	&& side == gui_data.drive_side
	&& ((writing && gui_data.drive_writing[num])
	    || (!writing && !gui_data.drive_writing[num]))) {
	return;
    }
    gui_data.drive_motor[num] = drv->state;
    gui_data.drive_track[num] = drv->cyl;
    gui_data.drive_side = side;
    if (!gui_data.drive_writing[num])
	gui_data.drive_writing[num] = writing;
    gui_ledstate &= ~(2 << num);
    if (drv->state)
	gui_ledstate |= 2 << num;
    gui_led (num + 1, gui_data.drive_motor[num]);
}

static void drive_fill_bigbuf (drive * drv,int);

struct zfile *DISK_validate_filename (const char *fname, int leave_open, int *wrprot)
{
    struct zfile *f = zfile_fopen (fname, "r+b");
    if (f) {
	if (wrprot)
	    *wrprot = 0;
    } else {
	if (wrprot)
	    *wrprot = 1;
	f = zfile_fopen (fname, "rb");
    }
    if (!leave_open)
	zfile_fclose (f);
    return f;
}

static void updatemfmpos (drive *drv)
{
    if (drv->prevtracklen)
	drv->mfmpos = drv->mfmpos * (drv->tracklen * 1000 / drv->prevtracklen) / 1000;
    drv->mfmpos %= drv->tracklen;
    drv->prevtracklen = drv->tracklen;
}

static void track_reset (drive *drv)
{
    drv->tracklen = FLOPPY_WRITE_LEN * drv->ddhd * 2 * 8;
    drv->trackspeed = get_floppy_speed ();
    drv->revolutions = 0;
    drv->tracklengths[0] = drv->tracklen;
    drv->current_revolution = 0;
    drv->trackpointers[0] = drv->bigmfmbuf;
    drv->buffered_side = -1;
    free (drv->tracktiming);
    drv->tracktiming = 0;
    memset (drv->bigmfmbuf, 0xaa, FLOPPY_WRITE_LEN * 2 * drv->ddhd);
    updatemfmpos (drv);
}

static int read_header_ext2 (struct zfile *diskfile, trackid *trackdata, int *num_tracks, int *ddhd)
{
    uae_u8 buffer[2 + 2 + 4 + 4];
    trackid *tid;
    int offs;
    int i;

    zfile_fseek (diskfile, 0, SEEK_SET);
    zfile_fread (buffer, 1, 8, diskfile);
    if (strncmp (buffer, "UAE-1ADF", 8))
	return 0;
    zfile_fread (buffer, 1, 4, diskfile);
    *num_tracks = buffer[2] * 256 + buffer[3];
    offs = 8 + 2 + 2 + (*num_tracks) * (2 + 2 + 4 + 4);

    for (i = 0; i < (*num_tracks); i++) {
        tid = trackdata + i;
        zfile_fread (buffer, 2 + 2 + 4 + 4, 1, diskfile);
        tid->type = buffer[2] * 256 + buffer[3];
        tid->len = buffer[5] * 65536 + buffer[6] * 256 + buffer[7];
        tid->bitlen = buffer[9] * 65536 + buffer[10] * 256 + buffer[11];
        tid->offs = offs;
        if (tid->len > 20000 && ddhd)
	    *ddhd = 2;
	tid->track = i;
	offs += tid->len;
    }
    return 1;
}

#ifdef _WIN32
    extern char *start_path;
#endif

static char *getwritefilename (const char *name)
{
    static char name1[1024];
    char name2[1024];
    int i;
    
    strcpy (name2, name);
    i = strlen (name2) - 1;
    while (i > 0) {
	if (name2[i] == '.') {
	    name2[i] = 0;
	    break;
	}
	i--;
    }
    while (i > 0) {
	if (name2[i] == '/' || name2[i] == '\\') {
	    i++;
	    break;
	}
	i--;
    }
//    sprintf (name1, "%s%cSaveImages%c%s_save.adf", start_path, FSDB_DIR_SEPARATOR, FSDB_DIR_SEPARATOR, name2 + i);
    return name1;
}

static struct zfile *getwritefile (const char *name, int *wrprot)
{
    return DISK_validate_filename (getwritefilename (name), 1, wrprot);
}

static int iswritefileempty (const char *name)
{
    struct zfile *zf;
    int wrprot;
    uae_u8 buffer[8];
    trackid td[MAX_TRACKS];
    int tracks, ddhd, i, ret;

    zf = getwritefile (name, &wrprot);
    if (!zf) return 1;
    zfile_fread (buffer, sizeof (char), 8, zf);
    if (strncmp ((char *) buffer, "UAE-1ADF", 8))
	return 0;
    ret = read_header_ext2 (zf, td, &tracks, &ddhd);
    zfile_fclose (zf);
    if (!ret)
	return 1;
    for (i = 0; i < tracks; i++) {
	if (td[i].bitlen) return 0;
    }
    return 1;
}

static int openwritefile (drive *drv, int create)
{
    int wrprot = 0;

    drv->writediskfile = getwritefile (currprefs.df[drv - &floppy[0]], &wrprot);
    if (drv->writediskfile) {
        drv->wrprot = wrprot;
	if (!read_header_ext2 (drv->writediskfile, drv->writetrackdata, &drv->write_num_tracks, 0)) {
	    zfile_fclose (drv->writediskfile);
	    drv->writediskfile = 0;
	    drv->wrprot = 1;
	}
    } else if (zfile_iscompressed (drv->diskfile)) {
	drv->wrprot = 1;
    }
    return drv->writediskfile ? 1 : 0;
}

static int diskfile_iswriteprotect (const char *fname, int *needwritefile, drive_type *drvtype)
{
    struct zfile *zf1, *zf2;
    int wrprot1 = 0, wrprot2 = 1;
    unsigned char buffer[25];
    
    *needwritefile = 0;
    *drvtype = DRV_35_DD;
    zf1 = DISK_validate_filename (fname, 1, &wrprot1);
    if (!zf1) return 1;
    if (zfile_iscompressed (zf1)) {
	wrprot1 = 1;
	*needwritefile = 1;
    }
    zf2 = getwritefile (fname, &wrprot2);
    zfile_fclose (zf2);
    zfile_fread (buffer, sizeof (char), 25, zf1);
    zfile_fclose (zf1);
    if (strncmp ((char *) buffer, "CAPS", 4) == 0) {
	*needwritefile = 1;
	return wrprot2;
    }
    if (strncmp ((char *) buffer, "Formatted Disk Image file", 25) == 0) {
	*needwritefile = 1;
	return wrprot2;
    }
    if (strncmp ((char *) buffer, "UAE-1ADF", 8) == 0) {
	if (wrprot1)
	    return wrprot2;
	return wrprot1;
    }
    if (strncmp ((char *) buffer, "UAE--ADF", 8) == 0) {
	*needwritefile = 1;
	return wrprot2;
    }
    if (memcmp (exeheader, buffer, sizeof(exeheader)) == 0)
	return 0;
    if (wrprot1)
	return wrprot2;
    return wrprot1;
}

static int drive_insert (drive * drv, int dnum, const char *fname)
{
    unsigned char buffer[2 + 2 + 4 + 4];
    trackid *tid;
    int num_tracks;

    drive_image_free (drv);
    drv->diskfile = DISK_validate_filename (fname, 1, &drv->wrprot);
    drv->ddhd = 1;
    drv->num_secs = 0;
    drv->hard_num_cyls = currprefs.dfxtype[dnum] == DRV_525_SD ? 40 : 80;
    free (drv->tracktiming);
    drv->tracktiming = 0;
    drv->useturbo = 0;
    
    if (!drv->motoroff)
	drv->dskready_time = DSKREADY_TIME;

    if (drv->diskfile == 0 && !drv->catweasel) {
	track_reset (drv);
	return 0;
    }
    strncpy (currprefs.df[dnum], fname, 255);
    currprefs.df[dnum][255] = 0;
    strncpy (changed_prefs.df[dnum], fname, 255);
    changed_prefs.df[dnum][255] = 0;
    gui_filename (dnum, fname);

    memset (buffer, 0, sizeof (buffer));
    if (drv->diskfile)
	zfile_fread (buffer, sizeof (char), 8, drv->diskfile);

    if (drv->catweasel) {

        drv->wrprot = 1;
	drv->filetype = ADF_CATWEASEL;
	drv->num_tracks = 80;
	drv->ddhd = 1;

#ifdef CAPS
    } else if (strncmp ((char *) buffer, "CAPS", 4) == 0) {

        drv->wrprot = 1;
	if (!caps_loadimage (drv->diskfile, drv - floppy, &num_tracks)) {
	    zfile_fclose (drv->diskfile);
	    drv->diskfile = 0;
	    return 0;
	}
        drv->num_tracks = num_tracks;
        drv->filetype = ADF_IPF;
#endif
    } else if (drv->fdi = fdi2raw_header (drv->diskfile)) {

	int len;
        drv->wrprot = 1;
	drv->num_tracks = fdi2raw_get_last_track (drv->fdi);
	drv->filetype = ADF_FDI;
	fdi2raw_read_track (drv->fdi, 0, &len);
	drv->num_secs = 11;
	if (len > 16000)
	    drv->num_secs = 22;

    } else if (strncmp ((char *) buffer, "UAE-1ADF", 8) == 0) {

	read_header_ext2 (drv->diskfile, drv->trackdata, &drv->num_tracks, &drv->ddhd);
	drv->filetype = ADF_EXT2;
	drv->num_secs = 11;
	if (drv->ddhd > 1)
	    drv->num_secs = 22;

    } else if (strncmp ((char *) buffer, "UAE--ADF", 8) == 0) {
	int offs = 160 * 4 + 8;
	int i;

        drv->wrprot = 1;
	drv->filetype = ADF_EXT1;
	drv->num_tracks = 160;
	drv->num_secs = 11;

	for (i = 0; i < 160; i++) {
	    tid = &drv->trackdata[i];
	    zfile_fread (buffer, 4, 1, drv->diskfile);
	    tid->sync = buffer[0] * 256 + buffer[1];
	    tid->len = buffer[2] * 256 + buffer[3];
	    tid->offs = offs;
	    if (tid->sync == 0) {
		tid->type = TRACK_AMIGADOS;
		tid->bitlen = 0;
	    } else {
		tid->type = TRACK_RAW1;
		tid->bitlen = tid->len * 8;
	    }
	    offs += tid->len;
	}

    } else if (memcmp (exeheader, buffer, sizeof(exeheader)) == 0) {

	int i;
	struct zfile *z = zfile_fopen_empty ("", 512 * 1760);
	createimagefromexe (drv->diskfile, z);
	drv->filetype = ADF_NORMAL;
	zfile_fclose (drv->diskfile);
	drv->diskfile = z;
	drv->num_tracks = 160;
	drv->num_secs = 11;
	for (i = 0; i < drv->num_tracks; i++) {
	    tid = &drv->trackdata[i];
	    tid->type = TRACK_AMIGADOS;
	    tid->len = 512 * drv->num_secs;
	    tid->bitlen = 0;
	    tid->offs = i * 512 * drv->num_secs;
	}
	drv->useturbo = 1;
    
    } else {
	int i;

	drv->filetype = ADF_NORMAL;

	zfile_fseek (drv->diskfile, 0, SEEK_END);
	i = zfile_ftell (drv->diskfile);
	zfile_fseek (drv->diskfile, 0, SEEK_SET);

	/* High-density disk? */
	if (i >= 160 * 22 * 512) {
	    drv->num_tracks = i / (512 * (drv->num_secs = 22));
	    drv->ddhd = 2;
	} else
	    drv->num_tracks = i / (512 * (drv->num_secs = 11));

	if (drv->num_tracks > MAX_TRACKS)
	    write_log ("Your diskfile is too big!\n");
	for (i = 0; i < drv->num_tracks; i++) {
	    tid = &drv->trackdata[i];
	    tid->type = TRACK_AMIGADOS;
	    tid->len = 512 * drv->num_secs;
	    tid->bitlen = 0;
	    tid->offs = i * 512 * drv->num_secs;
	}
    }
    openwritefile (drv, 0);
    drive_settype_id(drv);	/* Set DD or HD drive */
    drive_fill_bigbuf (drv, 1);
    drv->mfmpos = (rand () | (rand () << 16)) % drv->tracklen;
    drv->prevtracklen = 0;
    return 1;
}

static void rand_shifter (drive *drv)
{
    int r = ((rand () >> 4) & 7) + 1;
    while (r-- > 0) {
	word <<= 1;
	word |= (rand () & 0x1000) ? 1 : 0;
	bitoffset++;
	bitoffset &= 15;
    }
}

static int drive_empty (drive * drv)
{
#ifdef CATWEASEL
    if (drv->catweasel)
	return catweasel_disk_changed (drv->catweasel) == 0;
#endif
    return drv->diskfile == 0;
}

static void drive_step (drive * drv)
{
#ifdef CATWEASEL
    if (drv->catweasel) {
	int dir = direction ? -1 : 1;
	catweasel_step (drv->catweasel, dir);
	drv->cyl += dir;
	if (drv->cyl < 0)
	    drv->cyl = 0;
	write_log ("%d -> %d\n", dir, drv->cyl);
	return;
    }
#endif
    if (drv->steplimit) {
#ifdef DISK_DEBUG2
        write_log (" step ignored");
#endif
	return;
    }
    /* A1200's floppy drive needs at least 30 raster lines between steps
     * but we'll use very small value for better compatibility with faster CPU emulation
     * (stupid trackloaders with CPU delay loops)
     */
    drv->steplimit = 2;
    if (!drive_empty (drv))
	drv->dskchange = 0;
    if (direction) {
	if (drv->cyl) {
	    drv->cyl--;
	}
/*	else
	    write_log("program tried to step beyond track zero\n");
	    "no-click" programs does that
*/
  } else {
	int maxtrack = drv->hard_num_cyls;
	if (drv->cyl < maxtrack + 3) {
	    drv->cyl++;
#ifdef CATWEASEL
	    if (drv->catweasel)
		catweasel_step (drv->catweasel, 1);
#endif
	}
	if (drv->cyl >= maxtrack)
	    write_log("program tried to step over track %d\n", maxtrack);
    }
    rand_shifter (drv);
#ifdef DISK_DEBUG2
    write_log (" ->step %d", drv->cyl);
#endif
}

static int drive_track0 (drive * drv)
{
#ifdef CATWEASEL
    if (drv->catweasel)
	return catweasel_track0 (drv->catweasel);
#endif
    return drv->cyl == 0;
}

static int drive_writeprotected (drive * drv)
{
#ifdef CATWEASEL
    if (drv->catweasel)
	return 1;
#endif
    return drv->wrprot || drv->diskfile == NULL;
}

static int drive_running (drive * drv)
{
    return !drv->motoroff;
}

static void drive_motor (drive * drv, int off)
{
    if (drv->motoroff && !off) {
	drv->dskready_time = DSKREADY_TIME;
        rand_shifter (drv);
#ifdef DISK_DEBUG2
	write_log (" ->motor on");
#endif
    }
    if (!drv->motoroff && off) {
	drv->drive_id_scnt = 0; /* Reset id shift reg counter */
#ifdef DEBUG_DRIVE_ID
	write_log("drive_motor: Selected DF%d: reset id shift reg.\n",drv-floppy);
#endif
#ifdef DISK_DEBUG2
	write_log (" ->motor off");
#endif
    }
    drv->motoroff = off;
    if (drv->motoroff) {
	drv->dskready = 0;
	drv->dskready_time = 0;
    }
#ifdef CATWEASEL
    if (drv->catweasel)
	catweasel_set_motor (drv->catweasel, !drv->motoroff);
#endif
}

static void read_floppy_data (struct zfile *diskfile, trackid *tid, int offset, unsigned char *dst, int len)
{
    if (len == 0)
	return;
    zfile_fseek (diskfile, tid->offs + offset, SEEK_SET);
    zfile_fread (dst, 1, len, diskfile);
}

/* Megalomania does not like zero MFM words... */
static void mfmcode (uae_u16 * mfm, int words)
{
    uae_u32 lastword = 0;

    while (words--) {
	uae_u32 v = *mfm;
	uae_u32 lv = (lastword << 16) | v;
	uae_u32 nlv = 0x55555555 & ~lv;
	uae_u32 mfmbits = (nlv << 1) & (nlv >> 1);

	*mfm++ = v | mfmbits;
	lastword = v;
    }
}

static void drive_fill_bigbuf (drive * drv, int force)
{
    int tr = drv->cyl * 2 + side;
    trackid *ti = drv->trackdata + tr;

    if ((!drv->diskfile && !drv->catweasel) || tr >= drv->num_tracks) {
	track_reset (drv);
	return;
    }

    if (!force && drv->catweasel) {
	drv->buffered_cyl = -1;
	return;
    }

    if (!force && drv->buffered_cyl == drv->cyl && drv->buffered_side == side)
	return;
    drv->revolutions = 0;

    if (drv->writediskfile && drv->writetrackdata[tr].bitlen > 0) {
	int i;
	trackid *wti = &drv->writetrackdata[tr];
	drv->tracklen = wti->bitlen;
	read_floppy_data (drv->writediskfile, wti, 0, (char *) drv->bigmfmbuf, (wti->bitlen + 7) / 8);
	for (i = 0; i < (drv->tracklen + 15) / 16; i++) {
	    uae_u16 *mfm = drv->bigmfmbuf + i;
	    uae_u8 *data = (uae_u8 *) mfm;
	    *mfm = 256 * *data + *(data + 1);
	}
#ifdef DISK_DEBUG
	write_log ("track %d, length %d read from \"saveimage\"\n", tr, drv->tracklen);
#endif
    } else if (drv->filetype == ADF_CATWEASEL) {
#ifdef CATWEASEL
	drv->tracklen = 0;
	if (!catweasel_disk_changed (drv->catweasel)) {
	    drv->tracklen = catweasel_fillmfm (drv->catweasel, drv->bigmfmbuf, side, drv->ddhd, 0);
	}
	drv->buffered_cyl = -1;
	if (!drv->tracklen) {
	    track_reset (drv);
	    return;
	}
#endif	
    } else if (drv->filetype == ADF_IPF) {

#ifdef CAPS
	caps_loadtrack (drv->bigmfmbuf, drv->trackpointers, &drv->tracktiming, drv - floppy, tr, drv->tracklengths, &drv->revolutions);
	drv->tracklen = drv->tracklengths[0];
#endif

    } else if (drv->filetype == ADF_FDI) {

	uae_u8 *data;
	int i;
	data = fdi2raw_read_track (drv->fdi, tr, &drv->tracklen);
	for (i = 0; i < (drv->tracklen + 15) / 16; i++) {
	    uae_u16 *mfm = drv->bigmfmbuf + i;
	    mfm[0] = (data[0] << 8) | data[1];
	    data += 2;
	}

    } else if (ti->type == TRACK_AMIGADOS) {

	/* Normal AmigaDOS format track */
	int sec;
	int dstmfmoffset = 0;
        uae_u16 *dstmfmbuf = drv->bigmfmbuf;
        int len = drv->num_secs * 544 + FLOPPY_GAP_LEN;

	memset (dstmfmbuf, 0xaa, len * 2);
	dstmfmoffset += FLOPPY_GAP_LEN;
	drv->tracklen = len * 2 * 8;

	for (sec = 0; sec < drv->num_secs; sec++) {
	    uae_u8 secbuf[544];
	    uae_u16 mfmbuf[544];
	    int i;
	    uae_u32 deven, dodd;
	    uae_u32 hck = 0, dck = 0;

	    secbuf[0] = secbuf[1] = 0x00;
	    secbuf[2] = secbuf[3] = 0xa1;
	    secbuf[4] = 0xff;
	    secbuf[5] = tr;
	    secbuf[6] = sec;
	    secbuf[7] = drv->num_secs - sec;

	    for (i = 8; i < 24; i++)
		secbuf[i] = 0;

	    read_floppy_data (drv->diskfile, ti, sec * 512, &secbuf[32], 512);

	    mfmbuf[0] = mfmbuf[1] = 0xaaaa;
	    mfmbuf[2] = mfmbuf[3] = 0x4489;

	    deven = ((secbuf[4] << 24) | (secbuf[5] << 16)
		     | (secbuf[6] << 8) | (secbuf[7]));
	    dodd = deven >> 1;
	    deven &= 0x55555555;
	    dodd &= 0x55555555;

	    mfmbuf[4] = dodd >> 16;
	    mfmbuf[5] = dodd;
	    mfmbuf[6] = deven >> 16;
	    mfmbuf[7] = deven;

	    for (i = 8; i < 48; i++)
		mfmbuf[i] = 0xaaaa;
	    for (i = 0; i < 512; i += 4) {
		deven = ((secbuf[i + 32] << 24) | (secbuf[i + 33] << 16)
			 | (secbuf[i + 34] << 8) | (secbuf[i + 35]));
		dodd = deven >> 1;
		deven &= 0x55555555;
		dodd &= 0x55555555;
		mfmbuf[(i >> 1) + 32] = dodd >> 16;
		mfmbuf[(i >> 1) + 33] = dodd;
		mfmbuf[(i >> 1) + 256 + 32] = deven >> 16;
		mfmbuf[(i >> 1) + 256 + 33] = deven;
	    }

	    for (i = 4; i < 24; i += 2)
		hck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

	    deven = dodd = hck;
	    dodd >>= 1;
	    mfmbuf[24] = dodd >> 16;
	    mfmbuf[25] = dodd;
	    mfmbuf[26] = deven >> 16;
	    mfmbuf[27] = deven;

	    for (i = 32; i < 544; i += 2)
		dck ^= (mfmbuf[i] << 16) | mfmbuf[i + 1];

	    deven = dodd = dck;
	    dodd >>= 1;
	    mfmbuf[28] = dodd >> 16;
	    mfmbuf[29] = dodd;
	    mfmbuf[30] = deven >> 16;
	    mfmbuf[31] = deven;
	    mfmcode (mfmbuf + 4, 544 - 4);

	    for (i = 0; i < 544; i++) {
		dstmfmbuf[dstmfmoffset % len] = mfmbuf[i];
		dstmfmoffset++;
	    }
	 
	 }

#ifdef DISK_DEBUG
	write_log ("amigados read track %d\n", tr);
#endif
    } else {
	int i;
	int base_offset = ti->type == TRACK_RAW ? 0 : 1;
	drv->tracklen = ti->bitlen + 16 * base_offset;
	drv->bigmfmbuf[0] = ti->sync;
	read_floppy_data (drv->diskfile, ti, 0, (char *) (drv->bigmfmbuf + base_offset), (ti->bitlen + 7) / 8);
	for (i = base_offset; i < (drv->tracklen + 15) / 16; i++) {
	    uae_u16 *mfm = drv->bigmfmbuf + i;
	    uae_u8 *data = (uae_u8 *) mfm;
	    *mfm = 256 * *data + *(data + 1);
	}
#if 0 && defined DISK_DEBUG
	write_log ("rawtrack %d\n", tr);
#endif
    }
    drv->current_revolution = 0;
    drv->tracklengths[0] = drv->tracklen;
    drv->trackpointers[0] = drv->bigmfmbuf;
    drv->buffered_side = side;
    drv->buffered_cyl = drv->cyl;
    if (drv->tracklen == 0) {
	drv->tracklengths[0] = drv->tracklen = FLOPPY_WRITE_LEN * drv->ddhd * 2 * 8;
	memset (drv->bigmfmbuf, 0, FLOPPY_WRITE_LEN * 2 * drv->ddhd);
    }
    drv->trackspeed = get_floppy_speed () * drv->tracklen / (2 * 8 * FLOPPY_WRITE_LEN * drv->ddhd);
    updatemfmpos (drv);
}

/* Update ADF_EXT2 track header */
static void diskfile_update (struct zfile *diskfile, trackid *ti, int len, uae_u8 type)
{
    uae_u8 buf[2 + 2 + 4 + 4], *zerobuf;

    ti->bitlen = len;
    zfile_fseek (diskfile, 8 + 4 + (2 + 2 + 4 + 4) * ti->track, SEEK_SET);
    memset (buf, 0, sizeof buf);
    ti->type = type;
    buf[3] = ti->type;
    do_put_mem_long ((uae_u32 *) (buf + 4), ti->len);
    do_put_mem_long ((uae_u32 *) (buf + 8), ti->bitlen);
    zfile_fwrite (buf, sizeof (buf), 1, diskfile);
    if (ti->len > (len + 7) / 8) {
	zerobuf = malloc (ti->len);
	memset (zerobuf, 0, ti->len);
	zfile_fseek (diskfile, ti->offs, SEEK_SET);
	zfile_fwrite (zerobuf, 1, ti->len, diskfile);
	free (zerobuf);
    }
#ifdef DISK_DEBUG
    write_log ("track %d, raw track length %d written (total size %d)\n", ti->track, (ti->bitlen + 7) / 8, ti->len);
#endif
}

#define MFMMASK 0x55555555
static uae_u32 getmfmlong (uae_u16 * mbuf)
{
    return ((*mbuf << 16) | *(mbuf + 1)) & MFMMASK;
}

static int drive_write_adf_amigados (drive * drv)
{
    int i, secwritten = 0;
    int fwlen = FLOPPY_WRITE_LEN * drv->ddhd;
    int length = 2 * fwlen;
    int drvsec = drv->num_secs;
    uae_u32 odd, even, chksum, id, dlong;
    uae_u8 *secdata;
    uae_u8 secbuf[544];
    uae_u16 *mbuf = drv->bigmfmbuf;
    uae_u16 *mend = mbuf + length;
    char sectable[22];

    if (!drvsec)
	return 2;
    memset (sectable, 0, sizeof (sectable));
    memcpy (mbuf + fwlen, mbuf, fwlen * sizeof (uae_u16));
    mend -= (4 + 16 + 8 + 512);
    while (secwritten < drvsec) {
	int trackoffs;

	do {
	    while (*mbuf++ != 0x4489) {
		if (mbuf >= mend)
		    return 1;
	    }
	} while (*mbuf++ != 0x4489);

	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	id = (odd << 1) | even;

	trackoffs = (id & 0xff00) >> 8;
	if (trackoffs + 1 > drvsec) {
	    write_log ("Disk write: weird sector number %d\n", trackoffs);
	    if (drv->filetype == ADF_EXT2)
		return 2;
	    continue;
	}
	chksum = odd ^ even;
	for (i = 0; i < 4; i++) {
	    odd = getmfmlong (mbuf);
	    even = getmfmlong (mbuf + 8);
	    mbuf += 2;

	    dlong = (odd << 1) | even;
	    if (dlong) {
		if (drv->filetype == ADF_EXT2)
		    return 6;
		secwritten = -200;
	    }
	    chksum ^= odd ^ even;
	}			/* could check here if the label is nonstandard */
	mbuf += 8;
	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	if (((odd << 1) | even) != chksum || ((id & 0x00ff0000) >> 16) != drv->cyl * 2 + side) {
	    write_log ("Disk write: checksum error on sector %d header\n", trackoffs);
	    if (drv->filetype == ADF_EXT2)
		return 3;
	    continue;
	}
	odd = getmfmlong (mbuf);
	even = getmfmlong (mbuf + 2);
	mbuf += 4;
	chksum = (odd << 1) | even;
	secdata = secbuf + 32;
	for (i = 0; i < 128; i++) {
	    odd = getmfmlong (mbuf);
	    even = getmfmlong (mbuf + 256);
	    mbuf += 2;
	    dlong = (odd << 1) | even;
	    *secdata++ = dlong >> 24;
	    *secdata++ = dlong >> 16;
	    *secdata++ = dlong >> 8;
	    *secdata++ = dlong;
	    chksum ^= odd ^ even;
	}
	mbuf += 256;
	if (chksum) {
	    write_log ("Disk write: sector %d, data checksum error\n", trackoffs);
	    if (drv->filetype == ADF_EXT2)
		return 4;
	    continue;
	}
	sectable[trackoffs] = 1;
	secwritten++;
	memcpy (writebuffer + trackoffs * 512, secbuf + 32, 512);
    }
    if (drv->filetype == ADF_EXT2 && (secwritten == 0 || secwritten < 0))
	return 5;
    if (secwritten == 0)
	write_log ("Disk write in unsupported format\n");
    if (secwritten < 0)
	write_log ("Disk write: sector labels ignored\n");

    if (drv->filetype == ADF_EXT2)
	diskfile_update (drv->diskfile, &drv->trackdata[drv->cyl * 2 + side], drvsec * 512 * 8, TRACK_AMIGADOS);
    for (i = 0; i < drvsec; i++) {
	zfile_fseek (drv->diskfile, drv->trackdata[drv->cyl * 2 + side].offs + i * 512, SEEK_SET);
	zfile_fwrite (writebuffer + i * 512, sizeof (uae_u8), 512, drv->diskfile);
    }

    return 0;
}

/* write raw track to disk file */
static int drive_write_ext2 (uae_u16 *bigmfmbuf, struct zfile *diskfile, trackid *ti, int tracklen)
{
    int len, i;

    len = (tracklen + 7) / 8;
    if (len > ti->len) {
	write_log ("disk raw write: image file's track %d is too small (%d < %d)!\n", ti->track, ti->len, len);
	return 0;
    }
    diskfile_update (diskfile, ti, tracklen, TRACK_RAW);
    for (i = 0; i < ti->len / 2; i++) {
	uae_u16 *mfm = bigmfmbuf + i;
	uae_u16 *mfmw = bigmfmbufw + i;
	uae_u8 *data = (uae_u8 *) mfm;
	*mfmw = 256 * *data + *(data + 1);
    }
    zfile_fseek (diskfile, ti->offs, SEEK_SET);
    zfile_fwrite (bigmfmbufw, 1, len, diskfile);
    return 1;
}

static void drive_write_data (drive * drv)
{
    int ret;
    if (drive_writeprotected (drv)) {
	/* read original track back because we didn't really write anything */
	drv->buffered_side = 2;
	return;
    }
    if (drv->writediskfile)
	drive_write_ext2 (drv->bigmfmbuf, drv->writediskfile, &drv->writetrackdata[drv->cyl * 2 + side], drv->tracklen);

    switch (drv->filetype) {
    case ADF_NORMAL:
	drive_write_adf_amigados (drv);
	return;
    case ADF_EXT1:
	break;
    case ADF_EXT2:
	ret = drive_write_adf_amigados (drv);
	if (ret) {
	    write_log("not an amigados track (error %d), writing as raw track\n",ret);
	    drive_write_ext2 (drv->bigmfmbuf, drv->diskfile, &drv->trackdata[drv->cyl * 2 + side], drv->tracklen);
	}
	return;
    case ADF_IPF:
	break;
    }
}

static void drive_eject (drive * drv)
{
    drive_image_free (drv);
    drv->dskchange = 1;
    drv->ddhd = 1;
    drv->dskchange_time = 0;
    drv->dskready = 0;
    drv->dskready_time = 0;
    drive_settype_id(drv); /* Back to 35 DD */
#ifdef DISK_DEBUG
    write_log ("eject drive %d\n", drv - &floppy[0]);
#endif
}

/* We use this function if we have no Kickstart ROM.
 * No error checking - we trust our luck. */
void DISK_ersatz_read (int tr, int sec, uaecptr dest)
{
    uae_u8 *dptr = get_real_address (dest);
    zfile_fseek (floppy[0].diskfile, floppy[0].trackdata[tr].offs + sec * 512, SEEK_SET);
    zfile_fread (dptr, 1, 512, floppy[0].diskfile);
}

/* type: 0=regular, 1=ext2adf */
/* adftype: 0=DD,1=HD,2=525SD */
void disk_creatediskfile (char *name, int type, drive_type adftype)
{
    struct zfile *f;
    int i, l, file_size, tracks, track_len;
    char *chunk = NULL;
    uae_u8 tmp[3*4];
    char *disk_name = "empty";

    if (type == 1) tracks = 2 * 83; else tracks = 2 * 80;
    file_size = 880 * 1024;
    track_len = FLOPPY_WRITE_LEN * 2;
    if (adftype == 1) {
	file_size *= 2;
	track_len *= 2;
    } else if (adftype == 2) {
	file_size /= 2;
	tracks /= 2;
    }

    f = zfile_fopen (name, "wb");
    chunk = xmalloc (16384);
    if (f && chunk) {
	memset(chunk,0,16384);
	switch(type)
	    {
	    case 0:
	    for (i = 0; i < file_size; i += 11264) {
		memset(chunk, 0, 11264);
		if (i == 0) {
		    /* boot block */
		    strcpy (chunk, "DOS");
		} else if (i == file_size / 2) {
		    int block = file_size / 1024;
		    /* root block */
		    chunk[0+3] = 2;
		    chunk[12+3] = 0x48;
		    chunk[312] = chunk[313] = chunk[314] = chunk[315] = (uae_u8)0xff;
		    chunk[316+2] = (block + 1) >> 8; chunk[316+3] = (block + 1) & 255;
		    chunk[432] = strlen (disk_name);
		    strcpy (chunk + 433, disk_name);
		    chunk[508 + 3] = 1;
		    disk_date (chunk + 420);
		    memcpy (chunk + 472, chunk + 420, 3 * 4);
		    memcpy (chunk + 484, chunk + 420, 3 * 4);
		    disk_checksum(chunk, chunk + 20);
		    /* bitmap block */
		    memset (chunk + 512 + 4, 0xff, 220);
		    chunk[512 + 112 + 2] = 0x3f;
		    disk_checksum(chunk + 512, chunk + 512);

		}
		zfile_fwrite (chunk, 11264, 1, f);
	    }
	    break;
	    case 1:
	    l = track_len;
	    zfile_fwrite ("UAE-1ADF", 8, 1, f);
	    tmp[0] = 0; tmp[1] = 0; /* flags (reserved) */
	    tmp[2] = 0; tmp[3] = tracks; /* number of tracks */
	    zfile_fwrite (tmp, 4, 1, f);
	    tmp[0] = 0; tmp[1] = 0; /* flags (reserved) */
	    tmp[2] = 0; tmp[3] = 1; /* track type */
	    tmp[4] = 0; tmp[5] = 0; tmp[6]=(uae_u8)(l >> 8); tmp[7] = (uae_u8)l;
	    tmp[8] = 0; tmp[9] = 0; tmp[10] = 0; tmp[11] = 0;
	    for (i = 0; i < tracks; i++) zfile_fwrite (tmp, sizeof (tmp), 1, f);
	    for (i = 0; i < tracks; i++) zfile_fwrite (chunk, l, 1, f);
	    break;
	}
    }
    free (chunk);
    zfile_fclose (f);
}

int disk_getwriteprotect (const char *name)
{
    int needwritefile;
    drive_type drvtype;
    return diskfile_iswriteprotect (name, &needwritefile, &drvtype);
}

static void diskfile_readonly (const char *name, int readonly)
{
    struct stat st;
    int mode, oldmode;
    
    if (stat (name, &st))
	return;
    oldmode = mode = st.st_mode;
//    mode &= ~FILEFLAG_WRITE;
//    if (!readonly) mode |= FILEFLAG_WRITE;
    if (mode != oldmode)
	chmod (name, mode);
}

int disk_setwriteprotect (int num, const char *name, int protect)
{
    int needwritefile, oldprotect;
    struct zfile *zf1, *zf2;
    int wrprot1, wrprot2, i;
    char *name2;
    drive_type drvtype;
 
    oldprotect = diskfile_iswriteprotect (name, &needwritefile, &drvtype);
    zf1 = DISK_validate_filename (name, 1, &wrprot1);
    if (!zf1) return 0;
    if (zfile_iscompressed (zf1)) wrprot1 = 1;
    zf2 = getwritefile (name, &wrprot2);
    name2 = getwritefilename (name);

    if (needwritefile && zf2 == 0)
	disk_creatediskfile (name2, 1, drvtype);
    zfile_fclose (zf2);
    if (protect && iswritefileempty (name)) {
	for (i = 0; i < 4; i++) {
	    if (!strcmp (name, floppy[i].newname))
		drive_eject (&floppy[i]);
	}
	unlink (name2);
    }

    if (!needwritefile)
        diskfile_readonly (name, protect);
    diskfile_readonly (name2, protect);
    drive_eject (&floppy[num]);
    floppy[num].dskchange_time = 20; /* 2 second disk change delay */
    return 1;
}

void disk_eject (int num)
{
    gui_filename (num, "");
    drive_eject (floppy + num);
    *currprefs.df[num] = *changed_prefs.df[num] = 0;
    floppy[num].newname[0] = 0;
}

void disk_insert (int num, const char *name)
{
    drive *drv = floppy + num;
    strcpy (drv->newname, name);
    if (name[0] == 0) {
	disk_eject (num);
    } else if (!drive_empty(drv) || drv->dskchange_time > 0) {
	drive_eject (drv);
	/* set dskchange_time, disk_insert() will be
	 * called from DISK_check_change() after 2 second delay
	 * this makes sure that all programs detect disk change correctly
	 */
	drv->dskchange_time = 20; /* 2 second disk change delay */
    } else {
	/* no delayed insert if drive is already empty */
	drive_insert (drv, num, name);
    }
}

void DISK_check_change (void)
{
    int i;

    if (currprefs.floppy_speed != changed_prefs.floppy_speed)
	currprefs.floppy_speed = changed_prefs.floppy_speed;
    for (i = 0; i < 4; i++) {
	drive *drv = floppy + i;
	gui_lock ();
	if (currprefs.dfxtype[i] != changed_prefs.dfxtype[i]) {
	    currprefs.dfxtype[i] = changed_prefs.dfxtype[i];
	    reset_drive (i);
	}
	if (strcmp (currprefs.df[i], changed_prefs.df[i])) {
	    strcpy (currprefs.df[i], changed_prefs.df[i]);
	    disk_insert (i, currprefs.df[i]);
	}
	gui_unlock ();
	/* emulate drive motor turn on time */
	if (drv->dskready_time) {
	    drv->dskready_time--;
	    if (drv->dskready_time == 0)
		drv->dskready = 1;
	}
	/* delay until new disk image is inserted */
	if (drv->dskchange_time) {
	    drv->dskchange_time--;
	    if (drv->dskchange_time == 0) {
		drive_insert (drv, i, drv->newname);
#ifdef DISK_DEBUG
		write_log ("delayed insert, drive %d, image '%s'\n", i, drv->newname);
#endif
	    }
	}
    }
}

int disk_empty (int num)
{
    return drive_empty (floppy + num);
}

#ifdef DISK_DEBUG2
static char *tobin(uae_u8 v)
{
    int i;
    static char buf[10];
    for( i = 7; i >= 0; i--)
	buf[7 - i] = v & (1 << i) ? '1' : '0';
    buf[i] = 0;
    return buf;
}
#endif

void DISK_select (uae_u8 data)
{
    int step_pulse, lastselected, dr;
    static uae_u8 prevdata;
    static int step;

#ifdef DISK_DEBUG2
    write_log ("%08.8X %02.2X %s", m68k_getpc(), data, tobin(data));
#endif
    lastselected = selected;
    selected = (data >> 3) & 15;
    side = 1 - ((data >> 2) & 1);
    direction = (data >> 1) & 1;
    step_pulse = data & 1;

#ifdef DISK_DEBUG2
    write_log (" %d%d%d%d% ", (selected & 1) ? 0 : 1, (selected & 2) ? 0 : 1, (selected & 4) ? 0 : 1, (selected & 8) ? 0 : 1);
    if ((prevdata & 0x80) != (data & 0x80))
	write_log (" dskmotor %d ", (data & 0x80) ? 1 : 0);
    if ((prevdata & 0x02) != (data & 0x02))
	write_log (" direct %d ", (data & 0x02) ? 1 : 0);
    if ((prevdata & 0x04) != (data & 0x04))
	write_log (" side %d ", (data & 0x04) ? 1 : 0);
#endif

    selected |= disabled;

    if (step != step_pulse) {
#ifdef DISK_DEBUG2
	write_log (" dskstep %d ", step_pulse);
#endif
	step = step_pulse;
	if (step && !savestate_state) {
	    for (dr = 0; dr < 4; dr++) {
		if (!(selected & (1 << dr))) {
		    drive_step (floppy + dr);
		}
	    }
	}
    }
    if (!savestate_state) {
	for (dr = 0; dr < 4; dr++) {
	    drive *drv = floppy + dr;
	    /* motor on/off workings tested with small assembler code on real Amiga 1200. */
	    /* motor/id flipflop is set only when drive select goes from high to low */ 
	    if (!(selected & (1 << dr)) && (lastselected & (1 << dr)) ) {
		drv->drive_id_scnt++;
		drv->drive_id_scnt &= 31;
		drv->idbit = (drv->drive_id & (1L << (31 - drv->drive_id_scnt))) ? 1 : 0;
		if ((prevdata & 0x80) == 0 || (data & 0x80) == 0) {
		    /* motor off: if motor bit = 0 in prevdata or data -> turn motor on */
		    drive_motor (drv, 0);
		} else if (prevdata & 0x80) {
		    /* motor on: if motor bit = 1 in prevdata only (motor flag state in data has no effect)
			-> turn motor off */
		    drive_motor (drv, 1);
		}
		if (currprefs.dfxtype[dr] == DRV_35_DD) {
		    if (dr == 0) /* A500/A2000 internal drive always returns 0 */
			drv->idbit = 0;
		    else /* regular external DD drive always returns 1 */
			drv->idbit = 1;
		}
#ifdef DEBUG_DRIVE_ID
		write_log("DISK_status: sel %d id %s (%08.8X) [0x%08lx, bit #%02d: %d]\n",
		    dr, drive_id_name(drv), drv->drive_id, drv->drive_id << drv->drive_id_scnt, 31 - drv->drive_id_scnt, drv->idbit);
#endif
	    }
	}
    }
    for (dr = 0; dr < 4; dr++) {
	floppy[dr].state = (!(selected & (1 << dr))) | !floppy[dr].motoroff;
	update_drive_gui (dr);
    }
    prevdata = data;
#ifdef DISK_DEBUG2
    write_log ("\n");
#endif
}

uae_u8 DISK_status (void)
{
    uae_u8 st = 0x3c;
    int dr;

    for (dr = 0; dr < 4; dr++) {
	drive *drv = floppy + dr;
	if (!(selected & (1 << dr))) {
	    if (drive_running (drv)) {
		if (drv->catweasel) {
#ifdef CATWEASEL
		    if (catweasel_diskready (drv->catweasel))
			st &= ~0x20;
#endif
		} else {
		    if (drv->dskready)
			st &= ~0x20;
		}
	    } else {
		/* report drive ID */
		if (drv->idbit)
		    st &= ~0x20;
	    }
	    if (drive_track0 (drv))
		st &= ~0x10;
	    if (drive_writeprotected (drv))
		st &= ~8;
	    if (drv->catweasel) {
#ifdef CATWEASEL
		if (catweasel_disk_changed (drv->catweasel))
		    st &= ~4;
#endif
	    } else if (drv->dskchange && currprefs.dfxtype[dr] != DRV_525_SD) {
		st &= ~4;
	    }
	} 
    }
    return st;
}

static int unformatted (drive *drv)
{
    int tr = drv->cyl * 2 + side;
    if (tr >= drv->num_tracks)
	return 1;
    if (drv->filetype == ADF_EXT2 && drv->trackdata[tr].bitlen == 0)
	return 1;
    return 0;
}

/* get one bit from MFM bit stream */
STATIC_INLINE uae_u32 getonebit (uae_u16 * mfmbuf, int mfmpos)
{
    uae_u16 *buf;

    buf = &mfmbuf[mfmpos >> 4];
    return (buf[0] & (1 << (15 - (mfmpos & 15)))) ? 1 : 0;
}

void dumpdisk (void)
{
    int i, j, k;
    uae_u16 w;

    for (i = 0; i < 4; i++) {
	drive *drv = &floppy[i];
	if (!(disabled & (1 << i))) {
	    write_log ("Drive %d: motor %s cylinder %2d sel %s %s mfmpos %d/%d\n",
		i, drv->motoroff ? "off" : " on", drv->cyl, (selected & (1 << i)) ? "no" : "yes",
		drive_writeprotected(drv) ? "ro" : "rw", drv->mfmpos, drv->tracklen);
	    w = word;
	    for (j = 0; j < 15; j++) {
		write_log ("%04.4X ", w);
		for (k = 0; k < 16; k++) {
		    w <<= 1;
		    w |= getonebit (drv->bigmfmbuf, drv->mfmpos + j * 16 + k);
		}
	    }
	    write_log ("\n");
	}
    }
    write_log ("side %d, dma %d, bitoffset %d, word %06.6X, dskbytr %04.4X adkcon %04.4X dsksync %04.4X\n", side, dskdmaen, bitoffset, word, dskbytr_val, adkcon, dsksync);
}

static void disk_dmafinished (void)
{
    INTREQ (0x8002);
    dskdmaen = 0;
#ifdef DISK_DEBUG
    write_log("disk dma finished %08.8X\n", dskpt);
#endif
}    

extern void cia_diskindex (void);

static int diskevent_flag;
static int disk_sync_cycle;

void DISK_handler (void)
{
    int flag = diskevent_flag;
    eventtab[ev_disk].active = 0;
    DISK_update (disk_sync_cycle);
    if (flag & DISK_WORDSYNC) {
	INTREQ (0x8000 | 0x1000);
    }
    if (flag & DISK_INDEXSYNC) {
       cia_diskindex ();
    }
}

static void disk_doupdate_write (drive * drv, int floppybits)
{
    int dr;
    int drives[4];
    
    for (dr = 0; dr < 4 ; dr++) {
        drive *drv2 = &floppy[dr];
        drives[dr] = 0;
        if (drv2->motoroff)
	    continue;
        if (selected & (1 << dr))
	    continue;
	drives[dr] = 1;
    }

    while (floppybits >= drv->trackspeed) {
	for (dr = 0; dr < 4; dr++) {
	    if (drives[dr]) {
		floppy[dr].mfmpos++;
		floppy[dr].mfmpos %= drv->tracklen;
	    }
	}
	if ((dmacon & 0x210) == 0x210 && dskdmaen == 3 && dsklength > 0) {
	    bitoffset++;
	    bitoffset &= 15;
	    if (!bitoffset) {
		for (dr = 0; dr < 4 ; dr++) {
		    drive *drv2 = &floppy[dr];
		    if (drives[dr])
			drv2->bigmfmbuf[drv2->mfmpos >> 4] = get_word (dskpt);
		}
		dskpt += 2;
		dsklength--;
		if (dsklength == 0) {
		    disk_dmafinished ();
		    drive_write_data (drv);
		}
	    }
	}
	floppybits -= drv->trackspeed;
    }
}

static void updatetrackspeed (drive *drv, int mfmpos)
{
    uae_u16 *p = drv->trackpointers[drv->current_revolution] - drv->trackpointers[0] + drv->tracktiming;
    p += mfmpos / 8;
    drv->trackspeed = get_floppy_speed () * drv->tracklen / (2 * 8 * FLOPPY_WRITE_LEN * drv->ddhd);
    drv->trackspeed = drv->trackspeed * p[0] / 1000;
}

static void disk_doupdate_predict (drive * drv, int startcycle)
{
    int is_sync = 0;
    int firstcycle = startcycle;
    uae_u32 tword = word;
    int mfmpos = drv->mfmpos;

    diskevent_flag = 0;
    while (startcycle < (maxhpos << 8) && !diskevent_flag) {
	int cycle = startcycle >> 8;
	if (drv->tracktiming)
	    updatetrackspeed (drv, mfmpos);
	if (dskdmaen != 3) {
	    tword <<= 1;
	    if (unformatted (drv))
	        tword |= (rand() & 0x1000) ? 1 : 0;
	    else
	        tword |= getonebit (drv->trackpointers[drv->current_revolution], mfmpos);
	    if ((tword & 0xffff) == dsksync)
		diskevent_flag |= DISK_WORDSYNC;
	}
	mfmpos++;
	mfmpos %= drv->tracklen;
	if (!mfmpos && drv->dskready)
	    diskevent_flag |= DISK_INDEXSYNC;
	startcycle += drv->trackspeed;
    }
    if (drv->tracktiming)
        updatetrackspeed (drv, drv->mfmpos);
    if (diskevent_flag) {
	disk_sync_cycle = startcycle >> 8;
        eventtab[ev_disk].oldcycles = get_cycles ();
        eventtab[ev_disk].evtime = get_cycles () + startcycle - firstcycle;
	eventtab[ev_disk].active = 1;
	events_schedule ();
    }
}

#ifdef CPUEMU_6
extern uae_u8 cycle_line[256];
#endif

static void disk_doupdate_read (drive * drv, int floppybits)
{
    int j = 0, k = 1, l = 0;

/*
    uae_u16 *mfmbuf = drv->bigmfmbuf;
    dsksync = 0x4444;
    adkcon |= 0x400;
    drv->mfmpos = 0;
    memset (mfmbuf, 0, 1000);
    cycles = 0x1000000;
    // 4444 4444 4444 aaaa aaaaa 4444 4444 4444
    // 4444 aaaa aaaa 4444
    mfmbuf[0] = 0x4444;
    mfmbuf[1] = 0x4444;
    mfmbuf[2] = 0x4444;
    mfmbuf[3] = 0xaaaa;
    mfmbuf[4] = 0xaaaa;
    mfmbuf[5] = 0x4444;
    mfmbuf[6] = 0x4444;
    mfmbuf[7] = 0x4444;
*/
    while (floppybits >= drv->trackspeed) {
        if (drv->tracktiming)
	    updatetrackspeed (drv, drv->mfmpos);
	word <<= 1;
        if (unformatted (drv))
	    word |= (rand() & 0x1000) ? 1 : 0;
	else
	    word |= getonebit (drv->trackpointers[drv->current_revolution], drv->mfmpos);
	//write_log ("%08.8X bo=%d so=%d mfmpos=%d dma=%d\n", (word & 0xffffff), bitoffset, syncoffset, drv->mfmpos,dma_enable);
	drv->mfmpos++;
	drv->mfmpos %= drv->tracklen;
	if (drv->mfmpos == 0) {
	    drv->current_revolution++;
	    if (drv->current_revolution >= drv->revolutions)
		drv->current_revolution = 0;
	    drv->tracklen = drv->tracklengths[drv->current_revolution];
	    drv->trackspeed = get_floppy_speed () * drv->tracklen / (2 * 8 * FLOPPY_WRITE_LEN * drv->ddhd);
	}

	if (bitoffset == 15 && dma_enable && dskdmaen == 2 && dsklength >= 0) {
	    if (dsklength > 0) {
		put_word (dskpt, word);
		dskpt += 2;
#ifdef CPUEMU_6
		cycle_line[7] |= CYCLE_DISK;
		cycle_line[9] |= CYCLE_DISK;
#endif
	    }
#if 0
	    dma_tab[j++] = word;
	    if (j == MAX_DISK_WORDS_PER_LINE - 1) {
	        write_log ("Bug: Disk DMA buffer overflow!\n");
	        j--;
	    }
#endif
	    dsklength--;
	    if (dsklength < 0)
	        disk_dmafinished ();
	    //write_log ("->dma %04.4X\n", word);
	}
	if ((bitoffset & 7) == 7) {
	    dskbytr_val = word & 0xff;
	    dskbytr_val |= 0x8000;
	}
	if (word == dsksync) {
	    if (adkcon & 0x400)
	        bitoffset = 15;
	    dma_enable = 1;
	}
	
	bitoffset++;
	bitoffset &= 15;
	floppybits -= drv->trackspeed;
    }
#if 0
    dma_tab[j] = 0xffffffff;
#endif
}

#if 0
/* disk DMA fetch happens on real Amiga at the beginning of next horizontal line
   (cycles 9, 11 and 13 according to hardware manual) We transfer all DMA'd
   data at cycle 0. I don't think any program cares about this small difference.
*/
static void dodmafetch (void)
{
    int i;

    i = 0;
    while (dma_tab[i] != 0xffffffff && dskdmaen != 3 && (dmacon & 0x210) == 0x210) {
        put_word (dskpt, dma_tab[i++]);
        dskpt += 2;
    }
    dma_tab[0] = 0xffffffff;
}
#endif

/* this is very unoptimized. DSKBYTR is used very rarely, so it should not matter. */

uae_u16 DSKBYTR (int hpos)
{
    uae_u16 v;

    DISK_update (hpos);
    v = dskbytr_val;
    dskbytr_val &= ~0x8000;
    if (word == dsksync)
	v |= 0x1000;
    if (dskdmaen && (dmacon & 0x210) == 0x210)
	v |= 0x4000;
    if (dsklen & 0x4000)
	v |= 0x2000;
//    write_log ("DSKBYTR=%04.4X hpos=%d\n", v, hpos);
    return v;
}

static void DISK_start (void)
{
    int dr;

    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	if (!(selected & (1 << dr))) {
	    int tr = drv->cyl * 2 + side;
	    trackid *ti = drv->trackdata + tr;

	    if (dskdmaen == 3) {
		drv->tracklen = FLOPPY_WRITE_LEN * drv->ddhd * 8 * 2;
		drv->trackspeed = get_floppy_speed ();
		updatemfmpos (drv);
	    }
	    /* Ugh.  A nasty hack.  Assume ADF_EXT1 tracks are always read
	       from the start.  */
	    if (ti->type == TRACK_RAW1)
		drv->mfmpos = 0;
	    if (drv->catweasel)
		drive_fill_bigbuf (drv, 1);
	}
        drv->floppybitcounter = 0;
    }
    dma_enable = (adkcon & 0x400) ? 0 : 1;
}

static int linecounter;

void DISK_update (int tohpos)
{
    int dr;
    int cycles = (tohpos << 8) - disk_hpos;
    int startcycle = disk_hpos;

    if (cycles <= 0)
	return;
    disk_hpos += cycles;
    if (disk_hpos >= (maxhpos << 8))
	disk_hpos -= maxhpos << 8;

    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	if (drv->steplimit)
	    drv->steplimit--;
    }
    if (linecounter) {
	linecounter--;
	if (! linecounter)
	    disk_dmafinished ();
	return;
    }

#if 0
    dodmafetch ();
#endif

    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	
	if (drv->motoroff)
	    continue;
        drv->floppybitcounter += cycles;
	if (selected & (1 << dr)) {
	    drv->mfmpos += drv->floppybitcounter / drv->trackspeed;
	    drv->mfmpos %= drv->tracklen;
            drv->floppybitcounter %= drv->trackspeed;
	    continue;
	}
	drive_fill_bigbuf (drv, 0);
	drv->mfmpos %= drv->tracklen;
    }
    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	if (drv->motoroff)
	    continue;
	if (selected & (1 << dr))
	    continue;
	if (dskdmaen == 3)
	    disk_doupdate_write (drv, drv->floppybitcounter);
	else
	    disk_doupdate_read (drv, drv->floppybitcounter);
	disk_doupdate_predict (drv, disk_hpos);
        drv->floppybitcounter %= drv->trackspeed;
	break;
    }
}

void DSKLEN (uae_u16 v, int hpos)
{
    int dr;

    DISK_update (hpos);
    if ((v & 0x8000) && (dsklen & 0x8000)) {
	dskdmaen = 2;
        DISK_start ();
    }
    if (!(v & 0x8000)) {
	if (dskdmaen) {
	/* Megalomania and Knightmare does this */
#ifdef DISK_DEBUG
	    if (dskdmaen == 2)
		write_log ("warning: Disk read DMA aborted, %d words left\n", dsklength);
#endif
	    if (dskdmaen == 3)
		write_log ("warning: Disk write DMA aborted, %d words left\n", dsklength);
	    dskdmaen = 0;
	}
    }
    dsklen = v;
    dsklength = dsklen & 0x3fff;

    if (dskdmaen == 0)
	return;

    if (v & 0x4000) {
	dskdmaen = 3;
	DISK_start ();
    }

#ifdef DISK_DEBUG
    for (dr = 0; dr < 4; dr++) {
        drive *drv = &floppy[dr];
        if (drv->motoroff)
	    continue;
	if ((selected & (1 << dr)) == 0)
	    break;
    }
    if (dr == 4)
        write_log ("disk %s DMA started but no drive selected!\n",
    	       dskdmaen == 3 ? "write" : "read");
    else
        write_log ("disk %s DMA started, track %d mfmpos %d\n",
	       dskdmaen == 3 ? "write" : "read", floppy[dr].cyl * 2 + side, floppy[dr].mfmpos);
    write_log ("LEN=%04.4X (%d) SYNC=%04.4X PT=%08.8X ADKCON=%04.4X PC=%08.8X\n", 
	dsklength, dsklength, (adkcon & 0x400) ? dsksync : 0xffff, dskpt, adkcon, m68k_getpc());
#endif

    /* Try to make floppy access from Kickstart faster.  */
    if (dskdmaen != 2 && dskdmaen != 3)
	return;
    for (dr = 0; dr < 4; dr++) {
        drive *drv = &floppy[dr];
        if (selected & (1 << dr))
	    continue;
	if (drv->filetype != ADF_NORMAL)
	    break;
    }
    if (dr < 4) /* no turbo mode if any selected drive has non-standard ADF */
	return;
    {
	int done = 0;
	for (dr = 0; dr < 4; dr++) {
	    drive *drv = &floppy[dr];
	    int pos, i;

	    if (drv->motoroff)
		continue;
	    if (!drv->useturbo && currprefs.floppy_speed > 0)
		continue;
	    if (selected & (1 << dr))
		continue;

	    pos = drv->mfmpos & ~15;
	    drive_fill_bigbuf (drv, 0);

	    if (dskdmaen == 2) { /* TURBO read */

	        if (adkcon & 0x400) {
		    for (i = 0; i < drv->tracklen; i += 16) {
		        pos += 16;
		        pos %= drv->tracklen;
		        if (drv->bigmfmbuf[pos >> 4] == dsksync) {
			    /* must skip first disk sync marker */
			    pos += 16;
			    pos %= drv->tracklen;
			    break;
		        }
		    }
		    if (i >= drv->tracklen)
		        return;
	        }
	        while (dsklength-- > 0) {
		    put_word (dskpt, drv->bigmfmbuf[pos >> 4]);
		    dskpt += 2;
		    pos += 16;
		    pos %= drv->tracklen;
	        }
		INTREQ (0x9000);
		done = 1;

	    } else if (dskdmaen == 3) { /* TURBO write */

		for (i = 0; i < dsklength; i++) {
		    drv->bigmfmbuf[pos >> 4] = get_word (dskpt + i * 2);
		    pos += 16;
		    pos %= drv->tracklen;
		}
		drive_write_data (drv);
		done = 1;
	    }
	}
	if (done) {
	    linecounter = 2;
	    dskdmaen = 0;
	    return;
	}	
    }
}


void DSKSYNC (int hpos, uae_u16 v)
{
    DISK_update (hpos);
    dsksync = v;
}

void DSKDAT (uae_u16 v)
{
    static int count = 0;
    if (count < 5) {
	count++;
	write_log ("%04.4X written to DSKDAT. Not good. PC=%08.8X", v, m68k_getpc());
	if (count == 5)
	    write_log ("(further messages suppressed)");

	write_log ("\n");
    }
}
void DSKPTH (uae_u16 v)
{
    dskpt = (dskpt & 0xffff) | ((uae_u32) v << 16);
}

void DSKPTL (uae_u16 v)
{
    dskpt = (dskpt & ~0xffff) | (v);
}

void DISK_free (void)
{
    int dr;
    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	drive_image_free (drv);
    }
}

void DISK_init (void)
{
    int dr;

#if 0
    dma_tab[0] = 0xffffffff;
#endif
    for (dr = 0; dr < 4; dr++) {
	drive *drv = &floppy[dr];
	/* reset all drive types to 3.5 DD */
	drive_settype_id (drv);
	if (!drive_insert (drv, dr, currprefs.df[dr]))
	    disk_eject (dr);
    }
    if (disk_empty (0))
	write_log ("No disk in drive 0.\n");
}

void DISK_reset (void)
{
    int i;

    if (savestate_state)
	return;

    //floppy[0].catweasel = &cwc.drives[0];
    disk_hpos = 0;
    dskdmaen = 0;
    disabled = 0;
    for (i = 0; i < 4; i++)
	reset_drive (i);
}

/* Disk save/restore code */

void DISK_save_custom (uae_u32 *pdskpt, uae_u16 *pdsklength, uae_u16 *pdsksync, uae_u16 *pdskbytr)
{
    if(pdskpt) *pdskpt = dskpt;
    if(pdsklength) *pdsklength = dsklength;
    if(pdsksync) *pdsksync = dsksync;
    if(pdskbytr) *pdskbytr = dskbytr_val;
}

void DISK_restore_custom (uae_u32 pdskpt, uae_u16 pdsklength, uae_u16 pdskbytr)
{
    dskpt = pdskpt;
    dsklength = pdsklength;
    dskbytr_val = pdskbytr;
}

uae_u8 *restore_disk(int num,uae_u8 *src)
{
    drive *drv;
    int state;

    drv = &floppy[num];
    disabled &= ~(1 << num);
    drv->drive_id = restore_u32 ();
    drv->motoroff = 1;
    drv->idbit = 0;
    state = restore_u8 ();
    if (state & 2) {
       disabled |= 1 << num;
    } else {
	drv->motoroff = (state & 1) ? 0 : 1;
	drv->idbit = (state & 4) ? 1 : 0;
    }
    drv->cyl = restore_u8 ();
    drv->dskready = restore_u8 ();
    drv->drive_id_scnt = restore_u8 ();
    drv->mfmpos = restore_u32 ();
    drv->dskchange = 0;
    drv->dskchange_time = 0;
    restore_u32 ();
    strncpy(changed_prefs.df[num],src,255);
    src+=strlen(src)+1;
    drive_insert (floppy + num, num, changed_prefs.df[num]);
    if (drive_empty (floppy + num)) drv->dskchange = 1;

    return src;
}

static uae_u32 getadfcrc (drive *drv)
{
    uae_u8 *b;
    uae_u32 crc32;
    int size;

    if (!drv->diskfile)
	return 0;
    zfile_fseek (drv->diskfile, 0, SEEK_END);
    size = zfile_ftell (drv->diskfile);
    b = malloc (size);
    if (!b)
	return 0;
    zfile_fseek (drv->diskfile, 0, SEEK_SET);
    zfile_fread (b, 1, size, drv->diskfile);
    crc32 = CRC32 (0, b, size);
    free (b);
    return crc32;
}

uae_u8 *save_disk(int num,int *len)
{
    uae_u8 *dstbak,*dst;
    drive *drv;

    drv = &floppy[num];
    dstbak = dst = malloc (2+1+1+1+1+4+4+256);
    save_u32 (drv->drive_id);	    /* drive type ID */
    save_u8 ((drv->motoroff ? 0:1) | ((disabled & (1 << num)) ? 2 : 0) | (drv->idbit ? 4 : 0));
    save_u8 (drv->cyl);		    /* cylinder */
    save_u8 (drv->dskready);	    /* dskready */
    save_u8 (drv->drive_id_scnt);   /* id mode position */
    save_u32 (drv->mfmpos);	    /* disk position */
    save_u32 (getadfcrc (drv));	    /* CRC of disk image */
    strcpy (dst, currprefs.df[num]);/* image name */
    dst += strlen(dst) + 1;

    *len = dst - dstbak;
    return dstbak;
}

/* internal floppy controller variables */

uae_u8 *restore_floppy(uae_u8 *src)
{
    word = restore_u16();
    bitoffset = restore_u8();
    dma_enable = restore_u8();
    disk_hpos = restore_u8() << 8;
    dskdmaen = restore_u8();
    restore_u16 ();
    //word |= restore_u16() << 16;

    return src;
}

uae_u8 *save_floppy(int *len)
{
    uae_u8 *dstbak, *dst;

    /* flush dma buffer before saving */
#if 0
    dodmafetch();
#endif
    dstbak = dst = malloc(2+1+1+1+1+2);
    save_u16 (word);		/* current fifo (low word) */
    save_u8 (bitoffset);	/* dma bit offset */
    save_u8 (dma_enable);	/* disk sync found */
    save_u8 (disk_hpos >> 8);	/* next bit read position */
    save_u8 (dskdmaen);		/* dma status */
    save_u16 (0);		/* was current fifo (high word), but it was wrong???? */

    *len = dst - dstbak;
    return dstbak;
}


