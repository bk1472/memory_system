#include "common.h"
#include "osadap.h"
#include "test.h"

#define ALLOC_CNT	20

void *systemMemoryPool = NULL;

int main (void)
{
	void *	addr;
	size_t	asize;
	char	*tbuff[ALLOC_CNT] = { NULL, };
	int		alloc_size[]      = { 257, 325, 400, 440, 514, 1023, 1025, 2047, 5096, 238, 401, 511 };
	int		i, cnt            = sizeof(alloc_size)/sizeof(int);

	systemMemoryPool = mmap((void*)NULL, SYSTEM_MEM_SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	addr       = (void*)(((UINT32)systemMemoryPool + PART_MASK) & ~PART_MASK);
	asize      = 32*1024*1024;

	printf(">>Pool address:0x%08x, Size:%d\n", (UINT32)addr, asize);

	SM_InitPool(&addr, asize);

	for(i = 0; i  < cnt; i++)
	{
		int sz_idx = i%12;
		tbuff[i] = (char *)Malloc(alloc_size[sz_idx]);

		printf("[%2d]Alloc->tbuff[0x%08x], Req Size: %d\n", i, (UINT32)tbuff[i], alloc_size[sz_idx]);

	}

	for(i = cnt-1; i  >= 0; i--)
	{
		Free(tbuff[i]);
		printf("[%2d]Free->tbuff\n", i);
	}
	printf(">>\n");
	printf(">>\n");

	munmap(systemMemoryPool, SYSTEM_MEM_SZ);
	return 0;
}
