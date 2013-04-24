/* ===-- OSCacheControl.h - Interface cache flush functions ----------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 */

#ifndef	__OS_CACHHE_CONTROL_H__
#define	__OS_CACHHE_CONTROL_H__

#include <stddef.h>		/* size_t */
#include <sys/cdefs.h>

__BEGIN_DECLS

/* we will provide assembly from darwin9 sources */
/* perform one of the above cache functions: */

/** Prepare memory for execution.  This should be called
 * after writing machine instructions to memory, before
 * executing them.  It syncs the dcache and icache.
 * On IA32 processors this function is a NOP, because
 * no synchronization is required.
 */
#define kCacheFunctionPrepareForExecution       1

/* Flush data cache(s).  This ensures that cached data 
 * makes it all the way out to DRAM, and then removes
 * copies of the data from all processor caches.
 * It can be useful when dealing with cache incoherent
 * devices or DMA.
 */
#define kCacheFunctionFlushDcache       2

extern
int     sys_cache_control( int function, void *start, size_t len);

/* equivalent to sys_cache_control(kCacheFunctionPrepareForExecution): */
extern
void    sys_icache_invalidate( void *start, size_t len);

/* equivalent to sys_cache_control(kCacheFunctionFlushDcache): */
extern
void    sys_dcache_flush( void *start, size_t len);

__END_DECLS

#endif	/* __OS_CACHHE_CONTROL_H__ */
