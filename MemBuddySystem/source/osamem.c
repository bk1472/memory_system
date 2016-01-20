#include "common.h"
#include "osadap.h"

/*-----------------------------------------------------------------------------
	전역 제어 상수
	(Global Control Constant)
------------------------------------------------------------------------------*/
#define BUDDY_DEBUG				1
#define SM_LOG_ALLOC			0	/* must be 0 ins cygwin & VS mode */
#define SM_FIND_BEST_FIT
#define SM_SORTED_FLIST

#ifdef  SM_SORTED_FLIST
# undef SM_FIND_BEST_FIT
#endif

/*-----------------------------------------------------------------------------
	상수 정의
	(Constant Definitions)
------------------------------------------------------------------------------*/
#define SM_PHYS_POOLS			 4		/* No. of real pools(FMAT,Buddy1,Buddy2)  */
#define	SM_VIRT_POOLS			 8		/* No. of virtual pools					  */
#define	SM_FMAT_POOL			 0		/* Pool Id for Fast Mat					  */
#define	SM_SBDY_POOL			 1		/* Pool Id for System buddy				  */
#define	SM_SDEC_POOL			 2		/* Pool Id for SDEC buddy				  */
#define	SM_MMAP_POOL			 3		/* Pool Id for memories from mmap()		  */
#define	SM_VOID_POOL			63		/* Invalid Pool Id (6bit)				  */


#define BDY_BASE_INDEX			 9		/* (1 << 9) == 512                        */
#define BDY_MIN_INDEX			 0		/* Minimum buddy size is  128 byte        */
#define BDY_MAX_INDEX			20		/* Maximum buddy size is 128M byte.       */

#define SM_FLAG_FHEAD			 0		/* Memory is head of free partition		  */
#define SM_FLAG_UHEAD			 1		/* Memory is head of used partition		  */
#define SM_FLAG_CHILD			 2		/* Memory is child of partition			  */
#define SM_FLAG_BUDDY			 3		/* Memory is divided into small buddy	  */

#define DEF_AMASK		( (1<<10)-1 )	/* Default align mask of buddy pool :0x3FF(1023) */
#define PART_SIZE		(    0x1000 )	/*   4k: Max size of buddy unit		             */
#define PART_MASK		(PART_SIZE-1)	/* 4k-1: Mask for alignment				         */
#define OVERHEAD		(sizeof(BuddyHead_t))

/*-----------------------------------------------------------------------------
	메크로 함수 정의
	(Macro Function Definitions)
------------------------------------------------------------------------------*/
#if		(ENDIAN_TYPE != LITTLE_ENDIAN)
#define BUD_ISUSED(p,x)			((gBdyUsed[p][x.s.byteNo] &   (1<<x.s.bitNo)) != 0)
#define CLEAR_USED(p,x)			( gBdyUsed[p][x.s.byteNo] &= ~(1<<x.s.bitNo))
#define SET_USED(p,x)			( gBdyUsed[p][x.s.byteNo] |=  (1<<x.s.bitNo))
#else
#define BUD_ISUSED(p,x)			((gBdyUsed[p][x.u / 32  ] &   (1<<(x.u% 32))) != 0)
#define CLEAR_USED(p,x)			( gBdyUsed[p][x.u / 32  ] &= ~(1<<(x.u% 32)))
#define SET_USED(p,x)			( gBdyUsed[p][x.u / 32  ] |=  (1<<(x.u% 32)))
#endif	/* (ENDIAN_TYPE != LITTLE_ENDIAN) */

#define	BUD_OFFSET(_i,_p)		((UINT32)_p - gPoolBase[_i])
#define	OFS2BOFS(p,x,y,z)		(x.u = (gBdyBlks[p][(y)] + ((z)>>((y)+BDY_BASE_INDEX))))

#define Dptr2Hptr(_i, _p)		&gPartHdr[_i][(((UINT32)(_p)-gPoolBase[_i])/PART_SIZE)]
#define Hptr2Dptr(_i, _p)		(gPoolBase[_i] + PART_SIZE*((_p)-gPartHdr[_i]))

/*-----------------------------------------------------------------------------
	형 정의
	(Type Definitions)
------------------------------------------------------------------------------*/
typedef	struct BuddyHead
{
	struct	BuddyHead *next;	/* 0x00: Next free cell					*/
	struct	BuddyHead *prev;	/* 0x04: Prev free cell					*/
	UINT32	bd_asize;			/* 0x08: UnitSize or # of free cells	*/
	UINT32	bd_osize;			/* 0x0c: Org size of reclaimed memory	*/
	UINT16	bd_task;			/* 0x10: Id of task which owns it		*/
	UINT08	bd_flags;			/* 0x12: Type of memory pool			*/
	UINT08	bd_mProt  : 2,		/* 0x13: Protect memory over/underrun 	*/
	      	bd_vpid   : 6;		/* 0x13: Virutal pool id for this buddy	*/
} BuddyHead_t;

typedef	struct BuddyDHdr
{
	struct	BuddyHead *next;	/* Next free cell						*/
	struct	BuddyHead *prev;	/* Prev free cell						*/
} BuddyDHdr_t;

#if		(ENDIAN_TYPE != LITTLE_ENDIAN)
/*	Structure to present offset position from buddy base				*/
typedef struct _buddyOffsetSub
{
	UINT32	byteNo : 27,
			bitNo  :  5;
} bOffsetSub_t;
#endif	/* (ENDIAN_TYPE != LITTLE_ENDIAN) */

/*	Structure to present offset position from buddy base				*/
typedef union _buddyOffset
{
	UINT32			u;
	#if		(ENDIAN_TYPE != LITTLE_ENDIAN)
	bOffsetSub_t	s;
	#endif	/* (ENDIAN_TYPE != LITTLE_ENDIAN) */
} bOffset_t;


typedef BuddyDHdr_t	BuddyList_t[BDY_MAX_INDEX+1];
typedef UINT32		BuddyBlks_t[BDY_MAX_INDEX+1];

/*-----------------------------------------------------------------------------
	Static 전역 변수 정의
	(Static Global Variables Declaration)
------------------------------------------------------------------------------*/
static UINT32 __RUT[]=				// Round Up Table
{									// NO_LOG_MODE	LOG_MODE(Half size)
	1<<(BDY_BASE_INDEX+ 0),			//	  512b 	    128b
	1<<(BDY_BASE_INDEX+ 1),			//      1K      256b
	1<<(BDY_BASE_INDEX+ 2),			//      2K      512b
	1<<(BDY_BASE_INDEX+ 3),			//	    4K        1K
	1<<(BDY_BASE_INDEX+ 4),			//	    8K        2K
	1<<(BDY_BASE_INDEX+ 5),			//	   16K        4K
	1<<(BDY_BASE_INDEX+ 6),			//	   32K        8K
	1<<(BDY_BASE_INDEX+ 7),			//	   64K       16K
	1<<(BDY_BASE_INDEX+ 8),			//	  128K       32K
	1<<(BDY_BASE_INDEX+ 9),			//	  256K       64K
	1<<(BDY_BASE_INDEX+10),			//    512K      128K
	1<<(BDY_BASE_INDEX+11),			//      1M      256K
	1<<(BDY_BASE_INDEX+12),			//      2M      512K
	1<<(BDY_BASE_INDEX+13),			//	    4M        1M
	1<<(BDY_BASE_INDEX+14),			//	    8M        2M
	1<<(BDY_BASE_INDEX+15),			//	   16M        4M
	1<<(BDY_BASE_INDEX+16),			//	   32M        8M
	1<<(BDY_BASE_INDEX+17),			//	   64M       16M
	1<<(BDY_BASE_INDEX+18),			//	  128M       32M
	1<<(BDY_BASE_INDEX+19),			//	  512M       64M
	1<<(BDY_BASE_INDEX+20),			//	   	1G      128M
	0xFFFFFFFF						// Upper Limit
};

#if defined(SM_SORTED_FLIST)
static void SM_InsertToSortedList(int ppid, int end, BuddyHead_t *pBdyIns);
#endif

/* Memory Information per virtual pool	*/
static BuddyHead_t	gAllocHdr[SM_VIRT_POOLS];			// Buddy Allocation List Header
/* Memory Information per real pool		*/
static BuddyList_t	gFreeHdr [SM_PHYS_POOLS];			// Buddy Free List Header
static BuddyBlks_t	gBdyBlks [SM_PHYS_POOLS];			// Num of blocks in buddy
static BuddyHead_t	*gPartHdr[SM_PHYS_POOLS];			// External header for partition
static UINT32		gPartCnt [SM_PHYS_POOLS];			// Number of external partitions.

static UINT32		*gBdyUsed[SM_PHYS_POOLS] = { NULL };// Used bit for buddy
static UINT32		gPoolBase[SM_PHYS_POOLS] = { 0 };	// Start Address of Memory Pool
static UINT32		gPoolEnd1[SM_PHYS_POOLS] = { 0 };	// End   Address of Memory Pool
static UINT32		gPoolEnd2[SM_PHYS_POOLS] = { 0 };	// End   Address of Memory Pool
static UINT32		maxIndex [SM_PHYS_POOLS] = { 0 };	// Detected maximum index in buddy.
static UINT32		partIndex[SM_PHYS_POOLS] = { 0 };	// Start index of partition
static UINT32		nFreePool[SM_PHYS_POOLS] = { 0 };	// Total size of freemem in Buddy

/* Memory Information per system 		*/
static UINT32		gnAllocs   = 0;						/* Total number of Allocations		*/
static UINT32		gnFrees    = 0;						/* Total number of Frees			*/
static UINT32		gnASrchs   = 0;						/* Total allocation searches		*/
static UINT32		gnFSrchs   = 0;						/* Total free searches				*/
static UINT32		nFreeTot   = 0;						/* Total size of free memory		*/
static UINT32		memASize   = 0;						/* Total size of allocated memory	*/
static UINT32		memOSize   = 0;						/* Total size of requested memory	*/


__inline	UINT32 get_log2(UINT32 x)
{
	register UINT32 i=0;
	while(x > __RUT[i++])
		;
	return i-1;
}

static void
SM_BuddyInit(UINT08 ppid, char *base_addr, size_t pool_size)
{
	BuddyHead_t	*pBdyInit, *pBdyBase;
	UINT32		i, asize, total_blks, bIndex;
	UINT32		nMinSzBlks, nUsedBitSz;

	/* Round up Buddy Base Address by default align value	*/
	gPoolBase[ppid] = (((UINT32)base_addr+DEF_AMASK)&~DEF_AMASK);

	/* Decrease pool size by remainder of above round up	*/
	pool_size      -= (gPoolBase[ppid] - (UINT32)base_addr);

	/* Set last address of Buddy Pool						*/
	gPoolEnd2[ppid] = gPoolBase[ppid] + pool_size;

	/* Set number of blocks can be created in each buddy	*/
	maxIndex[ppid]  = get_log2(pool_size);
	partIndex[ppid] = get_log2(PART_SIZE);
	nMinSzBlks      = (1 << get_log2(pool_size));
	nUsedBitSz      = (nMinSzBlks / 8 *2);

	/* Set pointer for gBdyUsed[] bits and decrease pool size*/
	pool_size		-= nUsedBitSz;
	gBdyUsed[ppid]	 = (UINT32 *)(gPoolBase[ppid] + pool_size);

	/* Set pointer for gPartHdr[] list						*/
	gPartCnt[ppid]   = (pool_size / PART_SIZE) + 1;
	pool_size       -= OVERHEAD * gPartCnt[ppid];
	pool_size       &= ~PART_MASK;
	gPartHdr[ppid]   = (BuddyHead_t *)(gPoolBase[ppid] + pool_size);

	gPoolEnd1[ppid]  = gPoolBase[ppid] + pool_size;
	nMinSzBlks       = (1 << get_log2(pool_size));

	#if (BUDDY_DEBUG > 0)
	dbgprint("");
	dbgprint("BuddyInit_%d[0x%08x,%08x]", ppid, gPoolBase[ppid], gPoolEnd1[ppid]);
	#endif

	total_blks = 0;
	for (i = BDY_MIN_INDEX; i <= maxIndex[ppid]; i++)
	{
		gBdyBlks[ppid][i]    = total_blks;
		total_blks          += nMinSzBlks;
		nMinSzBlks          /= 2;
		#if (BUDDY_DEBUG > 0)
		dbgprint("blks[%2d] = (%07x*%05x) [%08x]", i, __RUT[i], total_blks - gBdyBlks[ppid][i], gBdyBlks[ppid][i]);
		#endif
	}

	memset((void *)&(gBdyUsed[ppid][0]), 0xFF, nUsedBitSz);
	/* Initialize headers in gPartHdr[] partition list		*/
	#if (BUDDY_DEBUG > 0)
	dbgprint("Initializing Partition Headers");
	#endif
	for (i = 0, pBdyBase = gPartHdr[ppid]; i < gPartCnt[ppid]; i++, pBdyBase++)
	{
		LS_INIT(pBdyBase);
		pBdyBase->bd_flags = SM_FLAG_CHILD;
	}

	bIndex   = get_log2(pool_size);
	asize    = __RUT[bIndex];
	pBdyBase = (BuddyHead_t *)&gFreeHdr[ppid][bIndex];
	pBdyInit = Dptr2Hptr(ppid, gPoolBase[ppid]);

	#if (BUDDY_DEBUG > 0)
	dbgprint("Insert 0x%08x to buddy[%2d]: asize=%07x", pBdyInit, bIndex, pool_size);
	#endif
	pBdyInit->bd_asize = pool_size;
	pBdyInit->bd_osize = pool_size;
	pBdyInit->bd_flags = SM_FLAG_FHEAD;
	LS_INS_BEFORE(pBdyBase, pBdyInit);

	nFreeTot        += pool_size;	// Increase Free Memory Size
	nFreePool[ppid] += pool_size;	// Increase Free Buddy Size

	#if (BUDDY_DEBUG > 0)
	dbgprint("Buddy Initialzation Done");
	#endif

	return;
}

void
SM_InitPool (void **ppaddr, size_t asize)
{
	int		ppid, vpid;
	char	*addr = (char*)*ppaddr;

	dbgprint("^y^Initializing Buddy Free Headers");
	for (ppid = 0; ppid < SM_PHYS_POOLS; ppid++)
	{
		int		i;
		for (i = BDY_MIN_INDEX; i < BDY_MAX_INDEX; i++)
		{
			LS_INIT(&gFreeHdr[ppid][i]);
		}
	}
	dbgprint("^y^Initializing Buddy Allocation Headers");
	for (vpid = 0; vpid < SM_VIRT_POOLS; vpid++)
	{
		/* Skip SM_MMAP_POOL header, it's initailized already */
		if (gAllocHdr[vpid].next == NULL)
			LS_INIT(&gAllocHdr[vpid]);
	}

	SM_BuddyInit(SM_SBDY_POOL, addr, asize);
	return;
}

static int
SM_BuddyMergeWithIndex(int ppid, int midx)
{
	int			index, bIndex, maxMerged = 0;
	UINT08		sppid, eppid, smidx, emidx;
	BuddyHead_t	*pBdy, *pBdyBase, *pBdy1, *pBdy2;

	if ( (ppid < 0) || (ppid >= SM_PHYS_POOLS) )
	{
		sppid = SM_FMAT_POOL;
		eppid = SM_SDEC_POOL;
		midx  = -1;
	}
	else
	{
		sppid = eppid = ppid;
	}

//	dbgprint("SM_BuddyMerge(%d)", ppid);

	for (ppid = sppid; ppid <= eppid; ppid++)
	{
		if (midx < 0)
		{
			smidx = (UINT08)partIndex[ppid];
			emidx = (UINT08)maxIndex[ppid];
		}
		else
		{
			smidx = midx;
			emidx = midx + 1;
		}

		for (index = smidx; index < emidx; index++)
		{
			pBdyBase = (BuddyHead_t *)&gFreeHdr[ppid][index];
			if (LS_ISEMPTY(pBdyBase))
				continue;

			pBdy = pBdyBase;

			while (pBdy->next != pBdyBase)
			{
				pBdy1 = pBdy->next;
				pBdy2 = pBdy1 + (pBdy1->bd_asize / PART_SIZE);

				if (pBdy2->bd_flags == SM_FLAG_FHEAD)
				{
					#if 0
					dbgprint("^p^ == Merging %x(%x+%06x) and %x(%x+%06x), idx %d->%d",
							pBdy1, Hptr2Dptr(ppid, pBdy1), pBdy1->bd_asize,
							pBdy2, Hptr2Dptr(ppid, pBdy2), pBdy2->bd_asize,
							index, get_log2(pBdy1->bd_asize + pBdy2->bd_asize));
					#endif

					LS_REMQUE(pBdy2);

					pBdy1->bd_asize	+= pBdy2->bd_asize;
					pBdy1->bd_osize	 = pBdy1->bd_asize;

					pBdy2->bd_flags	 = SM_FLAG_CHILD;

					if (pBdy1->bd_asize > (UINT32)maxMerged)
						maxMerged = pBdy1->bd_asize;

					bIndex = get_log2(pBdy1->bd_asize);
					#if	!defined(SM_SORTED_FLIST)
					if (bIndex != index)
					{
						LS_REMQUE(pBdy1);
						LS_INS_BEFORE(&gFreeHdr[ppid][bIndex], pBdy1);
						pBdy = pBdyBase;
					}
					#else	/* Insert in the middle of size-sorted list */
					if ( (bIndex != index)
					  || ((pBdy1->next != pBdyBase) && (pBdy1->bd_asize > pBdy1->next->bd_asize)))
					{
						LS_REMQUE(pBdy1);
						SM_InsertToSortedList(ppid, bIndex, pBdy1);

						/* cascade merge operation to upper pool */
						if (bIndex != index)
						{
							emidx = (UINT08)maxIndex[ppid];
						}
						pBdy = pBdyBase;
					}
					#endif
					else
					{
						/* Do not advance pointer, check this node once more */
					}
				}
				else
				{
					pBdy = pBdy->next;
				}
			}
		}
	}

//	dbgprint("SM_BuddyMerge(%d), max %d byte part merged", ppid, maxMerged);

	return(maxMerged);
}

int
SM_BuddyMerge(int ppid)
{
	return (SM_BuddyMergeWithIndex(ppid, -1));
}

#if defined(SM_SORTED_FLIST)
/*
 *	Find appropriate position in the free list and insert it.
 */
static void SM_InsertToSortedList(int ppid, int end, BuddyHead_t *pBdyIns)
{
	BuddyHead_t	*pBdyPar = gFreeHdr[ppid][end].next;

	while (pBdyPar != (BuddyHead_t *)&gFreeHdr[ppid][end])
	{
		if (pBdyIns->bd_asize <= pBdyPar->bd_asize)
			break;
		pBdyPar = pBdyPar->next;
	}
	LS_INS_BEFORE(pBdyPar, pBdyIns);
	return;
}
#endif
void *
SM_BuddyAlloc(size_t osize, UINT08 vpid)
{
	BuddyHead_t	*pBdyL = NULL, *pBdyR, *pBdyT;
	BuddyHead_t	*pBdyP = NULL;
	bOffset_t	bOffset;
	UINT32		start, end, asize;
	UINT08		ppid;
	UINT32		overhead;


	vpid = ppid = SM_SBDY_POOL;

	overhead = ((osize <= (PART_SIZE - OVERHEAD)) ? OVERHEAD : 0);
	asize    = osize + overhead;
	start    = get_log2(asize);

	#if (BUDDY_DEBUG > 0)
	dbgprint(" Entering SM_BuddyAlloc(%d, %d), Start=%d", osize, vpid, start);
	#endif

	if ((gnAllocs % 512) == 0)
	{
		#if (BUDDY_DEBUG > 0)
		dbgprint("^g^Call SM_BuddyMerge() by periodic");
		#endif
		(void)SM_BuddyMerge(ppid);
	}
Rescan_FreeList:
	gnAllocs++;
	for (end = start; end <= maxIndex[ppid]; end++)
	{
		BuddyHead_t	dummy;
		BuddyHead_t	*pBestFit = &dummy;

		gnASrchs++;
		if(LS_ISEMPTY(&gFreeHdr[ppid][end]))
			continue;

		pBestFit->bd_asize = (UINT32)-1;

		/*	Check pointer is valid or not		*/
		pBdyL = gFreeHdr[ppid][end].next;
		if (pBdyL->prev != (BuddyHead_t *)&gFreeHdr[ppid][end])
		{
			dbgprint("^R^SM_alloc_ERROR] Broken Buddy FreeList, ppid=%d, end=%d, osize=%d [%x->%x!=%x]",
							ppid, end, osize, pBdyL, pBdyL->prev, &gFreeHdr[ppid][end]);
			LS_INIT(&gFreeHdr[ppid][end]);
			continue;
		}

		if (end >= partIndex[ppid])
		{
			size_t new_asize = ((asize + PART_MASK) & ~PART_MASK);
			#if	defined(SM_SORTED_FLIST)
			/*
			 *	Current free list의 마지막 노드가 가장 큰 것이므로,
			 *	이것에 요구된 메모리보다 작으면 다음 free list 검색.
			 */
			if (new_asize > gFreeHdr[ppid][end].prev->bd_asize)
				continue;
			#endif

			/*
			 *	파티션에 의한 할당인 경우, 최적의 크기의 풀을 검색한다.
			 */
			while (pBdyL != (BuddyHead_t *)&gFreeHdr[ppid][end])
			{
				#ifdef	SM_FIND_BEST_FIT
				if (pBdyL->bd_asize == new_asize)	// Find best fit pool
				#else
				if (pBdyL->bd_asize >= new_asize)	// Find first fit pool
				#endif
				{
					/* This is right matched */
					pBestFit = pBdyL;
					//dbgprint("^p^New just fit for malloc(%d): %d", asize, pBdyL->bd_asize);
					break;
				}
				#ifdef	SM_FIND_BEST_FIT
				else if ((pBdyL->bd_asize > new_asize) && (pBestFit->bd_asize > pBdyL->bd_asize))
				{
					//if (pBestFit != &dummy)
					//	dbgprint("^p^New best fit for malloc(%d): %d <- %d", asize, pBdyL->bd_asize, pBestFit->bd_asize);
					pBestFit = pBdyL;
				}
				#endif
				pBdyL = pBdyL->next;
			}
			if (pBestFit == &dummy)
				continue;

			pBdyL = pBestFit;
		}

		LS_REMQUE(pBdyL);
		break;
	}

	if (end > maxIndex[ppid])
	{
		if (SM_BuddyMerge(-1) >= (int)asize)
			goto Rescan_FreeList;

		dbgprint("^R^No memory available greater than (%d) byte", osize);
		return (NULL);
	}
	#if (BUDDY_DEBUG > 0)
	dbgprint("   %2d] Free buddy found = 0x%x(%x)", end, pBdyL, (pBdyL ? pBdyL->bd_asize : -1));
	#endif
	if (end >= partIndex[ppid])
	{
		UINT32	pSize;
		UINT32	dPtr;

		/*
		 *	요구된 크기의 메모리를 할당할 수 있는 파티션이 찾아진 경우,
		 *	Free 메모리를 앞 부분을 할당하고, 뒷부분은 다시 free list에 넣는다.
		 */
		pSize = (asize + PART_MASK) & ~PART_MASK;

		#if (BUDDY_DEBUG > 0)
		dbgprint("   %2d] Cut %d byte Part from Header %x(%d)", end, pSize, pBdyL, pBdyL->bd_asize);
		#endif
		if (pBdyL->bd_asize > pSize)
		{
			dPtr = Hptr2Dptr(ppid, pBdyL) + pSize;
			pBdyR = Dptr2Hptr(ppid, dPtr);

			pBdyR->bd_flags = SM_FLAG_FHEAD;
			pBdyR->bd_asize = pBdyL->bd_asize - pSize;
			pBdyR->bd_osize = pBdyR->bd_asize;
			end = get_log2(pBdyR->bd_asize);
			#if (BUDDY_DEBUG > 0)
			dbgprint("   %2d] Add Right Part (%08x, %06x) to FreeList", end, pBdyR, pBdyR->bd_asize);
			#endif
			#if	!defined(SM_SORTED_FLIST)
			LS_INS_BEFORE(&gFreeHdr[ppid][end], pBdyR);
			#else	/* Insert in the middle of size-sorted list */
			SM_InsertToSortedList(ppid, end, pBdyR);
			#endif

			pBdyL->bd_asize = pSize;
		}
		end = get_log2(pBdyL->bd_asize);

		pBdyP = pBdyL;
		if (start < partIndex[ppid])
		{
			pBdyP->bd_flags = SM_FLAG_BUDDY;
			pBdyP->bd_asize = pSize;
			pBdyP->bd_osize = pSize;
			pBdyL = (BuddyHead_t *)Hptr2Dptr(ppid, pBdyP);
		}
		else
		{
			pBdyP->bd_flags = SM_FLAG_UHEAD;
			pBdyP->bd_asize = pSize;
			pBdyP->bd_osize = asize;
		}
		asize = pSize;
	}

	if (end > start)
	{
		/*
		 *	요구된 크기의 메모리의 크기가 파티션의 크기보다 작으면,
		 *	Buddy 의 단위 크기(128, 256, 512, 1024, 2048)로 나누어 왼쪽 것은
		 *	할당하고, 오른쪽 것은 Free buddy list에 등록한다.
		 */
		OFS2BOFS(ppid, bOffset, end, BUD_OFFSET(ppid, pBdyL));
		SET_USED(ppid, bOffset);
		#if (BUDDY_DEBUG > 0)
		dbgprint("   %2d] Mark Left buddy(%08x) as used", end, pBdyL);
		#endif
		do
		{
			end--;
			asize  = __RUT[end];
			pBdyT  = (BuddyHead_t *)&gFreeHdr[ppid][end];
			pBdyR  = (BuddyHead_t *)((UINT32) pBdyL + asize);
			#if (BUDDY_DEBUG > 0)
			dbgprint("   %2d] Add Right buddy(%08x, %06x) to FreeList", end, pBdyR, asize);
			#endif
			OFS2BOFS(ppid, bOffset, end, BUD_OFFSET(ppid, pBdyR));
			CLEAR_USED(ppid, bOffset);
			pBdyL->bd_asize = asize;
			pBdyL->bd_flags = SM_FLAG_BUDDY;
			pBdyR->bd_asize = asize;
			pBdyR->bd_flags = SM_FLAG_BUDDY;
			LS_INS_BEFORE(pBdyT, pBdyR);
		} while ( end > start);
	}

	if (start < partIndex[ppid])
	{
		asize  = __RUT[start];
		OFS2BOFS(ppid, bOffset, start, BUD_OFFSET(ppid, pBdyL));
		SET_USED(ppid, bOffset);
		pBdyL->bd_asize = asize;
		pBdyL->bd_osize = osize + overhead;
		pBdyL->bd_flags = SM_FLAG_BUDDY;
	}

	pBdyL->bd_vpid = vpid;
	LS_INS_BEFORE(&gAllocHdr[vpid], pBdyL);

	nFreeTot        -= asize;
	nFreePool[ppid] -= asize;
	memASize		+= asize;
	memOSize		+= (osize + overhead);

	pBdyL->bd_task	 = 0;

	if (start < partIndex[ppid]) pBdyL++;
	else                         pBdyL = (BuddyHead_t *)Hptr2Dptr(ppid, pBdyL);

	#if	(SM_FILL_MODE >= 1)
	SM_checkMagic((void *)pBdyL, asize - overhead, osize, 0);
	#endif	/* SM_FILL_MODE >= 1 */

	#if (BUDDY_DEBUG > 0)
	dbgprint(" Leaving : %08x(%d)", pBdyL, osize);
	#endif
	return((void *)pBdyL);
}

void
SM_BuddyFree(void *vaddr, UINT32 caller, UINT08 ppid)
{
	BuddyHead_t	*pBdyF;		// Current free pointer;
	BuddyHead_t	*pBdyS;		// Sibling free pointer;
	BuddyHead_t	*pBdyP;		// Current partition pointer
	BuddyHead_t	*pBdyBase;
	bOffset_t	bOffsetF, bOffsetS;
	UINT32		offsetF, offsetS, bIndex, asize, osize, eAddr;
	UINT32		isUsed, asMask, overhead;
	UINT08		isPart;

	pBdyP	= Dptr2Hptr(ppid, vaddr);
	isPart	= (pBdyP->bd_flags <= SM_FLAG_UHEAD);
	pBdyF	= (isPart ? pBdyP : (BuddyHead_t *)vaddr - 1);
	eAddr   = (UINT32) pBdyF + pBdyF->bd_asize;
	bIndex  = get_log2(pBdyF->bd_asize);
	asize   = (isPart ? pBdyP->bd_asize : __RUT[bIndex]);
	osize   = pBdyF->bd_osize;

	if (isPart)
	{
		offsetF = offsetS = (UINT32)vaddr - gPoolBase[ppid];
		OFS2BOFS(ppid, bOffsetF, bIndex, offsetF);
		isUsed	= (pBdyP->bd_flags == SM_FLAG_UHEAD);
		asMask	= PART_MASK;
		overhead= 0;
	}
	else
	{
		offsetF = offsetS = BUD_OFFSET(ppid, pBdyF);
		OFS2BOFS(ppid, bOffsetF, bIndex, offsetF);
		isUsed	= BUD_ISUSED(ppid, bOffsetF);
		asMask	= asize - 1;
		overhead= OVERHEAD;
	}

	if ( (isUsed           ==     0 ) ||	// Used bit must be set.
		#ifndef	SM_LOG_ALLOC
		 !LS_ISEMPTY(pBdyF)		      ||	// Has valid allocated chain ?
		#endif	/* SM_LOG_ALLOC */
		 (offsetS          &  asMask) ||	// Address must be 'asize' aligned
		 (asize            <  osize ) ||	// osize can not greater than asize
		 (pBdyF->bd_asize  != asize )  )		// Is asize field correct ?
	{
		dbgprint("^R^>>\tfree_ERROR_2:A=%x(%06x),U=%d,S=%04x/%04x/%04x",
				 	 pBdyF, offsetF, isUsed, osize, asize, pBdyF->bd_asize);
		dbgprint("^R^>>\tfree_ERROR_2:P=%x,N=%x", pBdyF->prev, pBdyF->next);
		if (isPart)
		{
			hexdump("Parts Error, pBdyP ", pBdyP, OVERHEAD);
			hexdump("Parts Error, pData", vaddr, 0x40);
		}
		else
		{
			hexdump("Buddy Error", (void *)((UINT32)pBdyF - 0x20), 0x80);
		}
		return;
	}

	#if	(SM_LOG_ALLOC > 0)
	LS_REMQUE(pBdyF);
	pBdyF->bd_lbolt = OSA_ReadMsTicks();
	#endif	/* SM_LOG_ALLOC */

	nFreeTot        += asize;
	nFreePool[ppid] += asize;

	if (1)//initState[ppid] == SM_STATE_2)
	{
		//SM_MacUpdate(asize, -1, 1);
		gnFrees  += 1;
		memASize -= asize;
		memOSize -= osize;

		//OSA_UpdateMemUsage(pBdyF->bd_task, -asize);
	}

	#if	(SM_FILL_MODE >= 2)
	/* Check Magic Patterns	*/
	SM_checkMagic(vaddr, pBdyF->bd_asize-overhead, pBdyF->bd_osize-overhead, 4);
	#endif	/* SM_FILL_MODE == 3 */

	#if	(SM_LOG_ALLOC > 0)
	if (do_log_caller) LOG_CALLER(pBdyF->bd_mUser[0]);
	#endif	/* SM_LOG_ALLOC */

	pBdyF->bd_osize = 0xF1EE00;
	while (bIndex < partIndex[ppid])
	{
		gnFSrchs++;				// Increment Free Search Counter
		offsetF = BUD_OFFSET(ppid, pBdyF);
		offsetS = (offsetF ^ __RUT[bIndex]);
		OFS2BOFS(ppid, bOffsetF, bIndex, offsetF);
		OFS2BOFS(ppid, bOffsetS, bIndex, offsetS);
		pBdyS   = (BuddyHead_t *)(gPoolBase[ppid] + offsetS);
		pBdyBase= (BuddyHead_t *)&gFreeHdr[ppid][bIndex];

		if (((UINT32)pBdyS <  gPoolEnd1[ppid]) &&
			(BUD_ISUSED(ppid, bOffsetS) == 0) &&
			(bIndex < maxIndex[ppid]        ) )
		{
			SET_USED(ppid, bOffsetS);
			LS_REMQUE(pBdyS);
			if (pBdyS < pBdyF)
			{
				pBdyF = pBdyS;
				pBdyF->bd_osize = 0xF1EE00;
			}
			bIndex++;
			pBdyF->bd_asize = __RUT[bIndex];
			#if (BUDDY_DEBUG > 0)
			dbgprint("   %2d] Merging   Buddy(%08x, %06x)", bIndex, pBdyF, pBdyF->bd_asize);
			#endif
			continue;
		}
		else
		{
			CLEAR_USED(ppid, bOffsetF);
			#if (BUDDY_DEBUG > 0)
			dbgprint("   %2d] Add Freed Buddy(%08x, %06x)", bIndex, pBdyF, pBdyF->bd_asize);
			#endif
			LS_INS_BEFORE(pBdyBase, pBdyF)
			pBdyF->bd_flags = SM_FLAG_BUDDY;
			#if (SM_FILL_MODE >= 2)
			memset((void *) (pBdyF+1), SM_FILL_MAGIC_B, pBdyF->bd_asize-OVERHEAD);
			#endif	/* (SM_FILL_MODE >= 2) */
			break;
		}
	}

	if (bIndex >= partIndex[ppid])
	{
		#if (BUDDY_DEBUG > 0)
		dbgprint("   %2d] Add Freed Parts(%08x, %06x)", bIndex, pBdyP, pBdyP->bd_asize);
		#endif
		pBdyBase= (BuddyHead_t *)&gFreeHdr[ppid][bIndex];
		LS_INS_BEFORE(pBdyBase, pBdyP)
		pBdyP->bd_flags = SM_FLAG_FHEAD;
		#if (SM_FILL_MODE >= 2)
		memset((void *)Hptr2Dptr(ppid, pBdyP), SM_FILL_MAGIC_B, pBdyP->bd_asize);
		#endif	/* (SM_FILL_MODE >= 2) */

		if (pBdyP->bd_asize >= 0x4000)
		{
			pBdyS = pBdyP + (pBdyP->bd_asize / PART_SIZE);
			if (pBdyS->bd_flags == SM_FLAG_FHEAD)
			{
				SM_BuddyMergeWithIndex(ppid, bIndex);
			}
		}
	}
}

void *Malloc (size_t osize)
{
	return(SM_BuddyAlloc(osize, SM_SBDY_POOL));
}

void Free (void *vaddr)
{
	SM_BuddyFree(vaddr, 0, 1);
	return;
}
