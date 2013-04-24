#include <machine/cpu_capabilities.h>

/* void sys_icache_invalidate(char *start, long len) */

	.text
	.globl  _sys_icache_invalidate
	.align  2
_sys_icache_invalidate:
	ba      _COMM_PAGE_FLUSH_ICACHE



/* void sys_dcache_flush(char *start, long len) */

	.text
	.globl  _sys_dcache_flush
	.align  2
_sys_dcache_flush:
	ba      _COMM_PAGE_FLUSH_DCACHE

