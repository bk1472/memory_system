#include "common.h"
#include "osadap.h"

/*-----------------------------------------------------------------------------
	상수 정의
	(Constant Definitions)
------------------------------------------------------------------------------*/
#define _KBYTE		 			1024

#ifndef EXT_MATDEF
 #define	MAT_008_MAX		    20*_KBYTE	/*   8 Byte Entries:  8*20K = (0x003000)  */
 #define	MAT_010_MAX		    40*_KBYTE	/*  16 Byte Entries: 16*40K = (0x028000)  */
 #define	MAT_020_MAX			20*_KBYTE	/*  32 Byte Entries: 32*20K = (0x040000)  */
 #define	MAT_040_MAX		     8*_KBYTE   /*  64 Byte Entries: 64* 8K = (0x040000)  */
 #define	MAT_080_MAX			30*_KBYTE	/* 128 Byte Entries:128*30K = (0x040000)  */
 #define	MAT_100_MAX			10*_KBYTE	/* 256 Byte Entries:256*10K = (0x100000)  */
#endif

//#define SM_USE_LINK_METHOD
#undef SM_USE_LINK_METHOD

#ifdef SM_USE_LINK_METHOD
 # define SM_MAT_DEBUG_LVL     1
 # define SM_MAT_OVHEAD		   1
#else
 # define SM_MAT_DEBUG_LVL     0
 # define SM_MAT_OVHEAD		   0
#endif

#define	MAT_NUM_LEVELS		   4	/* Number of levels in fast MAT tree	  */
#define	SM_NUM_MATS			   6	/* Number size entries fast MAT			  */
#define SM_MAT_MIN			   8	/* Minimum size of SM_MAT pool/2          */
#define SM_MAT_MIN_LOG		   3	/* LOG(SM_MAT_MIN)                        */
#define	SM_MAT_MAX			(SM_MAT_MIN << (SM_NUM_MATS-1))

#define SM_MAC_LOGGERS		  		17	/* Maximum number of MAC loggers		 */
#define MAX_SM_MAT_POOL_CNT			20
#define MAX_FAST_MAT_BYTE			256

#ifndef SM_NCALLS
 #define SM_NCALLS			   6	// Depth of stack trace.
#endif

#if		(MAT_008_MAX>=32768) || (MAT_010_MAX>=32768) || (MAT_020_MAX>=32768)	\
	 ||	(MAT_040_MAX>=32768) || (MAT_080_MAX>=32768) || (MAT_100_MAX>=32768)
  #define	MAT_BASE_LEVEL	   0
#else
  #define	MAT_BASE_LEVEL	   1
#endif	/* MAT_nn_MAX	*/

#define SM_LOG(addr)

typedef struct
{
	UINT32	sm_base;			/* Base address of pool					*/
	UINT32	sm_maxu;			/* Maximum No. of units in this pool	*/
	UINT32	sm_free;			/* Number of free units in this pool	*/
	#ifdef SM_USE_LINK_METHOD
	UINT32	*sm_masks;			/* Mask data for each level				*/
	#else
	UINT32	**sm_masks;			/* Mask data for each level				*/
	#endif
	UINT08	*sm_orgSize;		/* Original size of [mc]alloc()	arg		*/
	UINT08	*sm_owner;			/* Memory Owner Task                    */
	UINT32	*sm_link;			/* Memory link addr                     */
	UINT32	sm_index;			/* alloc index                          */
	void*	sm_start;			/* start pointer                        */
	#if (SM_MAT_DEBUG_LVL > 1)
	UINT32	sm_mUser[SM_NCALLS];
	#endif
} SM_MAT_POOL_t;

typedef struct
{
	UINT32	nAlloc;				/* Number of allocations				*/
	UINT32	nFrees;				/* Number of frees						*/
	UINT32	nMax;				/* Maximum concurrent allocations.		*/
	UINT32	naSize;				/* Allocated memory size.				*/
} mac_log_t;

static mac_log_t	mac_log[SM_MAC_LOGGERS+1] = { { 0, }, };

static int		sm_ovhead = 0;
static int		sm_mat_baselevel = MAT_BASE_LEVEL;

static UINT08	MAT_SIZ2ENT[SM_MAT_MAX>>SM_MAT_MIN_LOG] =
{
//  ~8 ~16 ~24 ~32 ~40 ~48 ~56 ~64 ~72 ~80 ~88 ~96 ~104 ~112 ~120 ~128
	 0,  1,  2,  2,  3,  3,  3,  3,  4,  4,  4,  4,   4,   4,   4,   4,
	 5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,   5,   5,   5,   5
};

char*			pszSM_MAT_STR[]   = {
	"MAT_008", "MAT_010", "MAT_020",
	"MAT_040", "MAT_080", "MAT_100",
	NULL
};
UINT32 SM_MAT_SZ_ENTRY[] = {
			MAT_008_MAX, MAT_010_MAX, MAT_020_MAX,
            MAT_040_MAX, MAT_080_MAX, MAT_100_MAX, 0 };

SM_MAT_POOL_t	SM_MAT_POOL[MAX_SM_MAT_POOL_CNT];

void
SM_MacUpdate (size_t asize, int poolNo, int mode)
{
	mac_log_t	*pMAC;		// Pointer to memory allocation log pool

	if (poolNo < 0)
	{
		poolNo = SM_MAT_MIN_LOG;
		while ((asize - 1) >> poolNo)
			poolNo++;
		poolNo -= SM_MAT_MIN_LOG;
	}
	if (poolNo > SM_MAC_LOGGERS) poolNo = SM_MAC_LOGGERS;

	pMAC = &mac_log[poolNo];

	if (mode == 0)			// Update allocation counter
	{
		pMAC->nAlloc++;
		if ((pMAC->nAlloc - pMAC->nFrees) > pMAC->nMax)
		 	pMAC->nMax = pMAC->nAlloc - pMAC->nFrees;
		pMAC->naSize += asize;
	}
	else					// Update free counter
	{
		pMAC->nFrees++;
		pMAC->naSize -= asize;
	}

	return;
}

/**
 *
 * This is for New Fast Mat allocation initialize method.....
 * dynamic initial value
 */
static void
_SM_FastMatStrcInit(void)
{
	int mat_cnt = MAT_SIZ2ENT[(MAX_FAST_MAT_BYTE-1)>>SM_MAT_MIN_LOG] + 1;
	int	i;

	sm_ovhead = SM_MAT_OVHEAD;
	sm_mat_baselevel = 1;
	for (i = 0; i < mat_cnt; i++)
	{
		if(SM_MAT_SZ_ENTRY[i] >= 32768)
		{
			sm_mat_baselevel = 0;
			break;
		}
	}

	for (i = 0; i <= mat_cnt; i++)
	{
		SM_MAT_POOL_t	*pMAT_Pool;
		int				maskSz;
		int				maxUnitSz = SM_MAT_SZ_ENTRY[i];
		int				matMngSz;
		void			*pManagePool;
		UINT32			mngPoolPtr;
		#ifdef SM_USE_LINK_METHOD
		#else
		int				j;
		int				unitCnt = 0;
		UINT32			maskCnt[MAT_NUM_LEVELS];
		#endif

		pMAT_Pool = &SM_MAT_POOL[i];

		//Mask member size
		#ifdef SM_USE_LINK_METHOD
		maskSz = ((maxUnitSz+31)/32)*sizeof(UINT32);
		#else
		maskSz = MAT_NUM_LEVELS*sizeof(UINT32);
		for (j = MAT_NUM_LEVELS-1; j >= sm_mat_baselevel; j--)
		{
			if (j == (MAT_NUM_LEVELS-1)) maskCnt[j] = ((maxUnitSz+31)/32);
			else                         maskCnt[j] = ((unitCnt  +31)/32);

			unitCnt += maskCnt[j];
			maskSz  += unitCnt*sizeof(UINT32);
		}
		#endif
		//OSIZE member size
		matMngSz = maxUnitSz*sizeof(UINT08);
		//OWNER member size
		matMngSz += maxUnitSz*sizeof(UINT08);
		if(sm_ovhead)
		{
			//LINK member size
			matMngSz += maxUnitSz*sizeof(UINT32);
		}
		//Total Management size alloc.
		matMngSz  = maskSz + matMngSz;

		pManagePool = mmap((void*)NULL, matMngSz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		mngPoolPtr  = (UINT32)pManagePool;

		pMAT_Pool->sm_base    = 0;
		pMAT_Pool->sm_maxu    = maxUnitSz;
		pMAT_Pool->sm_free    = maxUnitSz;
		#ifdef SM_USE_LINK_METHOD
		pMAT_Pool->sm_masks   = (UINT32 * )(mngPoolPtr); mngPoolPtr += maskSz;
		#else
		pMAT_Pool->sm_masks   = (UINT32 **)(mngPoolPtr); mngPoolPtr += MAT_NUM_LEVELS*sizeof(UINT32);
		for (j = MAT_NUM_LEVELS-1; j >= sm_mat_baselevel; j--)
		{
			pMAT_Pool->sm_masks[j]= (UINT32 *)(mngPoolPtr); mngPoolPtr += maskCnt[j]*sizeof(UINT32);
		}
		#endif
		pMAT_Pool->sm_owner   = (UINT08 *)(mngPoolPtr); mngPoolPtr += maxUnitSz*sizeof(UINT08);
		if (sm_ovhead)
		{
			pMAT_Pool->sm_link= (UINT32 *)(mngPoolPtr); mngPoolPtr += maxUnitSz*sizeof(UINT32);
		}
		else
		{
			pMAT_Pool->sm_link= NULL;
		}
		pMAT_Pool->sm_orgSize = (UINT08 *)(mngPoolPtr);
		pMAT_Pool->sm_index   = 0;
		pMAT_Pool->sm_start   = NULL;
	}
	memset((void*)&SM_MAT_POOL[i], 0x00, sizeof(SM_MAT_POOL_t));
}

void
SM_ConfigParam(void)
{
	int		i = 0;

	while (NULL != pszSM_MAT_STR[i])
	{
		UINT32	matCnt;
		char	*pMatSz = _getenv(pszSM_MAT_STR[i]);

		if(pMatSz != NULL)
		{
			UINT32 rUpMsk = _KBYTE -1;

			matCnt = (UINT32)atoi(pMatSz);

			matCnt = (matCnt + rUpMsk) & ~rUpMsk;

			SM_MAT_SZ_ENTRY[i] = matCnt;
		}
		i++;
	}
}

void
SM_InitPool (void **ppaddr, size_t asize)
{
	int	i;
	void *addr = *ppaddr;

	_SM_FastMatStrcInit();
	for (i = 0; i < SM_NUM_MATS; i++)
	{
		SM_MAT_POOL_t	*pMAT_Pool;
		UINT32	*pMAT_Mask, unitSize, poolSize;
		int		j, maxEntry;
		#ifdef SM_USE_LINK_METHOD
		#else
		int		numEntry, matLevel;
		#endif

		pMAT_Pool = &SM_MAT_POOL[i];

		#ifdef SM_USE_LINK_METHOD
		maxEntry  = pMAT_Pool->sm_maxu;
		pMAT_Mask = pMAT_Pool->sm_masks;

		// Set free mask bits in each level whose 32 child node are
		// all not allocated.
		for (j = 0; j < (maxEntry / 32); j++)
			pMAT_Mask[j] = 0xFFFFFFFF;
		#else
		maxEntry = pMAT_Pool->sm_maxu;
		for (matLevel = MAT_NUM_LEVELS-1; matLevel >= sm_mat_baselevel; matLevel--)
		{
			pMAT_Mask = pMAT_Pool->sm_masks[matLevel];

			// Set free mask bits in each level whose 32 child node are
			// all not allocated.
			for (j = 0; j < (maxEntry / 32); j++)
				pMAT_Mask[j] = 0xFFFFFFFF;

			numEntry = j* 32;
			if (numEntry < maxEntry)
			{
				pMAT_Mask[j] = 0xFFFFFFFF << (32 - (maxEntry - numEntry));
				maxEntry = j + 1;
			}
			else
			{
				maxEntry = j;
			}
		}
		#endif
		unitSize = (1 << (i+SM_MAT_MIN_LOG));
		poolSize = unitSize * pMAT_Pool->sm_maxu;

		pMAT_Pool->sm_base = (UINT32) addr;

		if (asize > poolSize)
		{
			addr  = (void *) ((UINT32) addr + poolSize);
			asize = asize - poolSize;
		}
		else
		{
			pMAT_Pool->sm_maxu = 0;
			pMAT_Pool->sm_free = 0;
		}

		dbgprint("^y^SM_MAT_POOL[%3d] = 0x%08x(0x%06x)", unitSize, pMAT_Pool->sm_base, poolSize);
	}
	SM_MAT_POOL[i].sm_base = (UINT32) addr;
}

void *
SM_Fast_Alloc(size_t osize, UINT08 vpid)
{
	SM_MAT_POOL_t	*pMAT_Pool;
	void			*addr;
	int				matIndex;
	int				numEntry;
	UINT32			unitSize, *pMAT_Mask;
	UINT32			leafMask;
	#ifdef SM_USE_LINK_METHOD
	#else
	UINT32	leafIndex, testMask;
	int		matLevel;
	#endif

	matIndex  = MAT_SIZ2ENT[(osize-1)>>SM_MAT_MIN_LOG];
	pMAT_Pool = &SM_MAT_POOL[matIndex];

	if ( pMAT_Pool->sm_free > 0 )
	{
		#ifdef SM_USE_LINK_METHOD

		#if	(SM_FILL_MODE >= 1)
		UINT32	uaddr;
		#endif
		unitSize  = (1 << (matIndex+SM_MAT_MIN_LOG));

		if( pMAT_Pool->sm_start != NULL )
		{
			addr     = (void *) pMAT_Pool->sm_start;
			numEntry = (((UINT32)addr) - (UINT32) pMAT_Pool->sm_base) / (unitSize);
			if(pMAT_Pool->sm_link != NULL)
			{
				pMAT_Pool->sm_start = (void*)pMAT_Pool->sm_link[numEntry];
			}
			else
			{
				#if (SM_MAT_DEBUG_LVL > 0)
				UINT32	chkAddr, chkMode = 0;
				UINT32	base = pMAT_Pool->sm_base;
				UINT32	end  = base + pMAT_Pool->sm_maxu*unitSize;
				/**
				 * -- bk1472 note
				 * free 가 이루어진 이후에 free address 가 깨진다면
				 * 다음 allocation time에 system이 crash 될 수 있다.
				 */
				chkAddr = (UINT32)pMAT_Pool->sm_start;
				if(0 != chkAddr)
				{
					if      (chkAddr < base || chkAddr > end) chkMode = 1;
					else if (0 != (chkAddr & (unitSize-1))  ) chkMode = 2;
					if(chkMode)
					{
						dbgprint("^r^Memory crashed after free : %s!", (chkMode & 1)?"Range diff.":"Not aligned");
				  		#if (SM_MAT_DEBUG_LVL > 1)
						dbgprint("^y^Next Stack frame shows latest free sequnce!");
				  		#endif
					}
				}
				#endif
				pMAT_Pool->sm_start = (void*) * ((UINT32*) addr);
			}
		}
		else if( pMAT_Pool->sm_index < pMAT_Pool->sm_maxu )
		{
			addr = (void *)(pMAT_Pool->sm_base+ pMAT_Pool->sm_index*unitSize);

			numEntry = pMAT_Pool->sm_index;
			pMAT_Pool->sm_index ++;
		}
		else
		{
			dbgprint("^R^FMAT broken: numEntry=%p, max=%d", pMAT_Pool->sm_start, pMAT_Pool->sm_index);
			return NULL;
		}
		#else/*SM_USE_LINK_METHOD*/
		unitSize  = (1 << (matIndex+SM_MAT_MIN_LOG));
		numEntry  = 0;
		leafIndex = 0;
		for (matLevel = sm_mat_baselevel; matLevel < MAT_NUM_LEVELS;  matLevel++)
		{
			pMAT_Mask = pMAT_Pool->sm_masks[matLevel];

			/* Search entry having at least 1 free unit	*/
			while (pMAT_Mask[numEntry] == 0)
				numEntry++;

			leafMask = pMAT_Mask[numEntry];
			testMask = ((UINT32)1 << 31);
			for (leafIndex=0; leafIndex < 32; leafIndex++)
			{
				if (leafMask & testMask) break;
				testMask >>= 1;
			}

			numEntry  = numEntry * 32 + leafIndex;
		}

		if (numEntry > (int)pMAT_Pool->sm_maxu)
		{
			dbgprint("^R^FMAT broken: numEntry=%d, max=%d", numEntry, pMAT_Pool->sm_maxu);
			return NULL;
		}
		addr = ((void *)(pMAT_Pool->sm_base+numEntry*unitSize));
		#endif/*SM_USE_LINK_METHOD*/
		if (pMAT_Pool->sm_orgSize != NULL)
		{
			if (unitSize < 256) ((UINT08 *)pMAT_Pool->sm_orgSize)[numEntry] = osize;
			else                ((UINT16 *)pMAT_Pool->sm_orgSize)[numEntry] = osize;
		}

		pMAT_Pool->sm_free--;
		SM_MacUpdate(unitSize, matIndex, 0);

		#ifdef SM_USE_LINK_METHOD
		pMAT_Mask = pMAT_Pool->sm_masks;
		leafMask = (1 << (31 - (numEntry % 32)));

		pMAT_Mask[numEntry/32] &= ~leafMask;
		#else/*SM_USE_LINK_METHOD*/
		for (matLevel = MAT_NUM_LEVELS-1; matLevel >= sm_mat_baselevel; matLevel--)
		{
			testMask = (1 << (31 - (numEntry % 32)));
			numEntry = numEntry / 32;

			pMAT_Mask = pMAT_Pool->sm_masks[matLevel];
			pMAT_Mask[numEntry] &= ~testMask;
			if (pMAT_Mask[numEntry] != 0)
				break;
		}
		#endif/*SM_USE_LINK_METHOD*/
		#if	(SM_FILL_MODE >= 1)
		uaddr = (UINT32)addr;
		SM_checkMagic((void*)(uaddr), unitSize, osize, 0);
		#endif	/* SM_FILL_MODE >= 1 */
	}
	else
	{
		;
	}

	return(addr);
}

void
SM_Fast_Free(void *buf, UINT32 caller, UINT08 ppid)
{
	SM_MAT_POOL_t	*pMAT_Pool;
	int		matIndex, numEntry;
	UINT32	unitSize, *pMAT_Mask;
	UINT32	leafMask, orgSize;
	UINT32	uaddr = (UINT32)buf;
	#ifdef SM_USE_LINK_METHOD
	#else
	int		matLevel;
	#endif

	// This search should not fail, range was checked already.
	for (matIndex=0; matIndex < SM_NUM_MATS; matIndex++)
	{
		if (uaddr<SM_MAT_POOL[matIndex+1].sm_base)
			break;
	}

	unitSize  = (1 << (matIndex+SM_MAT_MIN_LOG));

	if (uaddr & (unitSize-1))
	{
		dbgprint("^R^>>\tfree_ERROR_1: Invalid Address(0x%08x) @ 0x%08x", uaddr, caller);
		return;
	}

	pMAT_Pool = &SM_MAT_POOL[matIndex];
	#ifdef SM_USE_LINK_METHOD
	pMAT_Mask = pMAT_Pool->sm_masks;
	#else
	pMAT_Mask = pMAT_Pool->sm_masks[MAT_NUM_LEVELS-1];
	#endif

	numEntry = (uaddr - (UINT32) pMAT_Pool->sm_base) / unitSize;
	leafMask = (1 << (31 - (numEntry % 32)));

	if (pMAT_Mask[numEntry/32] & leafMask)
	{
		dbgprint("^R^>>\tfree_ERROR_1[A=0x%08x] %d %d %08x: Multiple free @ 0x%08x",
						uaddr,numEntry, unitSize,pMAT_Mask[numEntry/32], caller);
	}
	else
	{
		#ifdef SM_USE_LINK_METHOD
		if(pMAT_Pool->sm_link != NULL)
		{
			pMAT_Pool->sm_link[numEntry] = (UINT32)pMAT_Pool->sm_start;
		}
		else
		{
			* ((UINT32*) uaddr) = (UINT32) pMAT_Pool->sm_start;
		}
		pMAT_Pool->sm_start = (void*) uaddr;
		#endif
		if (pMAT_Pool->sm_orgSize == NULL)
			orgSize = unitSize;
		else if (unitSize < 256)
			orgSize = ((UINT08 *)pMAT_Pool->sm_orgSize)[numEntry];
		else
			orgSize = ((UINT16 *)pMAT_Pool->sm_orgSize)[numEntry];

		pMAT_Pool->sm_free++;
		SM_MacUpdate(unitSize, matIndex, 1);

		#ifdef SM_USE_LINK_METHOD
		pMAT_Mask[numEntry/32] |= leafMask;

		#if (SM_MAT_DEBUG_LVL > 1)
		SM_LOG(pMAT_Pool->sm_mUser[0]);
		#endif
		#else
		for (matLevel = MAT_NUM_LEVELS-1; matLevel >= sm_mat_baselevel; matLevel--)
		{
			leafMask = (1 << (31 - (numEntry % 32)));
			numEntry = (numEntry / 32);

			pMAT_Mask = pMAT_Pool->sm_masks[matLevel];
			pMAT_Mask[numEntry] |= leafMask;
		}
		#endif

		#if	(SM_FILL_MODE >= 2)
		/* Check Magic Patterns	*/
		SM_checkMagic((void *)(uaddr), unitSize, orgSize, 4);
		/* Write Magic Patterns */
		memset((void *)(uaddr), SM_FILL_MAGIC_B, unitSize  );
		#endif	/* SM_FILL_MODE >= 2 */
	}

	return;
}

void *Malloc (size_t osize)
{
	return(SM_Fast_Alloc(osize, 0));
}

void Free (void *vaddr)
{
	SM_Fast_Free(vaddr, 0, 0);
	return;
}
