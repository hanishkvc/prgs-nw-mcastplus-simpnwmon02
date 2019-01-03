/*
    Simple Network Monitor 02 - C version
    v20190103IST1715
    HanishKVC, GPL, 19XY
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <LinkedListRange.h>

const int STATS_TIMEDELTA=20;
const int MCAST_USLEEP=1000;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_DATA_OFFSET=4;

int portMCast=1111;
int portServer=1112;
int portClient=1113;

//char *sLocalAddr="0.0.0.0";
char *sPIInitAddr="255.255.255.255";

#define BUF_MAXSIZE 1600
int giDataSize=(1024+4);
char gcBuf[BUF_MAXSIZE];

int gbDoMCast = 1;

#define ENABLE_MULTICAST_ALL 1
int sock_mcast_init_ex(int ifIndex, char *sMCastAddr, int port, char *sLocalAddr)
{
	int iRet;
	int sockMCast = -1;
	struct ip_mreqn mreqn;
	struct sockaddr_in myAddr;

	sockMCast = socket(AF_INET, SOCK_DGRAM, 0);
	
	iRet = inet_aton(sMCastAddr, &mreqn.imr_multiaddr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s:4JoinMCast: Failed to set MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
		exit(-1);
	}
	//fprintf(stderr, "INFO:%s: for IP_ADD_MEMBERSHIP set MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
	iRet = inet_aton(sLocalAddr, &mreqn.imr_address);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s:4JoinMCast: Failed to set localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
		exit(-1);
	}
	//fprintf(stderr, "INFO:%s: for IP_ADD_MEMBERSHIP set localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
	mreqn.imr_ifindex = ifIndex;
	iRet = setsockopt(sockMCast, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: Failed joining group[%s] with localAddr[%s] & ifIndex[%d], ret=[%d]\n", __func__, sMCastAddr, sLocalAddr, ifIndex, iRet);
		perror("Failed joining group:");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: sockMCast [%d] joined [%s] on (LocalAddr [%s] & ifIndex [%d]), ret=[%d]\n", __func__, sockMCast, sMCastAddr, sLocalAddr, ifIndex, iRet);

#ifdef ENABLE_MULTICAST_ALL
	uint8_t iEnable = 1;
	iRet = setsockopt(sockMCast, IPPROTO_IP, IP_MULTICAST_ALL, &iEnable, sizeof(iEnable));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: Failed Enabling MulticastALL, ret=[%d]\n", __func__, iRet);
		perror("Failed MulticastALL:");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: Enabled MulticastALL, ret=[%d]\n", __func__, iRet);
#endif

	myAddr.sin_family=AF_INET;
	myAddr.sin_port=htons(port);
	myAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	iRet = bind(sockMCast, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (iRet < 0) {
		fprintf(stderr, "ERROR:%s: Failed bind localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, INADDR_ANY, port, iRet);
		perror("Failed bind:");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: sockMCast [%d] bound to localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, sockMCast, INADDR_ANY, port, iRet);
	return sockMCast;
}


int filedata_save(int fileData, char *buf, int iBlockOffset, int len) {
	int iRet;
	int iExpectedDataLen = giDataSize-PKT_DATA_OFFSET;

	if (len != iExpectedDataLen) {
		fprintf(stderr, "WARN:%s:ExpectedDataSize[%d] != pktDataLen[%d]\n", __func__, iExpectedDataLen, len);
	}
	iRet = lseek(fileData, iBlockOffset*giDataSize, SEEK_SET);
	if (iRet == -1) {
		perror("WARN:filedata_save:lseek failed:");
		return -1;
	}
	iRet = write(fileData, buf, len);
	if (iRet == -1) {
		perror("WARN:filedata_save:write failed:");
		return -1;
	}
	if (iRet != len) {
		fprintf(stderr, "WARN:%s:wrote less: block[%d] got[%d] wrote[%d]\n", __func__, iBlockOffset, len, iRet);
		return -1;
	}
	return 0;
}

int mcast_recv(int sockMCast, int fileData, struct LLR *llLostPkts) {

	int iRet;
	int iPrevSeq = -1;
	int iSeq = 0;
	int iPktCnt = 0;
	int iOldSeqs = 0;
	int iDisjointSeqs = 0;
	int iDisjointPktCnt = 0;
	int iSeqDelta;
	time_t prevSTime, curSTime;
	time_t prevDTime, curDTime;
	int iDataBytesPerSec = 0;
	int iPrevPktCnt = 0;

	prevSTime = time(NULL);
	prevDTime = prevSTime;
	while (gbDoMCast) {

		iRet = recvfrom(sockMCast, gcBuf, giDataSize, MSG_DONTWAIT, NULL, NULL);
		curSTime = time(NULL);
		if ((curSTime-prevSTime) > STATS_TIMEDELTA) {
			int iDeltaPkts = iPktCnt - iPrevPktCnt;
			int iDeltaTimeSecs = curDTime - prevDTime;
			if (iDeltaTimeSecs == 0) iDeltaTimeSecs = 1;
			iDataBytesPerSec = (iDeltaPkts*giDataSize)/iDeltaTimeSecs;
			fprintf(stderr, "INFO:%s: iPktCnt[%d] iSeqNum[%d] DataDelay[%ld] iDisjointSeqs[%d] iDisjointPktCnt[%d] iOldSeqs[%d] DataBPS[%d]\n",
				__func__, iPktCnt, iSeq, (curSTime-curDTime), iDisjointSeqs, iDisjointPktCnt, iOldSeqs, iDataBytesPerSec);
			prevSTime = curSTime;
			iPrevPktCnt = iPktCnt;
			prevDTime = curDTime;
		}
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep(MCAST_USLEEP);
			} else {
				perror("WARN:mcast_recv:revfrom failed:");
			}
			continue;
		}
		curDTime = time(NULL);
		iPktCnt += 1;
		iSeq = *((uint32_t*)&gcBuf[PKT_SEQNUM_OFFSET]);
		iSeqDelta = iSeq - iPrevSeq;
		if (iSeqDelta <= 0) {
			iOldSeqs += 1;
			continue;
		}
		if (iSeqDelta > 1) {
			//fprintf(stderr, "DEBUG:%s: iSeq[%d] iPrevSeq[%d] iSeqDelta[%d] iDisjointSeqs[%d] iDisjointPktCnt[%d]\n", __func__, iSeq, iPrevSeq, iSeqDelta, iDisjointSeqs, iDisjointPktCnt);
			ll_add_sorted(llLostPkts, iPrevSeq+1, iSeq-1);
			iDisjointSeqs += 1;
			iDisjointPktCnt += (iSeqDelta-1);
		}
		if (fileData != -1) {
			filedata_save(fileData, &gcBuf[PKT_DATA_OFFSET], iSeq, iRet-PKT_DATA_OFFSET);
		}
		iPrevSeq = iSeq;

	}

	return 0;
}


#define ARG_IFINDEX 1
#define ARG_MCASTADDR 2
#define ARG_LOCALADDR 3
#define ARG_DATAFILE 4
#define ARG_COUNT (4+1)

int main(int argc, char **argv) {

	int sockMCast = -1;
	struct LLR llLostPkts;

	if (argc < ARG_COUNT) {
		fprintf(stderr, "usage: %s <ifIndex4mcast> <mcast_addr> <local_addr> <datafile>\n", argv[0]);
		exit(-1);
	}

	int ifIndex = strtol(argv[ARG_IFINDEX], NULL, 0);
	char *sMCastAddr = argv[ARG_MCASTADDR];
	char *sLocalAddr = argv[ARG_LOCALADDR];
	int fileData = open(argv[ARG_DATAFILE], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); // Do I need truncate to think later. Also if writing to device files, then have to re-evaluate the flags
	if (fileData == -1) {
		fprintf(stderr, "WARN:%s: Failed to open data file [%s], saving data will be skipped\n", __func__, argv[ARG_DATAFILE]);
		perror("Failed datafile open");
	} else {
		fprintf(stderr, "INFO:%s: opened data file [%s]\n", __func__, argv[ARG_DATAFILE]);
	}

	ll_init(&llLostPkts);
	sockMCast = sock_mcast_init_ex(ifIndex, sMCastAddr, portMCast, sLocalAddr);
	mcast_recv(sockMCast, fileData, &llLostPkts);
	ll_print(&llLostPkts);
	ll_free(&llLostPkts);

	return 0;
}

