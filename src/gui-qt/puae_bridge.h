#ifndef PUAE_BRIDGE_H
#define PUAE_BRIDGE_H

#include <stdio.h>
#define TCHAR char

typedef unsigned char  uae_u8;
typedef signed char    uae_s8;

typedef unsigned int   uae_u16;
typedef int            uae_s16;

typedef unsigned long  uae_u32;
typedef long           uae_s32;

#define MAX_DPATH       512
#define STATIC_INLINE static __inline__

extern "C" {
extern void inputdevice_updateconfig (struct uae_prefs *prefs);
}
extern void read_rom_list (void);

#endif // PUAE_BRIDGE_H
