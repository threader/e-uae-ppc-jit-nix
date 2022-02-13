/*
 * UAE - The Un*x Amiga Emulator
 *
 * Support for IPF/CAPS disk images
 *
 * Copyright 2004 Richard Drummond
 *
 * Based on Win32 CAPS code by Toni Wilen
 */

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef CAPS

#include <caps/capsimage.h>
#include "zfile.h"
#include "caps.h"

static CapsLong caps_cont[4]= {-1, -1, -1, -1};
static int caps_locked[4];
static int caps_flags = DI_LOCK_DENVAR|DI_LOCK_DENNOISE|DI_LOCK_NOISE;


#ifndef __AMIGA__

/*
 * Repository for function pointers to the CAPSLib routines
 * which gets filled when we link at run-time
 *
 * We don't symbolically link on the Amiga, so don't need
 * this there
 */
struct {
  void *handle;
  CapsLong (*CAPSInit)(void);
  CapsLong (*CAPSExit)(void);
  CapsLong (*CAPSAddImage)(void);
  CapsLong (*CAPSRemImage)(CapsLong id);
  CapsLong (*CAPSLockImage)(CapsLong id, char *name);
  CapsLong (*CAPSLockImageMemory)(CapsLong id, CapsUByte *buffer, CapsULong length, CapsULong flag);
  CapsLong (*CAPSUnlockImage)(CapsLong id);
  CapsLong (*CAPSLoadImage)(CapsLong id, CapsULong flag);
  CapsLong (*CAPSGetImageInfo)(struct CapsImageInfo *pi, CapsLong id);
  CapsLong (*CAPSLockTrack)(struct CapsTrackInfo *pi, CapsLong id, CapsULong cylinder, CapsULong head, CapsULong flag);
  CapsLong (*CAPSUnlockTrack)(CapsLong id, CapsULong cylinder, CapsULong head);
  CapsLong (*CAPSUnlockAllTracks)(CapsLong id);
  char *(*CAPSGetPlatformName)(CapsULong pid);
} capslib;

#endif


#ifdef HAVE_DLOPEN

#include <dlfcn.h>

#define CAPSLIB_NAME    "libcapsimage.so.1"

/*
 * The Unix/dlopen method for loading and linking the CAPSLib plug-in
 */
static int load_capslib (void)
{
    /* This could be done more elegantly ;-) */
    if ((capslib.handle = dlopen(CAPSLIB_NAME, RTLD_LAZY))) {
	capslib.CAPSInit            = dlsym (capslib.handle, "CAPSInit");            if (dlerror () != 0) return 0;
	capslib.CAPSExit            = dlsym (capslib.handle, "CAPSExit");            if (dlerror () != 0) return 0;
	capslib.CAPSAddImage        = dlsym (capslib.handle, "CAPSAddImage");        if (dlerror () != 0) return 0;
	capslib.CAPSRemImage        = dlsym (capslib.handle, "CAPSRemImage");        if (dlerror () != 0) return 0;
	capslib.CAPSLockImage       = dlsym (capslib.handle, "CAPSLockImage");       if (dlerror () != 0) return 0;
	capslib.CAPSLockImageMemory = dlsym (capslib.handle, "CAPSLockImageMemory"); if (dlerror () != 0) return 0;
	capslib.CAPSUnlockImage     = dlsym (capslib.handle, "CAPSUnlockImage");     if (dlerror () != 0) return 0;
	capslib.CAPSLoadImage       = dlsym (capslib.handle, "CAPSLoadImage");       if (dlerror () != 0) return 0;
	capslib.CAPSGetImageInfo    = dlsym (capslib.handle, "CAPSGetImageInfo");    if (dlerror () != 0) return 0;
	capslib.CAPSLockTrack       = dlsym (capslib.handle, "CAPSLockTrack");       if (dlerror () != 0) return 0;
	capslib.CAPSUnlockTrack     = dlsym (capslib.handle, "CAPSUnlockTrack");     if (dlerror () != 0) return 0;
	capslib.CAPSUnlockAllTracks = dlsym (capslib.handle, "CAPSUnlockAllTracks"); if (dlerror () != 0) return 0;
	capslib.CAPSGetPlatformName = dlsym (capslib.handle, "CAPSGetPlatformName"); if (dlerror () != 0) return 0;
	if (capslib.CAPSInit() == imgeOk)
	    return 1;
    }
    return 0;
}

#else

#ifdef __AMIGA__

#include <proto/capsimage.h>
#include <proto/exec.h>

static struct MsgPort   CAPS_MsgPort;
static struct IORequest CAPS_IOReq;


static int unload_capslib (void)
{
    CloseDevice (CAPS_IOReq);
    DeleteIORequest (CAPS_IOReq);
    DeleteMsgPort (CAPS_MsgPort);
}

static int load_capslib (void)
{
    if ((CAPS_MsgPort = CreateMsgPort ())) {
	if ((CAPS_IOReq = CreateIORequest (CAPS_MsgPort, sizeof(struct IORequest)))) {
	    if (!OpenDevice(CAPS_NAME, 0, CAPS_IOReq, 0)) {
		CapsImageBase = (struct Library *)CAPS_IOReq->io_Device;
		atexit (unload_capslib);
		if (CAPSInit() == imgeOk)
		    return 1;
		else
		    CloseDevice (CAPS_IOReq);
	    }
	    DeleteIORequest (CAPS_IOReq);
	}
	DeleteMsgPort (CAPS_MsgPort);
    }

    return 0;
}

#else

/*
 * Sorry, we don't know how to load the CAPSLib plug-in
 * on other systems yet . ..
 */
static int load_capslib (void)
{
    return 0;
}

#endif
#endif


#ifndef __AMIGA__

/*
 * Some defines so that we don't care that CAPSLib
 * isn't statically linked
 */
#define CAPSInit            capslib.CAPSInit
#define CAPSExit            capslib.CAPSExit
#define CAPSAddImage        capslib.CAPSAddImage
#define CAPSRemImage        capslib.CAPSRemImage
#define CAPSLockImage       capslib.CAPSLockImage
#define CAPSLockImageMemory capslib.CAPSLockImageMemory
#define CAPSUnlockImage     capslib.CAPSUnlockImage
#define CAPSLoadImage       capslib.CAPSLoadImage
#define CAPSGetImageInfo    capslib.CAPSGetImageInfo
#define CAPSLockTrack       capslib.CAPSLockTrack
#define CAPSUnlockTrack     capslib.CAPSUnlockTrack
#define CAPSUnlockAllTracks capslib.CAPSUnlockAllTracks
#define CAPSGetPlatformName capslib.CAPSGetPlatformName

#endif


/*
 * CAPS support proper starts here
 *
 * This is more or less a straight copy of Toni's Win32 code
 */
int caps_init (void)
{
    static int init, noticed;
    int i;

    if (init) {
	return 1;
    }
    if (!load_capslib ()) {
	write_log ("Failed to load CAPS plug-in.\n");
	if (noticed)
	    return 0;
	gui_message ("This disk image needs the C.A.P.S. plugin\n"
	             "which is available from\n"
	             "http//www.caps-project.org/download.shtml\n");
	noticed = 1;
	return 0;
    }
    init = 1;
    for (i = 0; i < 4; i++) {
	caps_cont[i] = CAPSAddImage ();
    }

    return 1;
}

void caps_unloadimage (int drv)
{
    if (!caps_locked[drv])
	return;
    CAPSUnlockAllTracks (caps_cont[drv]);
    CAPSUnlockImage (caps_cont[drv]);
    caps_locked[drv] = 0;
}

int caps_loadimage (struct zfile *zf, int drv, int *num_tracks)
{
    struct CapsImageInfo ci;
    int len,ret ;
    uae_u8 *buf;

    if (!caps_init ())
	return 0;
    caps_unloadimage (drv);
    zfile_fseek (zf, 0, SEEK_END);
    len = zfile_ftell (zf);
    zfile_fseek (zf, 0, SEEK_SET);
    buf = xmalloc (len);
    if (!buf)
	return 0;
    if (zfile_fread (buf, len, 1, zf) == 0)
	return 0;
    ret = CAPSLockImageMemory(caps_cont[drv], buf, len, 0);
    free (buf);
    if (ret != imgeOk) {
	free (buf);
	return 0;
    }
    caps_locked[drv] = 1;
    CAPSGetImageInfo (&ci, caps_cont[drv]);
    *num_tracks = (ci.maxcylinder - ci.mincylinder + 1) * (ci.maxhead - ci.minhead + 1);
    CAPSLoadImage(caps_cont[drv], caps_flags);

    return 1;
}

int caps_loadtrack (uae_u16 *mfmbuf, uae_u16 **trackpointers, uae_u16 **tracktiming, int drv, int track, int *tracklengths, int *revolutions)
{
    struct CapsTrackInfo ci;
    unsigned int i, j, len;
    uae_u16 *tt, *mfm;

    *tracktiming = 0;
    CAPSLockTrack (&ci, caps_cont[drv], track / 2, track & 1, caps_flags);
    mfm = mfmbuf;
    *revolutions = ci.trackcnt;
    for (j = 0; j < ci.trackcnt; j++) {
	len = ci.tracksize[j];
	trackpointers[j] = mfm;
	tracklengths[j] = len * 8;
	for (i = 0; i < (len + 1) / 2; i++) {
	    uae_u8 *data = ci.trackdata[j]+ i * 2;
	    *mfm++ = 256 * *data + *(data + 1);
	}
    }
#if 0
    {
	FILE *f = fopen ("c:\\1.txt","wb");
	fwrite (ci.trackdata[0], len, 1, f);
	fclose (f);
    }
#endif
    if (ci.timelen > 0) {
	tt = xmalloc (ci.timelen * sizeof (uae_u16));
	for (i = 0; i < ci.timelen; i++)
	    tt[i] = (uae_u16)ci.timebuf[i];
	*tracktiming = tt;
    }
#if 0
    write_log ("caps: drive: %d, track: %d, revolutions: %d, uses timing: %s\n", drv, track, *revolutions, ci.timelen > 0 ? "yes" : "no");
    for (i = 0; i < *revolutions; i++)
	write_log ("- %d: length: %d bits, %d bytes\n", i, tracklengths[i], tracklengths[i] / 8);
#endif
    return 1;
}

#endif
