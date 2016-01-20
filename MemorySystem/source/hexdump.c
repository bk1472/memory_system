#include "common.h"
#include "osadap.h"

#define	GHD_BUFSZ			80
#define END_B				0x01
#define END_L				0x02

void hexdump_fp(FILE *fp, const char *name, void *vcp, int width, int size)
{
	static UCHAR n2h[] = "0123456789abcdef";
	int		i, hpos, cpos;
	int		id[4];
	char	buf[GHD_BUFSZ+1] = {0};
	UCHAR	*cp, uc;
	ULONG	crc_val = 0;

	#ifdef _CYG_WIN_
	snprintf(buf, GHD_BUFSZ, "%s(Size=0x%x, CRC32=0x%08x)", name, size, crc_val);
	#else
	sprintf(buf, "%s(Size=0x%x, CRC32=0x%08x)", name, size, crc_val);
	#endif
	buf[GHD_BUFSZ] = 0;
	fprintf(fp,"%s\n",buf);

	if (size == 0) return;

	memset(buf, ' ', GHD_BUFSZ);

	cp = (UCHAR*)vcp;
	hpos = cpos = 0;
	for (i=0; i < size; ) {

		if ((i % 16) == 0) {
			#ifdef _CYG_WIN_
			snprintf(buf, GHD_BUFSZ, "\t0x%08x(%04x):", (int)vcp+i, i);
			#else
			sprintf(buf, "\t0x%08x(%04x):", (int)vcp+i, i);
			#endif
			hpos = 18;
		}

		if ((i % 4)  == 0) buf[hpos++] = ' ';

		#if   (ENDIAN_TYPE == BIG_ENDIAN   )
		id[0] = 0; id[1] = 1; id[2] = 2; id[3] = 3;
		#elif (ENDIAN_TYPE == LITTLE_ENDIAN)
		switch(width)
		{
			case 1: // 8bit
				id[0] = 0; id[1] = 1; id[2] = 2; id[3] = 3;
				break;
			case 2: //16bit
				id[0] = 1; id[1] = 0; id[2] = 3; id[3] = 2;
				break;
			case 4: //32bit
				id[0] = 3; id[1] = 2; id[2] = 1; id[3] = 0;
				break;
		}
		#else
		#error "Endianess info. is not set!"
		#endif

		if ((i+0)<size) {uc=cp[i+id[0]]; buf[hpos++]=n2h[(uc&0xF0)>>4];buf[hpos++]=n2h[uc&15];}
		if ((i+1)<size) {uc=cp[i+id[1]]; buf[hpos++]=n2h[(uc&0xF0)>>4];buf[hpos++]=n2h[uc&15];}
		if ((i+2)<size) {uc=cp[i+id[2]]; buf[hpos++]=n2h[(uc&0xF0)>>4];buf[hpos++]=n2h[uc&15];}
		if ((i+3)<size) {uc=cp[i+id[3]]; buf[hpos++]=n2h[(uc&0xF0)>>4];buf[hpos++]=n2h[uc&15];}
		//buf[hpos] = ' ';

		cpos = (i%16) + 56;

		if (i<size) {buf[cpos++] = (isprint(cp[i]) ? cp[i] : '.'); i++;}
		if (i<size) {buf[cpos++] = (isprint(cp[i]) ? cp[i] : '.'); i++;}
		if (i<size) {buf[cpos++] = (isprint(cp[i]) ? cp[i] : '.'); i++;}
		if (i<size) {buf[cpos++] = (isprint(cp[i]) ? cp[i] : '.'); i++;}

		if ((i%16) == 0) {
			buf[cpos] = 0x00;
			fprintf(fp,"%s\n", buf);
		}
	}
	buf[cpos] = 0x00;
	if ((i%16) != 0) {
		for ( ; hpos < 56; hpos++)
			buf[hpos] = ' ';
		fprintf(fp,"%s\n", buf);
	}
}

void HEXDUMP(const char *name, void *vcp, int width, int size)
{
	hexdump_fp(stdout, name, vcp, width, size);
}

void hexdump(const char *name, void *vcp, int size)
{
	hexdump_fp(stdout, name, vcp, 4, size);
}
