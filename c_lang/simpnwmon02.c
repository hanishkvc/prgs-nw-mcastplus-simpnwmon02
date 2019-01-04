/*
    Simple Network Monitor 02 - C version
    v20190104IST0037
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
#include <string.h>

#include <LinkedListRange.h>

const int STATS_TIMEDELTA=20;
const int MCASTSLOWEXIT_CNT=3;		// 20*3 = atleast 60secs of No MCast stop commands, after recieving atleast 1 stop command
const int MCAST_USLEEP=1000;
const int UCAST_PI_USLEEP=1000000;
const int PI_RETRYCNT = 300;		// 300*1e6uSecs = Client will try PI for atleast 300 seconds
const int UCAST_UR_USLEEP=1000;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_DATA_OFFSET=4;

int portMCast=1111;
int portServer=1112;
int portClient=1113;

const int SeqNumCmdsCmn   = 0xffffff00;
const int PIReqSeqNum     = 0xffffff00;
const int PIAckSeqNum     = 0xffffff01;
const int URReqSeqNum     = 0xffffff02;
const int URAckSeqNum     = 0xffffff03;
const int MCASTSTOPSeqNum = 0xffffffff;

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
	if (sockMCast == -1) {
		fprintf(stderr, "ERROR:%s: Failed to create socket, ret=[%d]\n", __func__, sockMCast);
		perror("Failed socket");
		exit(-1);
	}

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

#define ENABLE_BROADCAST 1
int sock_ucast_init(char *sLocalAddr, int port) {
	int iRet;
	int sockUCast = -1;
	struct sockaddr_in myAddr;

	sockUCast = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockUCast == -1) {
		fprintf(stderr, "ERROR:%s: Failed to create socket, ret=[%d]\n", __func__, sockUCast);
		perror("Failed socket");
		exit(-1);
	}

#ifdef ENABLE_BROADCAST
	int iEnable = 1;
	iRet = setsockopt(sockUCast, SOL_SOCKET, SO_BROADCAST, &iEnable, sizeof(iEnable));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: Failed Enabling Broadcast, ret=[%d]\n", __func__, iRet);
		perror("Failed enabling Broadcast:");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: Enabled Broadcast, ret=[%d]\n", __func__, iRet);
#endif

	myAddr.sin_family=AF_INET;
	myAddr.sin_port=htons(port);
	myAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	iRet = bind(sockUCast, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (iRet < 0) {
		fprintf(stderr, "ERROR:%s: Failed bind localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, INADDR_ANY, port, iRet);
		perror("Failed bind:");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: sockUCast [%d] bound to localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, sockUCast, INADDR_ANY, port, iRet);
	return sockUCast;
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

void print_stat(const char *sTag, int iPktCnt, int iPrevPktCnt, int iSeq, time_t curSTime, time_t curDTime, time_t prevDTime, int iDisjointSeqs, int iDisjointPktCnt, int iOldSeqs) {
	int iDeltaPkts = iPktCnt - iPrevPktCnt;
	int iDeltaTimeSecs = curDTime - prevDTime;
	if (iDeltaTimeSecs == 0) iDeltaTimeSecs = 1;
	int iDataBytesPerSec = (iDeltaPkts*giDataSize)/iDeltaTimeSecs;
	fprintf(stderr, "INFO:%s: iPktCnt[%d] iSeqNum[%d] DataDelay[%ld] iDisjointSeqs[%d] iDisjointPktCnt[%d] iOldSeqs[%d] DataBPS[%d]\n",
			sTag, iPktCnt, iSeq, (curSTime-curDTime), iDisjointSeqs, iDisjointPktCnt, iOldSeqs, iDataBytesPerSec);
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
	int iPrevPktCnt = 0;
	int iRecvdStop = 0;
	int iPrevRecvdStop = -1;
	int iMCastSlowExit = 0;

	prevSTime = time(NULL);
	prevDTime = prevSTime;
	curDTime = prevDTime;
	gbDoMCast = 1;
	while (gbDoMCast) {

		iRet = recvfrom(sockMCast, gcBuf, giDataSize, MSG_DONTWAIT, NULL, NULL);
		curSTime = time(NULL);
		if ((curSTime-prevSTime) > STATS_TIMEDELTA) {
			print_stat(__func__, iPktCnt, iPrevPktCnt, iSeq, curSTime, curDTime, prevDTime, iDisjointSeqs, iDisjointPktCnt, iOldSeqs);
			prevSTime = curSTime;
			iPrevPktCnt = iPktCnt;
			prevDTime = curDTime;
			if (iRecvdStop > 0) {
				fprintf(stderr, "INFO:%s: In MCastStop phase...", __func__);
				if (iRecvdStop == iPrevRecvdStop) {
					fprintf(stderr, ": No New MCastStop Packets\n");
					iMCastSlowExit += 1;
					if (iMCastSlowExit > MCASTSLOWEXIT_CNT) {
						gbDoMCast = 0;
					}
				} else {
					fprintf(stderr, ": Still MCastStop Packets coming\n");
				}
				iPrevRecvdStop = iRecvdStop;
			}
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
		if (iSeq == MCASTSTOPSeqNum) {
			iRecvdStop += 1;
			continue;
		}
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

int ucast_pi(int sockUCast, char *sPINwBCast, int portServer, uint32_t *theSrvrPeer) {
	int iRet;
	char bufS[32], bufR[32];
	struct sockaddr_in addrS, addrR;

	addrS.sin_family=AF_INET;
	addrS.sin_port=htons(portServer);
	iRet = inet_aton(sPINwBCast, &addrS.sin_addr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to convert [%s] to nw addr, ret=[%d]\n", __func__, sPINwBCast, iRet);
		perror("sPINwBCast aton failed");
		exit(-1);
	}
	*(uint32_t *)bufS = PIReqSeqNum;
	strncpy(&bufS[4], "c_lang seat", 16);

	for(int i = 0; i < PI_RETRYCNT; i++) {
		fprintf(stderr, "INFO:%s: PI find peer srvr: try %d\n", __func__, i);
		iRet = sendto(sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
		if (iRet == -1) {
			perror("Failed sending PIReq");
			exit(-1);
		}
		unsigned int iAddrLen = sizeof(addrR);
		iRet = recvfrom(sockUCast, bufR, sizeof(bufR), MSG_DONTWAIT, (struct sockaddr *)&addrR, &iAddrLen);
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep(UCAST_PI_USLEEP);
			} else {
				perror("WARN:ucast_pi:revfrom failed:");
			}
			continue;
		}
		int iSeq = *((uint32_t*)&bufR[PKT_SEQNUM_OFFSET]);
		if (iSeq == PIAckSeqNum) {
			*theSrvrPeer = addrR.sin_addr.s_addr;
			fprintf(stderr, "INFO:%s: Found peer srver: %s\n", __func__, inet_ntoa(addrR.sin_addr));
			return 0;
		} else {
			fprintf(stderr, "DEBG:%s: Unexpected command [0x%X], check why\n", __func__, iSeq);
		}
	}
	return -1;
}

#define UR_BUFS_LEN 512
#define UR_BUFR_LEN 1500
int ucast_recover(int sockUCast, int fileData, uint32_t theSrvrPeer, int portServer, struct LLR *llLostPkts) {
	int iRet;
	char bufS[UR_BUFS_LEN], bufR[UR_BUFR_LEN];
	struct sockaddr_in addrS, addrR;
	time_t prevSTime, curSTime;
	int iPktCnt, iPrevPktCnt;

	addrS.sin_family=AF_INET;
	addrS.sin_port=htons(portServer);
	addrS.sin_addr.s_addr = theSrvrPeer;
	*(uint32_t *)bufS = URAckSeqNum;

	iPrevPktCnt = 0;
	iPktCnt = 0;
	prevSTime = time(NULL);
	int bUCastRecover = 1;
	while(bUCastRecover) {
		curSTime = time(NULL);
		int iDeltaTimeSecs = curSTime - prevSTime;
		if (iDeltaTimeSecs > STATS_TIMEDELTA) {
			int iDeltaPkts = iPktCnt - iPrevPktCnt;
			int iDataBytesPerSec = (iDeltaPkts*giDataSize)/iDeltaTimeSecs;
			fprintf(stderr, "INFO:%s: iPktCnt[%d] DataBPS[%d]\n", __func__, iPktCnt, iDataBytesPerSec);
			prevSTime = curSTime;
			iPrevPktCnt = iPktCnt;
		}
		unsigned int iAddrLen = sizeof(addrR);
		iRet = recvfrom(sockUCast, bufR, sizeof(bufR), MSG_DONTWAIT, (struct sockaddr *)&addrR, &iAddrLen);
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep(UCAST_UR_USLEEP);
			} else {
				perror("WARN:ucast_recover:revfrom failed:");
			}
			continue;
		}
		iPktCnt += 1;
		int iSeq = *((uint32_t*)&bufR[PKT_SEQNUM_OFFSET]);
		if (iSeq == URReqSeqNum) {
			if (theSrvrPeer != addrR.sin_addr.s_addr) {
				fprintf(stderr, "WARN:%s: Found peer srvr[0x%X] cmdGot from[0x%X], Ignoring\n", __func__, theSrvrPeer, addrR.sin_addr.s_addr);
				continue;
			}
			int iRecords = ll_getdata(llLostPkts, &bufS[4], UR_BUFS_LEN-4, 10);
			iRet = sendto(sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
			if (iRet == -1) {
				perror("Failed sending URAck");
				exit(-1);
			} else {
				fprintf(stderr, "INFO:%s: Sent URAck with [%d] records\n", __func__, iRecords);
			}
		} else {
			if ((iSeq & SeqNumCmdsCmn) == SeqNumCmdsCmn) {
				fprintf(stderr, "DEBG:%s: Unexpected command [0x%X], skipping, check why\n", __func__, iSeq);
				continue;
			}
			ll_delete(llLostPkts, iSeq);
			if (fileData != -1) {
				filedata_save(fileData, &bufR[PKT_DATA_OFFSET], iSeq, iRet-PKT_DATA_OFFSET);
			}
		}
	}
	return -1;
}

#define ARG_IFINDEX 1
#define ARG_MCASTADDR 2
#define ARG_LOCALADDR 3
#define ARG_DATAFILE 4
#define ARG_PINWBCAST 5
#define ARG_COUNT (5+1)

int main(int argc, char **argv) {

	int sockMCast = -1;
	int sockUCast = -1;
	struct LLR llLostPkts;
	uint32_t theSrvrPeer = 0;

	if (argc < ARG_COUNT) {
		fprintf(stderr, "usage: %s <ifIndex4mcast> <mcast_addr> <local_addr> <datafile> <pi_nw_bcast_addr>\n", argv[0]);
		exit(-1);
	}

	int ifIndex = strtol(argv[ARG_IFINDEX], NULL, 0);
	char *sMCastAddr = argv[ARG_MCASTADDR];
	char *sLocalAddr = argv[ARG_LOCALADDR];
	char *sPINwBCast = argv[ARG_PINWBCAST];
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

	sockUCast = sock_ucast_init(sLocalAddr, portClient);
	if (ucast_pi(sockUCast, sPINwBCast, portServer, &theSrvrPeer) < 0) {
		fprintf(stderr, "WARN:%s: No Server found during PI Phase, quiting...\n", __func__);
	} else {
		ucast_recover(sockUCast, fileData, theSrvrPeer, portServer, &llLostPkts);
	}

	ll_free(&llLostPkts);
	return 0;
}

