/*
 * UAE - The Un*x Amiga Emulator
 *
 * uaeserial.device
 *
 * (c) 2006 Toni Wilen
 */

#pragma once
#ifndef UAESERIAL_H
#define UAESERIAL_H

uaecptr uaeserialdev_startup (uaecptr resaddr);
void uaeserialdev_install (void);
void uaeserialdev_reset (void);
void uaeserialdev_start_threads (void);

extern int log_uaeserial;

#endif // UAESERIAL_H
