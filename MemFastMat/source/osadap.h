#ifndef __OSADAP_H__
#define __OSADAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#define	PART_SIZE	(	 0x1000 )	/*   4k: Max size of buddy unit		      */
#define	PART_MASK	(PART_SIZE-1)	/* 4k-1: Mask for alignment				  */

extern void		SM_ConfigParam	(void);
extern void		SM_InitPool		(void **ppaddr, size_t asize);
extern void		SM_MacUpdate	(size_t asize, int poolNo, int mode);
extern void *	SM_Fast_Alloc	(size_t osize, UINT08 vpid);
extern void		SM_Fast_Free	(void *buf, UINT32 caller, UINT08 ppid);

extern void *	Malloc			(size_t len);
extern void 	Free			(void *addr);
extern char *	_getenv			(const char *pEnv);
extern char *	_strsep			(char **s, const char *ct);
#ifndef _CYG_WIN_
extern void	*	mmap			(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
extern int		munmap			(void *addr, size_t len);
#endif
extern void		HEXDUMP			(const char *name, void *vcp, int width, int size);
extern void		hexdump			(const char *name, void *vcp, int size);
extern int		rprint0n		(const char *format , ...);
extern int		rprint1n		(const char *format , ...);
extern int		dbgprint		(const char *format , ...);

extern char * 	pENV_List[];

#ifdef __cplusplus
}
#endif

#endif/*__OSADAP_H__*/
