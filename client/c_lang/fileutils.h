/*
 * File utils - A library with additional file operations support.
 * v20190113IST1039
 * HanishKVC, GPL, 19XY
 */

#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define FU_BUFLEN 128

struct fu {
	int iFile;
	int iPos;
	int iCnt;
	char buf[FU_BUFLEN];
};

void fu_init(struct fu *me);
int fu_open(struct fu *me, char *sFName, int flags, int mode);
int fu_readline(struct fu *me, char *buf, int bufLen);
int fu_close(struct fu *me);

#endif
