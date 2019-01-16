/*
    LinkedList implementation which stores a Range as the content of each node in the list
    v20190113IST0200
    HanishKVC, GPL, 19XY
 */

#ifndef _LINKEDLISTRANGE_H_
#define _LINKEDLISTRANGE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct _ll{
	struct _ll *prev, *next;
	int rStart, rEnd;
};

struct LLR{
	struct _ll *llStart;
	struct _ll *llLastAdded;
	struct _ll *llBeforeDel;
	struct _ll *llEnd;
	int iNodeCnt;
	int iTotalFromRanges;
};

void ll_init(struct LLR *me);
struct _ll *_ll_alloc(int start, int end);
#define _ADD_FROM_ADD 0
#define _ADD_FROM_DEL 1
void _ll_add(struct LLR *me, struct _ll *llCur, struct _ll *llNewNext, int iMode);
#define ll_add_sorted ll_add_sorted_startfrom_start
int ll_add_sorted_startfrom_start(struct LLR *me, int start, int end);
int ll_add_sorted_startfrom_lastadded(struct LLR *me, int start, int end);
void _ll_delete(struct LLR *me, struct _ll *llDel);
int ll_delete_core(struct LLR *me, int val, struct _ll *llStartFrom);
//#define ll_delete ll_delete_starthint_start
int ll_delete_starthint_start(struct LLR *me, int val);
#define ll_delete ll_delete_starthint_beforedel
int ll_delete_starthint_beforedel(struct LLR *me, int val);
int ll_free(struct LLR *me);
void ll_print(struct LLR *me, char *sMsg);
void ll_print_content(struct LLR *me, char *sMsg);
void ll_print_summary(struct LLR *me, char *sMsg);
int ll_getdata(struct LLR *me, char *buf, int bufLen, int MaxCnt);
int _ll_save(struct LLR *me, int iFSaveTo);
int ll_save(struct LLR *me, char *sFName);
int ll_load_append(struct LLR *me, char *sFName);
int ll_load(struct LLR *me, char *sFName);

#endif

