/*
    LinkedList implementation which stores a Range as the content of each node in the list
    v20190105IST1051
    HanishKVC, GPL, 19XY
 */

#include <LinkedListRange.h>


void ll_init(struct LLR *me) {
	me->llStart = NULL;
	me->llLastAdded = NULL;
	me->llEnd = NULL;
	me->iNodeCnt = 0;
}

struct _ll *_ll_alloc(int start, int end) {
	struct _ll *llTemp;
	llTemp = malloc(sizeof(struct _ll));
	if (llTemp == NULL) {
		fprintf(stderr, "ERROR:%s: NoMemory to allocate new node for [%d-%d]\n", __func__, start, end);
		return NULL;
	}
	llTemp->rStart = start;
	llTemp->rEnd = end;
	llTemp->prev = NULL;
	llTemp->next = NULL;
	return llTemp;
}

void _ll_update_end_for_add(struct LLR *me, struct _ll *new) {
	if (new->next == NULL) {
		me->llEnd = new;
	}
}

void _ll_add(struct LLR *me, struct _ll *llCur, struct _ll *llNewNext) {

	if (llNewNext == NULL) {
		fprintf(stderr, "WARN:%s: A NULL node given for adding, returning\n", __func__);
		return;
	}
	me->iNodeCnt += 1;
	me->llLastAdded = llNewNext;
	if (llCur == NULL) {
		if (me->llStart != NULL) {
			fprintf(stderr, "DEBUG:%s: Adding entry to begin of LL, [%d-%d]\n", __func__, llNewNext->rStart, llNewNext->rEnd);
			me->llStart->prev = llNewNext;
			llNewNext->next = me->llStart;
			llNewNext->prev = NULL;
			me->llStart = llNewNext;
		} else {
			fprintf(stderr, "DEBUG:%s: Adding first entry for this LL, [%d-%d]\n", __func__, llNewNext->rStart, llNewNext->rEnd);
			me->llStart = llNewNext;
			llNewNext->prev = NULL;
			llNewNext->next = NULL;
		}
		_ll_update_end_for_add(me, llNewNext);
		return;
	}
	llNewNext->prev = llCur;
	llNewNext->next = llCur->next;
	llCur->next = llNewNext;
	if (llNewNext->next != NULL) {
		llNewNext->next->prev = llNewNext;
	}
	_ll_update_end_for_add(me, llNewNext);
}

int ll_add_sorted_startfrom_start(struct LLR *me, int start, int end) {
	struct _ll *llTemp;
	struct _ll *llNext, *llPrev;

	llTemp = _ll_alloc(start, end);
	llPrev = NULL;
	llNext = me->llStart;
	while (llNext != NULL) {
		if (llNext->rStart > end) {
			break;
		}
		llPrev = llNext;
		llNext = llNext->next;
	}
	_ll_add(me, llPrev, llTemp);
	fprintf(stderr, "INFO:%s: Added [%d-%d]\n", __func__, start, end);
	return 0;
}

int ll_add_sorted_startfrom_lastadded(struct LLR *me, int start, int end) {
	struct _ll *llTemp;
	struct _ll *llNext, *llPrev;

	llTemp = _ll_alloc(start, end);
	if (me->llLastAdded == NULL) {
		return ll_add_sorted_startfrom_start(me, start, end);
	}
	llPrev = me->llLastAdded->prev;
	llNext = me->llLastAdded;
	while (llNext != NULL) {
		if (llNext->rStart > end) {
			break;
		}
		llPrev = llNext;
		llNext = llNext->next;
	}
	_ll_add(me, llPrev, llTemp);
	fprintf(stderr, "INFO:%s: Added [%d-%d]\n", __func__, start, end);
	return 0;
}

void _ll_delete(struct LLR *me, struct _ll *llDel) {
	struct _ll *llNext, *llPrev;
	me->iNodeCnt -= 1;
	if (me->llLastAdded == llDel) {
		me->llLastAdded = NULL;
	}
	if (me->llEnd == llDel) {
		me->llEnd = llDel->prev;
	}
	if (me->llStart == llDel) {
		me->llStart = llDel->next;
		if (me->llStart != NULL) {
			me->llStart->prev = NULL;
		}
		return;
	}
	llPrev = llDel->prev;
	llNext = llDel->next;
	llPrev->next = llNext;
	if (llNext != NULL) {
		llNext->prev = llPrev;
	}
}

int ll_delete(struct LLR *me, int val) {
	struct _ll *llTemp;
	struct _ll *llNext, *llPrev;

	llPrev = NULL;
	llNext = me->llStart;
	while(llNext != NULL) {
		if (llNext->rStart == val) {
			if (llNext->rEnd == val) {
				_ll_delete(me, llNext);
				return 0;
			} else {
				llNext->rStart += 1;
				return 0;
			}
		}
		if (llNext->rEnd == val) {
			if (llNext->rStart == val) {
				// shouldn't reach here, it should have matched the (start==val => end==val) test above
				fprintf(stderr, "DEBUG:%s: Shouldn't reach here for removing [%d]\n", __func__, val);
				exit(-1);
			} else {
				llNext->rEnd -= 1;
				return 0;
			}
		}
		if ((llNext->rStart < val) && (llNext->rEnd > val)) {
			int iEnd = llNext->rEnd;
			llNext->rEnd = val-1;
			llTemp = _ll_alloc(val+1, iEnd);
			_ll_add(me, llNext, llTemp);
			return 0;
		}
		llNext = llNext->next;
	}
	return -1;
}

int ll_free(struct LLR *me) {
	struct _ll *llNext;
	struct _ll *llTemp;

	llNext = me->llStart;
	while(llNext != NULL) {
		llTemp = llNext;
		llNext = llNext->next;
		free(llTemp);
		// The below logic is just to keep the structure always consistant. But technically this is not required, as this will be freeing up the full ll.
		me->llStart = llNext;
		if (llNext != NULL) {
			llNext->prev = NULL;
		}
	}
	return 0;
}

void ll_print(struct LLR *me) {

	struct _ll *llNext = me->llStart;
	printf("**** LinkedList Content ****\n");
	while(llNext != NULL) {
		printf("%d-%d\n", llNext->rStart, llNext->rEnd);
		llNext = llNext->next;
	}
	printf("NodeCnt: %d\n", me->iNodeCnt);
	if (me->llStart != NULL) {
		printf("StartNode: %d-%d\n", me->llStart->rStart, me->llStart->rEnd);
	} else {
		printf("StartNode: NULL\n");
	}
	if (me->llLastAdded != NULL) {
		printf("LastAddedNode: %d-%d\n", me->llLastAdded->rStart, me->llLastAdded->rEnd);
	} else {
		printf("LastAddedNode: NULL\n");
	}
	if (me->llEnd != NULL) {
		printf("EndNode: %d-%d\n", me->llEnd->rStart, me->llEnd->rEnd);
	} else {
		printf("EndNode: NULL\n");
	}
}

int ll_getdata(struct LLR *me, char *buf, int bufLen, int MaxCnt) {
	char tBuf[128];

	int iCnt = 0;
	memset(buf, 0, bufLen);
	struct _ll *llNext = me->llStart;
	while(llNext != NULL) {
		iCnt += 1;
		snprintf(tBuf, 128, "%d-%d\n", llNext->rStart, llNext->rEnd);
		strncat(buf, tBuf, bufLen);
		llNext = llNext->next;
		if (iCnt >= MaxCnt) {
			break;
		}
	}
	return iCnt;
}

#ifdef MODE_PROGRAM_LL

int main(int argc, char **argv) {
	struct LLR theLLR;
	int iStart = 0;

	ll_init(&theLLR);
	iStart = 1000;
	for(int iCnt = 0; iCnt < (1024-10); iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	iStart = 0;
	for(int iCnt = 0; iCnt < 10; iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	iStart = 2050;
	for(int iCnt = 0; iCnt < 10; iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	ll_print(&theLLR);
	ll_free(&theLLR);
	return 0;
}

#endif

