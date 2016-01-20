#include "common.h"
#include "osadap.h"
#include "test.h"

#define ALLOC_CNT	20

void *systemMemoryPool = NULL;

static char *local_envList[] =
{
	"MAT_008=20480",
	"MAT_010=40960",
	"MAT_020=20480",
	"MAT_040=8192",
	"MAT_080=30720",
	"MAT_100=10240",
	NULL
};

int main (void)
{
	void *	addr;
	size_t	asize;
	char	*tbuff[ALLOC_CNT] = { NULL, };
	int		alloc_size[]      = { 4,    5,    7,    8, 
		                          15,   25,   1023, 1025, 
								  2047, 5096, 401, 511 
	                            };
	int		i = 0, cnt        = sizeof(alloc_size)/sizeof(int);

	systemMemoryPool = mmap((void*)NULL, SYSTEM_MEM_SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	addr       = (void*)(((UINT32)systemMemoryPool + PART_MASK) & ~PART_MASK);
	asize      = 32*1024*1024;

	dbgprint("^y^>>Pool address:0x%08x, Size:%d", (UINT32)addr, asize);

	while(local_envList[i])
	{
		pENV_List[i] = local_envList[i];
		i++;
	}
	pENV_List[i] = NULL;

	_OSA_MEM_BEG();

	SM_ConfigParam();
	SM_InitPool(&addr, asize);

	for(i = 0; i  < cnt; i++)
	{
		int sz_idx = i;
		tbuff[i] = (char *)Malloc(alloc_size[sz_idx]);

		dbgprint("^g^[%2d]Alloc->tbuff[0x%08x], Req Size: %d", i, (UINT32)tbuff[i], alloc_size[sz_idx]);

	}

	for(i = cnt-1; i  >= 0; i--)
	{
		Free(tbuff[i]);
		dbgprint("^p^[%2d]Free->tbuff", i);
	}
	dbgprint("^y^>>");
	dbgprint("^y^>>");

	munmap(systemMemoryPool, SYSTEM_MEM_SZ);

	return 0;
}
