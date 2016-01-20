#include "common.h"
#include "osadap.h"

#define DBG_BUFSZ			 256
#define	GHD_BUFSZ			  80

#define	CRNL_MASK			0x01
#define	HEAD_MASK			0x02
#define	TIME_MASK			0x04

#define	DBG_CRNL			"\n"

#define	DBG_FMT_R0N			(                                0)
#define	DBG_FMT_R1N			(                        CRNL_MASK)

#define	DBG_FMT_DEF0N		DBG_FMT_RH0N
#define	DBG_FMT_DEF1N		DBG_FMT_RH1N

static char *dbg_format[4] = {
			"%s%s%s",								//	DBG_FMT_R0N
			"%s%s%s"					DBG_CRNL,	//	DBG_FMT_R1N
			"%s%s%s\n",								//	DBG_FMT_R0N
			"%s%s%s\n"					DBG_CRNL	//	DBG_FMT_R1N
};

static char	*clrStrings[] = {   /* color    : index: Value      */
			"\x1b[30m",         /* blac[k]  :    0 : 30         */
			"\x1b[31m",         /* [r]ed    :    1 : 31         */
			"\x1b[32m",         /* [g]reen  :    2 : 32         */
			"\x1b[33m\x1b[40m", /* [y]ellow :    3 : 33         */
			"\x1b[34m",         /* [b]lue   :    4 : 34         */
			"\x1b[35m",         /* [p]urple :    5 : 35         */
			"\x1b[36m",         /* [c]yan   :    6 : 36         */
			"\x1b[37m",         /* gr[a]y   :    7 : 37==0 -> x */
			"\x1b[37m\x1b[40m", /* blac[K]  :    0 : 40         */
			"\x1b[37m\x1b[41m", /* [R]ed    :    1 : 41         */
			"\x1b[30m\x1b[42m", /* [G]reen  :    2 : 42         */
			"\x1b[30m\x1b[43m", /* [Y]ellow :    3 : 43         */
			"\x1b[37m\x1b[44m", /* [B]lue   :    4 : 44         */
			"\x1b[37m\x1b[45m", /* [P]urple :    5 : 45         */
			"\x1b[30m\x1b[46m", /* [C]yan   :    6 : 46         */
			"\x1b[37m\x1b[40m", /* gr[A]y   :    7 : 47==0 -> x */
			"\x1b[0m"           /* Reset  : Reset fg coloring */
			};

//
//	Named Debug Print Utility
//		Prepend the name of calling task by using t_ident system call
//		It may be slower than normal.
//		But, it's O.K, we are to debug almost all problems.
//
//	Note:	Increase the stack size of idle task.
//
static int __DBGvsprint(void *head, const char *format , va_list args)
{
	char	dbgbuf[DBG_BUFSZ];
	char	*color1 = (char *)"";
	char	*color2 = (char *)"";
    SINT32	count   = 0;
	UINT32  type;
	void	(*pFunc)() = (void (*)(void))printf;

	if (*format == '!')
		return 0;

	/* get color value and skip color code in debug print */

	if ( (format[0]=='^') && (format[2]=='^') )
	{
	#ifndef _CYG_WIN_
		format += 3;
	#else
		int	colorIndex = -1;
		switch (format[1])
		{
			case 'k': case 'K': colorIndex = 0; break;
			case 'r': case 'R': colorIndex = 1; break;
			case 'g': case 'G': colorIndex = 2; break;
			case 'y': case 'Y': colorIndex = 3; break;
			case 'b': case 'B': colorIndex = 4; break;
			case 'p': case 'P': colorIndex = 5; break;
			case 'c': case 'C': colorIndex = 6; break;
			case 'a': case 'A': colorIndex = 7; break;
		}

		if (colorIndex >= 0)
		{
			if (!(format[1] & 0x20)) colorIndex += 8;
			format += 3;
			color1  = clrStrings[colorIndex];
			color2  = clrStrings[16];
		}
	#endif
	}
	#ifdef _CYG_WIN_
   	count = vsnprintf( &dbgbuf[0], DBG_BUFSZ-14, format, args );
	#else
   	count = vsprintf( &dbgbuf[0], format, args );
	#endif
	type  = (ULONG) head;

	/* Because of ARM compiler bug, one computation is	*/
	/* moved to below in switch statement				*/

	switch (type)
	{
	  case DBG_FMT_R0N:
	  case DBG_FMT_R1N:
		(*pFunc)(dbg_format[type], color1, dbgbuf, color2);
		break;
    }

    return count;
}

//	Raw(non-named) debug print without ending newline
int	rprint0n(const char *format , ... )
{
    int		count;
    va_list	ap;

    va_start( ap, format);
    count = __DBGvsprint((void *)DBG_FMT_R0N, format, ap);
    va_end(ap);
    return count;
}


//	Raw(non-named) debug print with ending newline
int	rprint1n(const char *format , ... )
{
    int		count;
    va_list	ap;

    va_start( ap, format);
    count = __DBGvsprint((void *)DBG_FMT_R1N, format, ap);
    va_end(ap);
    return count;
}

//	Raw(non-named) debug print with ending newline
int	dbgprint(const char *format , ... )
{
    int		count;
    va_list	ap;

    va_start( ap, format);
    count = __DBGvsprint((void *)DBG_FMT_R1N, format, ap);
    va_end(ap);
    return count;
}
