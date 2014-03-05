 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68881 emulation
  *
  * Conversion routines for hosts with unknown floating point format.
  *
  * Copyright 1996 Herman ten Brugge
  */

#define FPCR_ROUNDING_MODE      0x00000030
#define FPCR_ROUND_NEAR         0x00000000
#define FPCR_ROUND_ZERO         0x00000010
#define FPCR_ROUND_MINF         0x00000020
#define FPCR_ROUND_PINF         0x00000030

#define FPCR_ROUNDING_PRECISION 0x000000c0
#define FPCR_PRECISION_SINGLE   0x00000040
#define FPCR_PRECISION_DOUBLE   0x00000080
#define FPCR_PRECISION_EXTENDED 0x00000000

static double twoto32 = 4294967296.0;

#ifndef HAVE_to_single
STATIC_INLINE double to_single (uae_u32 value)
{
    double frac;

    if ((value & 0x7fffffff) == 0)
		return (0.0);
    frac = (double) ((value & 0x7fffff) | 0x800000) / 8388608.0;
    if (value & 0x80000000)
		frac = -frac;
    return (ldexp (frac, ((value >> 23) & 0xff) - 127));
}
#endif

#ifndef HAVE_from_single
STATIC_INLINE uae_u32 from_single (double src)
{
    int expon;
    uae_u32 tmp;
    double frac;

    if (src == 0.0)
		return 0;
    if (src < 0) {
		tmp = 0x80000000;
		src = -src;
    } else {
		tmp = 0;
    }
    frac = frexp (src, &expon);
    frac += 0.5 / 16777216.0;
    if (frac >= 1.0) {
		frac /= 2.0;
		expon++;
    }
    return (tmp | (((expon + 127 - 1) & 0xff) << 23) |
	    (((int) (frac * 16777216.0)) & 0x7fffff));
}
#endif

#ifndef HAVE_to_exten
STATIC_INLINE void to_exten(fpdata *fpd, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
	double frac;

#ifdef USE_SOFT_LONG_DOUBLE
	fpd->fpe = ((uae_u64)wrd2 << 32) | wrd3;
	fpd->fpm = wrd1;
	fpd->fpx = true;
#endif
	if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0) {
		fpd->fp = 0.0;
		return;
	}
	frac = ((double)wrd2 + ((double)wrd3 / twoto32)) / 2147483648.0;
	if (wrd1 & 0x80000000)
		frac = -frac;

	fpd->fp = ldexp (frac, ((wrd1 >> 16) & 0x7fff) - 16383);
}
#endif

#ifndef HAVE_from_exten
STATIC_INLINE void from_exten(fpdata *fpd, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
	int expon;
	double frac;

#ifdef USE_SOFT_LONG_DOUBLE
	if (fpd->fpx) {
		*wrd1 = fpd->fpm;
		*wrd2 = fpd->fpe >> 32;
		*wrd3 = (uae_u32)fpd->fpe;
	} else
#endif
	{
		if (fpd->fp == 0.0) {
			*wrd1 = 0;
			*wrd2 = 0;
			*wrd3 = 0;
			return;
		}
		if (fpd->fp < 0) {
			*wrd1 = 0x80000000;
			fpd->fp = -fpd->fp;
		} else {
			*wrd1 = 0;
		}
		frac = frexp (fpd->fp, &expon);
		frac= 0.5 / (twoto32 * twoto32);
		if (frac >= 1.0) {
			frac /= 2.0;
			expon++;
		}
		*wrd1 |= (((expon + 16383 - 1) & 0x7fff) << 16);
		*wrd2 = (uae_u32) (frac * twoto32);
		*wrd3 = (uae_u32) ((frac * twoto32 - *wrd2) * twoto32);
	}
}
#endif

#ifndef HAVE_to_double
STATIC_INLINE double to_double(uae_u32 wrd1, uae_u32 wrd2)
{
    double frac;

    if ((wrd1 & 0x7fffffff) == 0 && wrd2 == 0)
		return 0.0;
    frac = (double) ((wrd1 & 0xfffff) | 0x100000) / 1048576.0 +
	(double) wrd2 / 4503599627370496.0;
    if (wrd1 & 0x80000000)
		frac = -frac;
    return ldexp (frac, ((wrd1 >> 20) & 0x7ff) - 1023);
}
#endif

#ifndef HAVE_from_double
STATIC_INLINE void from_double(double src, uae_u32 * wrd1, uae_u32 * wrd2)
{
    int expon;
    int tmp;
    double frac;

    if (src == 0.0) {
		*wrd1 = 0;
		*wrd2 = 0;
		return;
    }
    if (src < 0) {
		*wrd1 = 0x80000000;
		src = -src;
    } else {
		*wrd1 = 0;
    }
    frac = frexp (src, &expon);
    frac += 0.5 / 9007199254740992.0;
    if (frac >= 1.0) {
		frac /= 2.0;
		expon++;
    }
    tmp = (uae_u32) (frac * 2097152.0);
    *wrd1 |= (((expon + 1023 - 1) & 0x7ff) << 20) | (tmp & 0xfffff);
    *wrd2 = (uae_u32) (frac * 9007199254740992.0 - tmp * 4294967296.0);
}
#endif
