#pragma once
#ifndef SRC_INCLUDE_UAESERIAL_H_INCLUDED
#define SRC_INCLUDE_UAESERIAL_H_INCLUDED 1

 /*
  * UAE - The Un*x Amiga Emulator
  *
  * uaeserial.device
  *
  * (c) 2006 Toni Wilen
  */

uaecptr uaeserialdev_startup (uaecptr resaddr);
void uaeserialdev_install (void);
void uaeserialdev_reset (void);
void uaeserialdev_start_threads (void);

extern int log_uaeserial;

#ifdef _WIN32
struct uaeserialdata
{
    void *handle;
    void *writeevent;
};
#endif

#endif // SRC_INCLUDE_UAESERIAL_H_INCLUDED
