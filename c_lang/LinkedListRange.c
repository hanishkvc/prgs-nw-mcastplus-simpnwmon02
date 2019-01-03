#include <stdio.h>
#include <stdlib.h>

struct _ll{
	struct _ll *prev, *next;
	int rStart, rEnd;
};

struct LLR{
	struct _ll *llStart;
};


void ll_init(struct LLR *me) {
	me->llStart = NULL;
}

struct _ll *_ll_alloc(int start, int end)
{
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

int ll_add_sorted(struct LLR *me, int start, int end) {
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
	if (llPrev == NULL) {
		me->llStart = llTemp;
		me->llStart->prev = NULL;
		me->llStart->next = llPrev;
	} else {
		llTemp->prev = llPrev;
		llTemp->next = llPrev->next;
		llPrev->next = llTemp;
		if (llTemp->next != NULL) {
			llTemp->next->prev = llTemp;
		}
	}
	fprintf(stderr, "INFO:%s: Added [%d-%d]\n", __func__, start, end);
	return 0;
}

void _ll_delete(struct LLR *me, struct _ll *llDel) {
	struct _ll *llNext, *llPrev;
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
			llTemp->prev = llNext;
			llTemp->next = llNext->next;
			llNext->next = llTemp;
			if (llTemp->next != NULL) {
				llTemp->next->prev = llTemp;
			}
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
	while(llNext != NULL) {
		printf("%d-%d\n", llNext->rStart, llNext->rEnd);
		llNext = llNext->next;
	}
}


#ifdef MODE_PROGRAM_LL

int main(int argc, char **argv) {
	struct LLR theLLR;
	int iStart = 0;

	ll_init(&theLLR);
	for(int iCnt = 0; iCnt < 1024; iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	ll_print(&theLLR);
	ll_free(&theLLR);
	return 0;
}

#endif
