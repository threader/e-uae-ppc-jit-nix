/*
 * PUAE - The Un*x Amiga Emulator
 *
 * OS-specific memory support functions
 *
 * Copyright 2004 Richard Drummond
 * Copyright 2010 Mustafa Tufan
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "autoconf.h"
#ifndef ANDROID
#include <sys/sysctl.h>
#endif
#include "include/memory.h"

#ifdef JIT

#define BARRIER 32
#define MAXZ3MEM32 0x7F000000
#define MAXZ3MEM64 0xF0000000
#define MAX_SHMID 256

#define IPC_PRIVATE 0x01
#define IPC_RMID    0x02
#define IPC_CREAT   0x04
#define IPC_STAT    0x08

#define MEM_RESERVE 0
#define MEM_COMMIT 0
#define MEM_DECOMMIT 0
#define MEM_WRITE_WATCH 0
#define PAGE_READWRITE 0

#if !defined(__FreeBSD__)
typedef int key_t;
#endif

/* One shmid data structure for each shared memory segment in the system. */
struct shmid_ds {
    key_t  key;
    size_t size;
	size_t rosize;
    void   *addr;
    char  name[MAX_PATH];
    void   *attached;
    int    mode;
    void   *natmembase; /* if != NULL then shmem is shared from natmem */
	bool fake;
};

static struct shmid_ds shmids[MAX_SHMID];
uae_u8 *natmem_offset, *natmem_offset_end;
static uae_u8 *p96mem_offset;
static int p96mem_size;
static uae_u8 *memwatchtable;
int maxmem;
static int memwatchok = 0;
uae_u32 natmem_size;

#include <sys/mman.h>

/*
 * Allocate executable memory for JIT cache
 */
void cache_free (uae_u8 *cache)
{
	free (cache);
}

uae_u8 *cache_alloc (int size)
{
	void *cache;

	size = size < getpagesize() ? getpagesize() : size;

	if ((cache = valloc (size))) {
		if (mprotect (cache, size, PROT_READ|PROT_WRITE|PROT_EXEC)) {
			write_log (_T("MProtect Cache of %d failed. ERR=%d\n"), size, errno);
		}
	} else {
		write_log (_T("Cache_Alloc of %d failed. ERR=%d\n"), size, errno);
	}

	return cache;
}

#ifdef NATMEM_OFFSET
static uae_u32 lowmem (void)
{
	uae_u32 change = 0;
	if (currprefs.z3fastmem_size + currprefs.z3fastmem2_size + currprefs.z3chipmem_size >= 8 * 1024 * 1024) {
		if (currprefs.z3fastmem2_size) {
			change = currprefs.z3fastmem2_size;
			currprefs.z3fastmem2_size = 0;
		} else if (currprefs.z3chipmem_size) {
			if (currprefs.z3chipmem_size <= 16 * 1024 * 1024) {
				change = currprefs.z3chipmem_size;
				currprefs.z3chipmem_size = 0;
			} else {
				change = currprefs.z3chipmem_size / 2;
				currprefs.z3chipmem_size /= 2;
			}
		} else {
			change = currprefs.z3fastmem_size - currprefs.z3fastmem_size / 4;
			currprefs.z3fastmem2_size = changed_prefs.z3fastmem2_size = currprefs.z3fastmem_size / 4;
			currprefs.z3fastmem_size /= 2;
			changed_prefs.z3fastmem_size = currprefs.z3fastmem_size;
		}
	} else if (currprefs.rtgmem_size >= 1 * 1024 * 1024) {
		change = currprefs.rtgmem_size - currprefs.rtgmem_size / 2;
		currprefs.rtgmem_size /= 2;
		changed_prefs.rtgmem_size = currprefs.rtgmem_size;
	}
	if (currprefs.z3fastmem2_size < 128 * 1024 * 1024)
		currprefs.z3fastmem2_size = changed_prefs.z3fastmem2_size = 0;
	return change;
}

static uae_u64 size64;

static void clear_shm (void)
{
	shm_start = NULL;
	for (unsigned int i = 0; i < MAX_SHMID; i++) {
		memset (&shmids[i], 0, sizeof (struct shmid_ds));
		shmids[i].key = -1;
	}
}

bool preinit_shm (void)
{
	int i;
	uae_u64 total64;
	uae_u64 totalphys64;
	uae_u32 max_allowed_mman;

        if (natmem_offset)
                free (natmem_offset);
        natmem_offset = NULL;
        if (p96mem_offset)
                free (p96mem_offset);
        p96mem_offset = NULL;

#ifdef __x86_64__
	max_allowed_mman = 2048;
#else
	max_allowed_mman = 512;
#endif

#ifdef __APPLE__
//xaind
	int mib[2];
	size_t len;

	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE; /* gives a 64 bit int */
	len = sizeof(totalphys64);
	sysctl(mib, 2, &totalphys64, &len, NULL, 0);
	total64 = (uae_u64) totalphys64;
#else
	totalphys64 = sysconf (_SC_PHYS_PAGES) * getpagesize();
	total64 = (uae_u64)sysconf (_SC_PHYS_PAGES) * (uae_u64)getpagesize();
#endif
	size64 = total64;
#ifdef __x86_64__
	if (size64 > MAXZ3MEM64)
		size64 = MAXZ3MEM64;
#else
	if (size64 > MAXZ3MEM32)
		size64 = MAXZ3MEM32;
#endif
	if (maxmem < 0)
		size64 = MAXZ3MEM64;
	else if (maxmem > 0)
		size64 = maxmem * 1024 * 1024;
	if (size64 < 8 * 1024 * 1024)
		size64 = 8 * 1024 * 1024;
	if (max_allowed_mman * 1024 * 1024 > size64)
		max_allowed_mman = size64 / (1024 * 1024);

	natmem_size = max_allowed_mman * 1024 * 1024;
	if (natmem_size < 256 * 1024 * 1024)
		natmem_size = 256 * 1024 * 1024;

	write_log (_T("Total physical RAM %lluM. Attempting to reserve: %uM.\n"), totalphys64 >> 20, natmem_size >> 20);
	natmem_offset = 0;
	if (natmem_size <= 640 * 1024 * 1024) {
		uae_u32 p = 0x78000000 - natmem_size;
		for (;;) {
			natmem_offset = (uae_u8*)valloc (natmem_size);
			if (natmem_offset)
				break;
			p -= 256 * 1024 * 1024;
			if (p <= 256 * 1024 * 1024)
				break;
		}
	}
	if (!natmem_offset) {
		for (;;) {
			natmem_offset = (uae_u8*)valloc (natmem_size);
			if (natmem_offset)
				break;
			natmem_size -= 128 * 1024 * 1024;
			if (!natmem_size) {
				write_log (_T("Can't allocate 256M of virtual address space!?\n"));
				return false;
			}
		}
	}
	max_z3fastmem = natmem_size;
	write_log (_T("Reserved: 0x%p-0x%p (%08x %dM)\n"),
		natmem_offset, (uae_u8*)natmem_offset + natmem_size,
		natmem_size, natmem_size >> 20);

	clear_shm ();

//	write_log (_T("Max Z3FastRAM %dM. Total physical RAM %uM\n"), max_z3fastmem >> 20, totalphys64 >> 20);

	canbang = 1;
	return true;
}

static void resetmem (bool decommit)
{
	int i;

	if (!shm_start)
		return;
	for (i = 0; i < MAX_SHMID; i++) {
		struct shmid_ds *s = &shmids[i];
		int size = s->size;
		uae_u8 *shmaddr;
		uae_u8 *result;

		if (!s->attached)
			continue;
		if (!s->natmembase)
			continue;
		if (s->fake)
			continue;
		if (!decommit && ((uae_u8*)s->attached - (uae_u8*)s->natmembase) >= 0x10000000)
			continue;
		shmaddr = natmem_offset + ((uae_u8*)s->attached - (uae_u8*)s->natmembase);
		result = valloc (/*shmaddr,*/ size);
		if (result != shmaddr)
			write_log ("NATMEM: realloc(%p,%d,%d) failed, err=%x\n", shmaddr, size, s->mode, errno);
		else
			write_log ("NATMEM: rellocated(%p,%d,%s)\n", shmaddr, size, s->name);
	}
}

static ULONG getz2rtgaddr (void)
{
	ULONG start;
	start = currprefs.fastmem_size;
	while (start & (currprefs.rtgmem_size - 1) && start < 4 * 1024 * 1024)
		start += 1024 * 1024;
	return start + 2 * 1024 * 1024;
}

static uae_u8 *va (uae_u32 offset, uae_u32 len, long alloc, long protect)
{
	uae_u8 *addr;

	addr = (uae_u8*)valloc (len);
	if (addr) {
		write_log (_T("VA(%p - %p, %4uM, %s)\n"),
			natmem_offset + offset, natmem_offset + offset + len, len >> 20, (alloc & MEM_WRITE_WATCH) ? _T("WATCH") : _T("RESERVED"));
		return addr;
	}
	write_log (_T("VA(%p - %p, %4uM, %s) failed %d\n"),
		natmem_offset + offset, natmem_offset + offset + len, len >> 20, (alloc & MEM_WRITE_WATCH) ? _T("WATCH") : _T("RESERVED"), errno);
	return NULL;
}

static int doinit_shm (void)
{
	uae_u32 size, totalsize, z3size, natmemsize;
	uae_u32 rtgbarrier, z3chipbarrier, rtgextra;
	int rounds = 0;
	ULONG z3rtgmem_size = changed_prefs.rtgmem_type ? changed_prefs.rtgmem_size : 0;

	for (;;) {
		int lowround = 0;
		uae_u8 *blah = NULL;
		if (rounds > 0)
			write_log (_T("NATMEM: retrying %d..\n"), rounds);
		rounds++;

		z3size = 0;
		size = 0x1000000;
		rtgextra = 0;
		z3chipbarrier = 0;
		rtgbarrier = getpagesize();
		if (changed_prefs.cpu_model >= 68020)
			size = 0x10000000;
		if (changed_prefs.z3fastmem_size || changed_prefs.z3fastmem2_size || changed_prefs.z3chipmem_size) {
			z3size = changed_prefs.z3fastmem_size + changed_prefs.z3fastmem2_size + changed_prefs.z3chipmem_size + (changed_prefs.z3fastmem_start - 0x10000000);
			if (z3rtgmem_size) {
				rtgbarrier = 16 * 1024 * 1024 - ((changed_prefs.z3fastmem_size + changed_prefs.z3fastmem2_size) & 0x00ffffff);
			}
			if (changed_prefs.z3chipmem_size && (changed_prefs.z3fastmem_size || changed_prefs.z3fastmem2_size))
				z3chipbarrier = 16 * 1024 * 1024;
		} else {
			rtgbarrier = 0;
		}
		totalsize = size + z3size + z3rtgmem_size;
		while (totalsize > size64) {
			int change = lowmem ();
			if (!change)
				return 0;
			write_log (_T("NATMEM: %d, %dM > %dM = %dM\n"), ++lowround, totalsize >> 20, size64 >> 20, (totalsize - change) >> 20);
			totalsize -= change;
		}
		if ((rounds > 1 && totalsize < 0x10000000) || rounds > 20) {
			write_log (_T("NATMEM: No special area could be allocated (3)!\n"));
			return 0;
		}
		natmemsize = size + z3size;

		if (z3rtgmem_size) {
			rtgextra = getpagesize();
		} else {
			rtgbarrier = 0;
			rtgextra = 0;
		}
		if (natmemsize + rtgbarrier + z3chipbarrier + z3rtgmem_size + rtgextra + 16 * getpagesize() <= natmem_size)
			break;
		write_log (_T("NATMEM: %dM area failed to allocate, err=%d (Z3=%dM,RTG=%dM)\n"),
			natmemsize >> 20, errno, (changed_prefs.z3fastmem_size + changed_prefs.z3fastmem2_size + changed_prefs.z3chipmem_size) >> 20, z3rtgmem_size >> 20);
		if (!lowmem ()) {
			write_log (_T("NATMEM: No special area could be allocated (2)!\n"));
			return 0;
		}
	}

#if VAMODE == 1

	p96mem_offset = NULL;
	p96mem_size = z3rtgmem_size;
	if (changed_prefs.rtgmem_size && changed_prefs.rtgmem_type) {
		p96mem_offset = natmem_offset + natmemsize + rtgbarrier + z3chipbarrier;
	} else if (changed_prefs.rtgmem_size && !changed_prefs.rtgmem_type) {
		p96mem_offset = natmem_offset + getz2rtgaddr ();
	}

#else

	if (p96mem_offset)
		free (p96mem_offset);
	p96mem_offset = NULL;
	p96mem_size = z3rtgmem_size;
	if (changed_prefs.rtgmem_size && changed_prefs.rtgmem_type) {
		uae_u32 s, l;
		free (natmem_offset);

		s = 0;
		l = natmemsize + rtgbarrier + z3chipbarrier;
		if (!va (s, l, MEM_RESERVE, PAGE_READWRITE))
			return 0;

		s = natmemsize + rtgbarrier + z3chipbarrier;
		l = p96mem_size + rtgextra;
		p96mem_offset = va (s, l, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
		if (!p96mem_offset) {
			currprefs.rtgmem_size = changed_prefs.rtgmem_size = 0;
			z3rtgmem_size = 0;
			write_log (_T("NATMEM: failed to allocate special Picasso96 GFX RAM, err=%d\n"), errno);
		}

#if 0
		s = natmemsize + rtgbarrier + z3chipbarrier + p96mem_size + rtgextra + 4096;
		l = natmem_size - s - 4096;
		if (natmem_size > l) {
			if (!va (s, l, 	MEM_RESERVE, PAGE_READWRITE))
				return 0;
		}
#endif

	} else if (changed_prefs.rtgmem_size && !changed_prefs.rtgmem_type) {

		uae_u32 s, l;
		free (natmem_offset);
		// Chip + Z2Fast
		s = 0;
		l = 2 * 1024 * 1024 + changed_prefs.fastmem_size;
		if (!va (s, l, MEM_RESERVE, PAGE_READWRITE)) {
			currprefs.rtgmem_size = changed_prefs.rtgmem_size = 0;
		}
		// After RTG
		s = 2 * 1024 * 1024 + 8 * 1024 * 1024;
		l = natmem_size - (2 * 1024 * 1024 + 8 * 1024 * 1024) + getpagesize();
		if (!va (s, l, MEM_RESERVE, PAGE_READWRITE)) {
			currprefs.rtgmem_size = changed_prefs.rtgmem_size = 0;
		}
		// RTG
		s = getz2rtgaddr ();
		l = 10 * 1024 * 1024 - getz2rtgaddr ();
		p96mem_offset = va (s, l, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
		if (!p96mem_offset) {
			currprefs.rtgmem_size = changed_prefs.rtgmem_size = 0;
		}

	} else {

		free (natmem_offset);
		if (!valloc (natmem_size)) {
			write_log (_T("NATMEM: No special area could be reallocated! (1) err=%d\n"), errno);
			return 0;
		}
	}
#endif
	if (!natmem_offset) {
		write_log (_T("NATMEM: No special area could be allocated! err=%d\n"), errno);
	} else {
		write_log (_T("NATMEM: Our special area: 0x%p-0x%p (%08x %dM)\n"),
			natmem_offset, (uae_u8*)natmem_offset + natmemsize,
			natmemsize, natmemsize >> 20);
		if (changed_prefs.rtgmem_size)
			write_log (_T("NATMEM: P96 special area: 0x%p-0x%p (%08x %dM)\n"),
			p96mem_offset, (uae_u8*)p96mem_offset + changed_prefs.rtgmem_size,
			changed_prefs.rtgmem_size, changed_prefs.rtgmem_size >> 20);
		canbang = 1;
		if (p96mem_size)
			natmem_offset_end = p96mem_offset + p96mem_size;
		else
			natmem_offset_end = natmem_offset + natmemsize;
	}

	return canbang;
}

bool init_shm (void)
{
	static uae_u32 oz3fastmem_size, oz3fastmem2_size;
	static uae_u32 oz3chipmem_size;
	static uae_u32 ortgmem_size;
	static int ortgmem_type;

	if (
		oz3fastmem_size == changed_prefs.z3fastmem_size &&
		oz3fastmem2_size == changed_prefs.z3fastmem2_size &&
		oz3chipmem_size == changed_prefs.z3chipmem_size &&
		ortgmem_size == changed_prefs.rtgmem_size &&
		ortgmem_type == changed_prefs.rtgmem_type)
		return false;

	oz3fastmem_size = changed_prefs.z3fastmem_size;
	oz3fastmem2_size = changed_prefs.z3fastmem2_size;
	oz3chipmem_size = changed_prefs.z3chipmem_size;;
	ortgmem_size = changed_prefs.rtgmem_size;
	ortgmem_type = changed_prefs.rtgmem_type;

	doinit_shm ();

	resetmem (false);
	clear_shm ();

	memory_hardreset (2);
	return true;
}

void free_shm (void)
{
	resetmem (true);
	clear_shm ();
}

void mapped_free (uae_u8 *mem)
{
	shmpiece *x = shm_start;

	if (mem == filesysory) {
		while(x) {
			if (mem == x->native_address) {
				int shmid = x->id;
				shmids[shmid].key = -1;
				shmids[shmid].name[0] = '\0';
				shmids[shmid].size = 0;
				shmids[shmid].attached = 0;
				shmids[shmid].mode = 0;
				shmids[shmid].natmembase = 0;
			}
			x = x->next;
		}
		return;
	}

	while(x) {
		if(mem == x->native_address)
			my_shmdt (x->native_address);
		x = x->next;
	}
	x = shm_start;
	while(x) {
		struct shmid_ds blah;
		if (mem == x->native_address) {
			if (my_shmctl (x->id, IPC_STAT, &blah) == 0)
				my_shmctl (x->id, IPC_RMID, &blah);
		}
		x = x->next;
	}
}

static key_t get_next_shmkey (void)
{
	key_t result = -1;
	int i;
	for (i = 0; i < MAX_SHMID; i++) {
		if (shmids[i].key == -1) {
			shmids[i].key = i;
			result = i;
			break;
		}
	}
	return result;
}

STATIC_INLINE key_t find_shmkey (key_t key)
{
	int result = -1;
	if(shmids[key].key == key) {
		result = key;
	}
	return result;
}

void *my_shmat (int shmid, void *shmaddr, int shmflg)
{
	void *result = (void *)-1;
	unsigned int got = FALSE, readonly = FALSE;
	int p96special = FALSE;

//#ifdef NATMEM_OFFSET
	unsigned int size = shmids[shmid].size;
	unsigned int readonlysize = size;

	if (shmids[shmid].attached)
		return shmids[shmid].attached;

	if ((uae_u8*)shmaddr < natmem_offset) {
		if(!_tcscmp (shmids[shmid].name, _T("chip"))) {
			shmaddr=natmem_offset;
			got = TRUE;
			if (getz2endaddr () <= 2 * 1024 * 1024 || currprefs.chipmem_size < 2 * 1024 * 1024)
				size += BARRIER;
		} else if(!_tcscmp (shmids[shmid].name, _T("kick"))) {
			shmaddr=natmem_offset + 0xf80000;
			got = TRUE;
			size += BARRIER;
			readonly = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("rom_a8"))) {
			shmaddr=natmem_offset + 0xa80000;
			got = TRUE;
			readonly = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("rom_e0"))) {
			shmaddr=natmem_offset + 0xe00000;
			got = TRUE;
			readonly = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("rom_f0"))) {
			shmaddr=natmem_offset + 0xf00000;
			got = TRUE;
			readonly = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("rtarea"))) {
			shmaddr=natmem_offset + rtarea_base;
			got = TRUE;
			readonly = TRUE;
			readonlysize = RTAREA_TRAPS;
		} else if(!_tcscmp (shmids[shmid].name, _T("fast"))) {
			shmaddr=natmem_offset + 0x200000;
			got = TRUE;
			if (!(currprefs.rtgmem_size && !currprefs.rtgmem_type))
				size += BARRIER;
		} else if(!_tcscmp (shmids[shmid].name, _T("z2_gfx"))) {
			ULONG start = getz2rtgaddr ();
			got = TRUE;
			p96special = TRUE;
			shmaddr = natmem_offset + start;
			p96ram_start = start;
			if (start + currprefs.rtgmem_size < 10 * 1024 * 1024)
				size += BARRIER;
		} else if(!_tcscmp (shmids[shmid].name, _T("ramsey_low"))) {
			shmaddr=natmem_offset + a3000lmem_start;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("ramsey_high"))) {
			shmaddr=natmem_offset + a3000hmem_start;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("z3"))) {
			shmaddr=natmem_offset + z3fastmem_start;
			if (!currprefs.z3fastmem2_size)
				size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("z3_2"))) {
			shmaddr=natmem_offset + z3fastmem_start + currprefs.z3fastmem_size;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("z3_chip"))) {
			shmaddr=natmem_offset + z3chipmem_start;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("z3_gfx"))) {
			got = TRUE;
			p96special = TRUE;
			p96ram_start = p96mem_offset - natmem_offset;
			shmaddr = natmem_offset + p96ram_start;
			size += BARRIER;
		} else if(!_tcscmp (shmids[shmid].name, _T("bogo"))) {
			shmaddr=natmem_offset+0x00C00000;
			got = TRUE;
			if (currprefs.bogomem_size <= 0x100000)
				size += BARRIER;
		} else if(!_tcscmp (shmids[shmid].name, _T("filesys"))) {
			static uae_u8 *filesysptr;
			if (filesysptr == NULL)
				filesysptr = xcalloc (uae_u8, size);
			result = filesysptr;
			shmids[shmid].attached = result;
			shmids[shmid].fake = true;
			return result;
		} else if(!_tcscmp (shmids[shmid].name, _T("custmem1"))) {
			shmaddr=natmem_offset + currprefs.custom_memory_addrs[0];
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("custmem2"))) {
			shmaddr=natmem_offset + currprefs.custom_memory_addrs[1];
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("hrtmem"))) {
			shmaddr=natmem_offset + 0x00a10000;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("arhrtmon"))) {
			shmaddr=natmem_offset + 0x00800000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("xpower_e2"))) {
			shmaddr=natmem_offset + 0x00e20000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("xpower_f2"))) {
			shmaddr=natmem_offset + 0x00f20000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("nordic_f0"))) {
			shmaddr=natmem_offset + 0x00f00000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("nordic_f4"))) {
			shmaddr=natmem_offset + 0x00f40000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("nordic_f6"))) {
			shmaddr=natmem_offset + 0x00f60000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp(shmids[shmid].name, _T("superiv_b0"))) {
			shmaddr=natmem_offset + 0x00b00000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("superiv_d0"))) {
			shmaddr=natmem_offset + 0x00d00000;
			size += BARRIER;
			got = TRUE;
		} else if(!_tcscmp (shmids[shmid].name, _T("superiv_e0"))) {
			shmaddr=natmem_offset + 0x00e00000;
			size += BARRIER;
			got = TRUE;
		}
	}
//#endif

	if (shmids[shmid].key == shmid && shmids[shmid].size) {
		//DWORD protect = readonly ? PAGE_READONLY : PAGE_READWRITE;
		unsigned int protect = 0;
		shmids[shmid].mode = protect;
		shmids[shmid].rosize = readonlysize;
		shmids[shmid].natmembase = natmem_offset;
		if (shmaddr)
			free (shmaddr);
		result = valloc (size);
		if (result == NULL)
			free (shmaddr);
		result = valloc (size);
		if (result == NULL) {
			result = (void*)-1;
			write_log (_T("VA %08X - %08X %x (%dk) failed %d\n"),
				(uae_u8*)shmaddr - natmem_offset, (uae_u8*)shmaddr - natmem_offset + size,
				size, size >> 10, errno);
		} else {
			shmids[shmid].attached = result;
			write_log (_T("VA %08X - %08X %x (%dk) ok (%08X)%s\n"),
				(uae_u8*)shmaddr - natmem_offset, (uae_u8*)shmaddr - natmem_offset + size,
				size, size >> 10, shmaddr, p96special ? _T(" P96") : _T(""));
		}
	}
	return result;
}

void protect_roms (bool protect)
{
	struct shmid_ds *shm;
	
	if (!currprefs.cachesize || currprefs.comptrustbyte || currprefs.comptrustword || currprefs.comptrustlong)
		return;
	for (int i = 0; i < MAX_SHMID; i++) {
		long old;
		shm = &shmids[i];
/*		if (shm->mode != PAGE_READONLY)
			continue;
		if (!VirtualProtect (shm->attached, shm->rosize, protect ? PAGE_READONLY : PAGE_READWRITE, &old)) {
			write_log (_T("VirtualProtect %08X - %08X %x (%dk) failed %d\n"),
				(uae_u8*)shm->attached - natmem_offset, (uae_u8*)shm->attached - natmem_offset + shm->size,
				shm->size, shm->size >> 10, errno);
		}*/
	}
}

int my_shmdt (const void *shmaddr)
{
	return 0;
}

int my_shmget (key_t key, size_t size, int shmflg, const char *name)
{
	int result = -1;

//	write_log ("key %d (%d), size %d, shmflg %d, name %s\n", key, IPC_PRIVATE, size, shmflg, name);
//	if((key == IPC_PRIVATE) || ((shmflg & IPC_CREAT) && (find_shmkey (key) == -1))) {
		//write_log ("shmget of size %d (%dk) for %s\n", size, size >> 10, name);
		if ((result = get_next_shmkey ()) != -1) {
			shmids[result].size = size;
			_tcscpy (shmids[result].name, name);
		} else {
			result = -1;
		}
//	}
	return result;
}

int my_shmctl (int shmid, int cmd, struct shmid_ds *buf)
{
	int result = -1;

	if ((find_shmkey (shmid) != -1) && buf) {
		switch (cmd)
		{
		case IPC_STAT:
			*buf = shmids[shmid];
			result = 0;
			break;
		case IPC_RMID:
			// shmem was not shared from natmem but allocated -> so free it now
			if (shmids[shmid].natmembase == NULL) {
				free (shmids[shmid].attached);
			}
			shmids[shmid].key = -1;
			shmids[shmid].name[0] = '\0';
			shmids[shmid].size = 0;
			shmids[shmid].attached = 0;
			shmids[shmid].mode = 0;
			shmids[shmid].natmembase = NULL;
			result = 0;
			break;
		}
	}
	return result;
}

#endif //NATMEM_OFFSET
#endif //JIT
