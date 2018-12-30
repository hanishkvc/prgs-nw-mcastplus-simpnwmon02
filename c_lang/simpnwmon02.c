#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

const int MCAST_USLEEP=500;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_DATA_OFFSET=4;

int portMCast=1111;
int portServer=1112;
int portClient=1113;

char *sLocalAddr="0.0.0.0";
char *sPIInitAddr="255.255.255.255";

#define BUF_MAXSIZE 1600
int giDataSize=(1024+4);
char gcBuf[BUF_MAXSIZE];

int gbDoMCast = 1;

int sock_mcast_init_ex(int ifIndex, char *sMCastAddr)
{
	int iRet;
	int sockMCast = -1;
	struct ip_mreqn mreqn;

	sockMCast = socket(AF_INET, SOCK_DGRAM, 0);
	
	iRet = inet_aton(sMCastAddr, &mreqn.imr_multiaddr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to set MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
		exit(-1);
	}
	iRet = inet_aton(sLocalAddr, &mreqn.imr_address);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to set localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
		exit(-1);
	}
	mreqn.imr_ifindex = ifIndex;
	iRet = setsockopt(sockMCast, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: Failed joining group[%s] with localAddr[%s] & ifIndex[%d], ret=[%d]\n", __func__, sMCastAddr, sLocalAddr, ifIndex, iRet);
		perror("Failed joining group:");
		exit(-1);
	}
	return sockMCast;
}


int filedata_save(int fileData, char *buf, int iBlockOffset, int len) {
	if (len != giDataSize) {
		fprintf(stderr, "WARN:%s:giDataSize[%d] != pktDataLen[%d]\n", __func__, giDataSize, len);
	}
	lseek(fileData, iBlockOffset*giDataSize, SEEK_SET);
	write(fileData, buf, len);
}

int mcast_recv(int sockMCast, int fileData) {

	int iRet;
	uint32_t iPrevSeq = 0;
	uint32_t iSeq = 0;
	int iPktCnt = 0;
	int iOldSeqs = 0;
	int iDisjointSeqs = 0;
	int iDisjointPktCnt = 0;
	int iSeqDelta;

	while (gbDoMCast) {

		iRet = recvfrom(sockMCast, gcBuf, giDataSize, MSG_DONTWAIT, NULL, NULL);
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep(MCAST_USLEEP);
			} else {
				perror("WARN:mcast_recv:revfrom failed:");
			}
			continue;
		}
		iPktCnt += 1;
		iSeq = *((uint32_t*)&gcBuf[PKT_SEQNUM_OFFSET]);
		iSeqDelta = iSeq - iPrevSeq;
		if (iSeqDelta <= 0) {
			iOldSeqs += 1;
			continue;
		}
		if (iSeqDelta > 1) {
			iDisjointSeqs += 1;
			iDisjointPktCnt += (iSeqDelta-1);
		}
		filedata_save(fileData, &gcBuf[PKT_DATA_OFFSET], iSeq, iRet-PKT_DATA_OFFSET);

	}

}

int main(int argc, char **argv) {

	int sockMCast = -1;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <ifIndex4mcast> <mcast_addr>\n", argv[0]);
		exit(-1);
	}

	int ifIndex = strtol(argv[1], NULL, 0);
	char *sMCastAddr = argv[2];
	sockMCast = sock_mcast_init_ex(ifIndex, sMCastAddr);
	fprintf(stderr, "INFO:%s: sockMCast [%d] setup for [%s] on ifIndex [%d]\n", __func__, sockMCast, sMCastAddr, ifIndex);

}

