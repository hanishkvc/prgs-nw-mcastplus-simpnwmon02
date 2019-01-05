/*
    LinkedList implementation which stores a Range as the content of each node in the list
    v20190103IST1703
    HanishKVC, GPL, 19XY
 */

#ifndef _LINKEDLISTRANGE_H_
#define _LINKEDLISTRANGE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _ll{
	struct _ll *prev, *next;
	int rStart, rEnd;
};

struct LLR{
	struct _ll *llStart;
	int iNodeCnt;
};

void ll_init(struct LLR *me);
struct _ll *_ll_alloc(int start, int end);
void _ll_add(struct LLR *me, struct _ll *llCur, struct _ll *llNewNext);
int ll_add_sorted(struct LLR *me, int start, int end);
void _ll_delete(struct LLR *me, struct _ll *llDel);
int ll_delete(struct LLR *me, int val);
int ll_free(struct LLR *me);
void ll_print(struct LLR *me);
int ll_getdata(struct LLR *me, char *buf, int bufLen, int MaxCnt);

#endif

