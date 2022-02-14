
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern void S2X_refresh (void);
extern void S2X_render (void);
extern void S2X_init (int dw, int dh, int aw, int ah, int depth, int scale);

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef int bool;

extern void S2X_configure (int rb, int gb, int bb, int rs, int gs, int bs);
extern int Init_2xSaI(int rb, int gb, int bb, int rs, int gs, int bs);
extern void Super2xSaI(const uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
extern void SuperEagle(const uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);
extern void _2xSaI(const uint8 *srcPtr, uint32 srcPitch, uint8 *dstPtr, uint32 dstPitch, int width, int height);

#define UAE_FILTER_NULL 1
#define UAE_FILTER_DIRECT3D 2
#define UAE_FILTER_OPENGL 3
#define UAE_FILTER_SCALE2X 4
#define UAE_FILTER_SUPEREAGLE 5
#define UAE_FILTER_SUPER2XSAI 6
#define UAE_FILTER_2XSAI 7
