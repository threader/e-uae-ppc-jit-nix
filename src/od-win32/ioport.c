
#include "sysconfig.h"
#include "sysdeps.h"

//#define IOPORT_EMU
#define io_log
//#define io_log write_log

typedef int bool;

#include <windows.h>
#include <winio.h>

static bool initialized;

int ioport_init (void)
{
    if (initialized)
	return 1;
#ifndef IOPORT_EMU
    initialized = InitializeWinIo();
#else
    initialized = 1;
#endif
    io_log ("io initialize returned %d\n", initialized);
    return initialized;
}

void ioport_free (void)
{
#ifndef IOPORT_EMU
    if (initialized)
	ShutdownWinIo();
#endif
    io_log ("io freed\n");
    initialized = 0;
}

uae_u8 ioport_read (int port)
{
    DWORD v = 0;
#ifndef IOPORT_EMU
    GetPortVal (port, &v, 1);
#endif
    io_log ("ioport_read %04.4X returned %02.2X\n", port, v);
    return (uae_u8)v;
}

void ioport_write (int port, uae_u8 v)
{
#ifndef IOPORT_EMU
    SetPortVal (port, v, 1);
#endif
    io_log ("ioport_write %04.4X %02.2X\n", port, v);
}

