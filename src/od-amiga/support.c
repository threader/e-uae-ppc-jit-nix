 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Misc support code for AmigaOS target
  *
  * Copyright 2003-2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <exec/exec.h>
#include <exec/ports.h>
#include <exec/lists.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>

#include "threaddep/thread.h"
#include "custom.h"
#include "events.h"
#include "sleep.h"

int truncate (const char *path, off_t length)
{
   /* FIXME */
   return 0;
}

#ifdef HAVE_OSDEP_RPT
struct Device *TimerBase;

static struct MsgPort   *timer_msgport;
static struct IORequest *timer_ioreq;

void osdep_close_timer (void)
{
    if (timer_ioreq) {
	if (TimerBase)
	    CloseDevice (timer_ioreq);
	DeleteIORequest (timer_ioreq);
    }
    if (timer_msgport)
	DeleteMsgPort (timer_msgport);
}

void osdep_open_timer (void)
{
    timer_msgport = CreateMsgPort();
    timer_ioreq   = CreateIORequest (timer_msgport, sizeof *timer_ioreq);

    if (timer_ioreq && !OpenDevice ("timer.device", 0, timer_ioreq, NULL)) {
	struct EClockVal etime;

	TimerBase = timer_ioreq->io_Device;
	atexit (osdep_close_timer);
	syncbase  = ReadEClock (&etime);
	rpt_available = 1;

	write_log ("timer.device opened\n");
	sleep_test ();
    } else {
	/* This should never happen */
	osdep_close_timer ();
	write_log ("Warning: failed to open timer.device\n");
    }
}
#endif
