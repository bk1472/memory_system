#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef _CYG_WIN_
#include <sys/mman.h>
#endif


#ifndef _CYG_WIN_
#define LITTLE_ENDIAN	0
#define BIG_ENDIAN		1

#define ENDIAN_TYPE		LITTLE_ENDIAN

#endif
/*-----------------------------------------------------------------------------
	형 정의
	(Type Definitions)
------------------------------------------------------------------------------*/
typedef signed char			SINT8;
typedef signed char			SINT08;
typedef signed short		SINT16;
typedef signed long			SINT32;

typedef unsigned char		UINT8;
typedef unsigned char		UINT08;
typedef unsigned short		UINT16;
typedef unsigned long		UINT32;

typedef unsigned char		UCHAR;
typedef unsigned short		USHORT;
typedef unsigned long		ULONG;

typedef	struct ls_elt {
	struct ls_elt	*ls_next;
	struct ls_elt	*ls_prev;
} lst_t;


#ifndef _CYG_WIN_
typedef int					off_t;

#define PROT_READ			0x01
#define PROT_WRITE			0x02
#define PROT_EXEC			0x04

#define MAP_PRIVATE			0x01
#define MAP_ANONYMOUS		0x02
#endif

/*-----------------------------------------------------------------------------
	매크로 함수 정의
	(Macro Definitions)
------------------------------------------------------------------------------*/
#define	LS_ISEMPTY(listp)													\
		(((lst_t *)(listp))->ls_next == (lst_t *)(listp))

#define	LS_INIT(listp) {													\
		((lst_t *)(&(listp)[0]))->ls_next = 								\
		((lst_t *)(&(listp)[0]))->ls_prev = ((lst_t *)(&(listp)[0]));		\
}

#define	LS_INS_BEFORE(oldp, newp) {											\
		((lst_t *)(newp))->ls_prev = ((lst_t *)(oldp))->ls_prev;			\
		((lst_t *)(newp))->ls_next = ((lst_t *)(oldp));						\
		((lst_t *)(newp))->ls_prev->ls_next = ((lst_t *)(newp));			\
		((lst_t *)(newp))->ls_next->ls_prev = ((lst_t *)(newp));			\
}

#define	LS_INS_AFTER(oldp, newp) {											\
		((lst_t *)(newp))->ls_next = ((lst_t *)(oldp))->ls_next;			\
		((lst_t *)(newp))->ls_prev = ((lst_t *)(oldp));						\
		((lst_t *)(newp))->ls_next->ls_prev = ((lst_t *)(newp));			\
		((lst_t *)(newp))->ls_prev->ls_next = ((lst_t *)(newp));			\
}

#define	LS_REMQUE(remp) {													\
	if (!LS_ISEMPTY(remp)) {												\
		((lst_t *)(remp))->ls_prev->ls_next = ((lst_t *)(remp))->ls_next;	\
		((lst_t *)(remp))->ls_next->ls_prev = ((lst_t *)(remp))->ls_prev;	\
		LS_INIT(remp);														\
	}																		\
}

#define LS_BASE(type, basep) (type *) &(basep)[0]

#ifdef	__COMMENT_OUT
/*---	Example of doubly linked list	---*/

		typedef struct LINKED_LIST_tag {
			struct LINKED_LIST_tag	*next;
			struct LINKED_LIST_tag	*prev;

			int	data;
		} LINKED_LIST;

		LINKED_LIST	*list[2];		//	declaration of header
		LS_INIT(list);			//	initialize

		LINKED_LIST *list_base = LS_BASE(LINKED_LIST, list);
		//LINKED_LIST *list_base = (LINKED_LIST *) &list[0]; //	type cast - (*)

		LINKED_LIST *list_data = list_base->next;

		while (list_data != list_base) {
			//	traverse
			list_data = list_data->next;
		}

	(*)		*list[0] --> *next, *list[1] --> *prev
			i.e. Header has only links without data.
#endif	/* __COMMENT_OUT */

#ifdef __cplusplus
}
#endif

#endif/*__COMMON_H__*/
