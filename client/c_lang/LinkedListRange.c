/*
    LinkedList implementation which stores a Range as the content of each node in the list
    v20190116IST1538
    HanishKVC, GPL, 19XY
 */

#include <LinkedListRange.h>
#include <fileutils.h>


void ll_init(struct LLR *me) {
	me->llStart = NULL;
	me->llLastAdded = NULL;
	me->llBeforeDel = NULL;
	me->llEnd = NULL;
	me->iNodeCnt = 0;
	me->iTotalFromRanges = 0;
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

void _ll_add(struct LLR *me, struct _ll *llCur, struct _ll *llNewNext, int iMode) {

	if (llNewNext == NULL) {
		fprintf(stderr, "WARN:%s: A NULL node given for adding, returning\n", __func__);
		return;
	}
	me->iNodeCnt += 1;
	if (iMode == _ADD_FROM_ADD) {
		me->iTotalFromRanges += ((llNewNext->rEnd - llNewNext->rStart)+1);
	}
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
	_ll_add(me, llPrev, llTemp, _ADD_FROM_ADD);
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
	_ll_add(me, llPrev, llTemp, _ADD_FROM_ADD);
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
		free(llDel);
		return;
	}
	llPrev = llDel->prev;
	llNext = llDel->next;
	llPrev->next = llNext;
	if (llNext != NULL) {
		llNext->prev = llPrev;
	}
	free(llDel);
}

int ll_delete_core(struct LLR *me, int val, struct _ll *llStartFrom) {
	struct _ll *llTemp;
	struct _ll *llNext;

	llNext = llStartFrom;
	me->iTotalFromRanges -= 1;
	while(llNext != NULL) {
		if (llNext->rStart == val) {
			me->llBeforeDel = llNext->prev;
			if (llNext->rEnd == val) {
				_ll_delete(me, llNext);
				return 0;
			} else {
				llNext->rStart += 1;
				return 0;
			}
		}
		if (llNext->rEnd == val) {
			me->llBeforeDel = llNext->prev;
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
			me->llBeforeDel = llNext->prev;
			int iEnd = llNext->rEnd;
			llNext->rEnd = val-1;
			llTemp = _ll_alloc(val+1, iEnd);
			_ll_add(me, llNext, llTemp, _ADD_FROM_DEL);
			return 0;
		}
		llNext = llNext->next;
	}
	me->iTotalFromRanges += 1;
	return -1;
}

int ll_delete_starthint_start(struct LLR *me, int val) {
	return ll_delete_core(me, val, me->llStart);
}

int ll_delete_starthint_beforedel(struct LLR *me, int val) {
	if (me->llBeforeDel == NULL) {
		return ll_delete_core(me, val, me->llStart);
	}
	if (me->llBeforeDel->rStart > val) {
		return ll_delete_core(me, val, me->llStart);
	}
	return ll_delete_core(me, val, me->llBeforeDel);
}

int ll_free(struct LLR *me) {
	struct _ll *llNext;
	struct _ll *llTemp;

	llNext = me->llStart;
	while(llNext != NULL) {
		llTemp = llNext;
		llNext = llNext->next;
		me->iTotalFromRanges -= (llTemp->rEnd-llTemp->rStart+1);
		me->iNodeCnt -= 1;
		// The below logic is just to keep the structure always consistant. But technically this is not required, as this will be freeing up the full ll.
		me->llStart = llNext;
		if (llNext != NULL) {
			llNext->prev = NULL;
		}
		if (me->llLastAdded == llTemp) {
			me->llLastAdded = NULL;
		}
		if (me->llBeforeDel == llTemp) {
			me->llBeforeDel = NULL;
		}
		free(llTemp);
	}
	me->llEnd = NULL;
	return 0;
}

void ll_print(struct LLR *me, char *sMsg) {

	ll_print_content(me, sMsg);
	ll_print_summary(me, sMsg);
}

void ll_print_content(struct LLR *me, char *sMsg) {

	printf("**** [%s] LinkedList Content ****\n", sMsg);
	struct _ll *llNext = me->llStart;
	while(llNext != NULL) {
		printf("%d-%d\n", llNext->rStart, llNext->rEnd);
		llNext = llNext->next;
	}
	printf("****    ****    ****    ****\n");
}

void ll_print_summary(struct LLR *me, char *sMsg) {

	printf("**** [%s] LinkedList Summary ****\n", sMsg);
	printf("NodeCnt: %d\n", me->iNodeCnt);
	printf("TotalFromRanges: %d\n", me->iTotalFromRanges);
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
	printf("****    ****    ****    ****\n");
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
	snprintf(tBuf, 128, "IRangesCnt-%d\nILostPkts-%d\n", me->iNodeCnt, me->iTotalFromRanges);
	strncat(buf, tBuf, bufLen);
	return iCnt;
}

int _ll_save(struct LLR *me, int iFSaveTo) {
	char tBuf[128];
	int iRet;

	struct _ll *llNext = me->llStart;
	int iCnt = 0;
	while(llNext != NULL) {
		iCnt += 1;
		snprintf(tBuf, 128, "%d-%d\n", llNext->rStart, llNext->rEnd);
		iRet = write(iFSaveTo, tBuf, strlen(tBuf));
		if (iRet == -1) {
			perror("ERROR:LLR:Save:Write");
			break;
		}
		llNext = llNext->next;
	}
	return iCnt;
}

int ll_save(struct LLR *me, char *sFName) {
	int iCnt;

	int iFSaveTo = open(sFName, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	if (iFSaveTo == -1) {
		perror("ERROR:LLR:Save:Open");
		return -1;
	}
	iCnt = _ll_save(me, iFSaveTo);
	close(iFSaveTo);
	return iCnt;
}

#define LL_BUFLEN 64
int ll_load_append_ex(struct LLR *me, char *sFName, char cMarker, marker_callback marker_cb, void *markerContext) {
	char tBuf[LL_BUFLEN];
	int iRet;
	char *begin, *rem;
	int iStart, iEnd;
	int iPos;
	struct fu fuLoad;

	fu_init(&fuLoad);

	iRet = fu_open(&fuLoad, sFName, O_RDWR, 0);
	if (iRet == -1) {
		fprintf(stderr, "ERROR:%s: failed for [%s]\n", __func__, sFName);
		return -1;
	}
	do {
		iRet = fu_readline(&fuLoad, tBuf, LL_BUFLEN);
		//fprintf(stderr, "DEBUG:%s:[%d][%s]\n", __func__, iRet, tBuf);
		if (iRet <= 0) {
			break;
		}
		if (tBuf[0] == cMarker) {
			if (marker_cb != NULL) {
				marker_cb(tBuf, iRet, markerContext);
			}
			continue;
		}
		iPos = 0;
		begin = &tBuf[iPos];
		iStart = strtol(&tBuf[iPos], &rem, 10);
		iPos = rem - begin + 1;
		begin = &tBuf[iPos];
		iEnd = strtol(&tBuf[iPos], &rem, 10);
		iPos += (rem - begin + 1);
		iRet = iRet - iPos;
		if (iRet != 0) {
			fprintf(stderr, "DEBUG:%s: after parsing a range, end of line not reached\n", __func__);
		}
		// Ideally one can use add_sorted_startfrom_lastadded, because
		// the ll_save will have saved a sorted linked list (unless
		// while adding into that list originally there was a goof up),
		// which  ensures that each new range read from the save file
		// will be getting added such that it follows the last node added.
		// However in case some one edits the file manually and forgets
		// to keep it sorted, then to avoid any issues, for now I am
		// using the slightly inefficient startfrom_start which will
		// search from beginning of the linked list each time the new
		// range is being added into the list.
		ll_add_sorted(me, iStart, iEnd);
		//fprintf(stderr, "%d-%d\n", iStart, iEnd);
	} while(1);
	fu_close(&fuLoad);
	return iRet;
}

int ll_load_append(struct LLR *me, char *sFName) {
	return ll_load_append_ex(me, sFName, '#', NULL, NULL);
}

int ll_load(struct LLR *me, char *sFName) {
	ll_init(me);
	return ll_load_append(me, sFName);
}

#ifdef MODE_PROGRAM_LL

int main(int argc, char **argv) {
	struct LLR theLLR;
	int iStart = 0;

	ll_init(&theLLR);
	ll_print_summary(&theLLR, "Testing LLR - After Init");
	iStart = 1000;
	for(int iCnt = 0; iCnt < (1024-10); iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	ll_print_summary(&theLLR, "Testing LLR - After initial additions");
	iStart = 0;
	for(int iCnt = 0; iCnt < 10; iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	ll_print_summary(&theLLR, "Testing LLR - After additions which should go at begining");
	iStart = 2050;
	for(int iCnt = 0; iCnt < 10; iCnt++) {
		iStart += 100;
		ll_add_sorted(&theLLR, iStart, iStart+20);
	}
	ll_print_summary(&theLLR, "Testing LLR - After additions which should go in-between");
	ll_print(&theLLR, "Testing LLR - After all added");

	for(int iCnt = 1000; iCnt < (1024-10)*100; iCnt++) {
		ll_delete(&theLLR, iCnt);
	}
	ll_print_summary(&theLLR, "Testing LLR - After deleting from 1000 to (1024-10)*100");
	for(int iCnt = 0; iCnt < 1024; iCnt++) {
		ll_delete(&theLLR, iCnt);
	}
	ll_print_summary(&theLLR, "Testing LLR - After deleting from 0 to 1024");
	ll_print(&theLLR, "Testing LLR - After all deletes");

	// Test save and load
	ll_save(&theLLR, "/tmp/t.llr.100");
	ll_free(&theLLR);
	ll_print(&theLLR, "Testing LLR - After save and free");

	ll_load(&theLLR, "/tmp/t.llr.100");
	ll_print(&theLLR, "Testing LLR - After load");

	return 0;
}

#endif

