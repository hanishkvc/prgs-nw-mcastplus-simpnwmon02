/*
 * File utils - A library with additional file operations support.
 * v20190113IST1039
 * HanishKVC, GPL, 19XY
 */

#include <stdio.h>

#include <fileutils.h>

void fu_init(struct fu *me) {
	me->iFile = -1;
	me->iPos = 0;
	me->iCnt = 0;
}

int fu_open(struct fu *me, char *sFName, int flags, int mode) {
	me->iFile = open(sFName, flags, mode);
	if (me->iFile == -1) {
		perror("ERROR:fu_open:open failed");
	}
	return me->iFile;
}

int fu_readline(struct fu *me, char *buf, int bufLen) {
	int iPos = 0;
	for(; iPos < bufLen; iPos++) {
		if (me->iCnt == 0) {
			me->iCnt = read(me->iFile, me->buf, FU_BUFLEN);
			me->iPos = 0;
			if (me->iCnt == -1) {
				perror("ERROR:fu_readline:read failed");
			}
		}
		if (me->iCnt <= 0) {
			return iPos;
		}
		buf[iPos] = me->buf[me->iPos];
		me->iPos += 1;
		me->iCnt -= 1;
		if (buf[iPos] == '\n') {
			return (iPos+1);
		}
	}
	return iPos;
}

int fu_close(struct fu *me) {
	me->iPos = 0;
	return close(me->iFile);
}

