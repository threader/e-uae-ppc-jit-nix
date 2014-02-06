/*
 * UAE - The Un*x Amiga Emulator
 *
 * SANAII compatible network driver emulation
 *
 * (c) 2007 Toni Wilen
 */

#pragma once
#ifndef SANA2_H
#define SANA2_H

uaecptr netdev_startup (uaecptr resaddr);
void netdev_install (void);
void netdev_reset (void);
void netdev_start_threads (void);

#endif // SANA2_H
