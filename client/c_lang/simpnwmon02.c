/*
    Simple Network Monitor 02 - C version
    v20190129IST1647
    HanishKVC, GPL, 19XY
 */

#define _LARGEFILE64_SOURCE 1
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
#include <signal.h>

#include <LinkedListRange.h>

const int STATS_TIMEDELTA=20;
const int MCASTREJOIN_TIMEDELTA=300;
const int MCASTSLOWEXIT_CNT=3;		// 20*3 = atleast 60secs of No MCast stop commands, after recieving atleast 1 stop command
const int MCAST_USLEEP=1000;
const int UCAST_PI_USLEEP=1000000;
const int PI_RETRYCNT = 600;		// 600*1e6uSecs = Client will try PI for atleast 600 seconds
const int UCAST_UR_USLEEP=1000;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_CTXTID_OFFSET=4;
const int PKT_TOTALBLOCKS_OFFSET=12;
const int PKT_DATA_OFFSET=16;
const int PKT_MCASTSTOP_TOTALBLOCKS_OFFSET=8;
const int PKT_URREQ_TOTALBLOCKS_OFFSET=8;
const int PKT_PIREQ_NAME_OFFSET=4;
const int PKT_PIREQ_LOSTPKTS_OFFSET=20;

int gNwGroupMul=5;
int gPortMCast=1111;
int gPortServer=1112;
int gPortClient=1113;

const int SeqNumCmdsCmn   = 0xffffff00;
const int PIReqSeqNum     = 0xffffff00;
const int PIAckSeqNum     = 0xffffff01;
const int URReqSeqNum     = 0xffffff02;
const int URAckSeqNum     = 0xffffff03;
const int MCASTSTOPSeqNum = 0xffffffff;

//char *sLocalAddr="0.0.0.0";

#define BUF_MAXSIZE 1600
int giDataSize=(1024+4);
char gcBuf[BUF_MAXSIZE];
#define MAIN_FPATH_LEN 256
char gsContextFileBase[MAIN_FPATH_LEN] = "/tmp/snm02";
#define CID_MAXLEN 16
char gsCID[CID_MAXLEN] = "v20190130whoAMi";

// Saved SNM Contexts
#define SC_MAXDATASEQGOT "MaxDataSeqNumGot"
#define SC_MAXDATASEQGOTEX "#MaxDataSeqNumGot:"
#define SC_MAXDATASEQGOTEX_LEN 18
#define SC_MAXDATASEQPROC "MaxDataSeqNumProc"
#define SC_MAXDATASEQPROCEX "#MaxDataSeqNumProc:"
#define SC_MAXDATASEQPROCEX_LEN 19
#define SC_DONEMODES "DoneModes"
#define SC_DONEMODESEX "#DoneModes:"
#define SC_DONEMODESEX_LEN 11


int gbDoMCast = 1;
int gbSNMRun = 1;

struct snm {
	int state;
	unsigned int uCtxtId;
	int sockMCast, sockUCast;
	int iLocalIFIndex;
	char *sMCastAddr;
	char *sLocalAddr;
	char *sBCastAddr;
	char *sDataFile;
	int iRunModes;
	char *sContextFile;
	char *sContextFileBase;
	struct LLR llLostPkts;
	int iNwGroup;
	int portMCast, portServer, portClient;
	int fileData;
	uint32_t theSrvrPeer;
	int iMaxDataSeqNumGot;
	int iDoneModes;
	int iMaxDataSeqNumProcessed;
	char *sCID;
};
struct snm snmCur;

#define RUNMODE_MCAST 0x01
#define RUNMODE_UCASTPI 0x02
#define RUNMODE_UCASTUR 0x04
#define RUNMODE_UCAST 0x06
#define RUNMODE_ALL 0x07
#define RUNMODE_AUTO 0x10000 // 65536

void _snm_ports_update(struct snm *me) {
	me->portMCast = gPortMCast + gNwGroupMul*me->iNwGroup;
	me->portServer = gPortServer + gNwGroupMul*me->iNwGroup;
	me->portClient = gPortClient + gNwGroupMul*me->iNwGroup;
}

void snm_init(struct snm *me) {
	me->state = 0;
	me->uCtxtId = -1;
	me->sockMCast = -1;
	me->sockUCast = -1;
	me->iLocalIFIndex = 0;
	me->sMCastAddr = NULL;
	me->sLocalAddr = NULL;
	me->sBCastAddr = NULL;
	me->sDataFile = NULL;
	me->iRunModes = RUNMODE_ALL;
	me->sContextFile = NULL;
	me->sContextFileBase = gsContextFileBase;
	me->iNwGroup = 0;
	_snm_ports_update(me);
	me->fileData = -1;
	me->theSrvrPeer = 0;
	ll_init(&me->llLostPkts);
	me->iMaxDataSeqNumGot = -1;
	me->iDoneModes = 0;
	me->iMaxDataSeqNumProcessed = -1;
	me->sCID = gsCID;
}

void _save_ll_context(struct LLR *meLLR, int iFile, char *sTag, char *sFName) {
	int iRet;
	if (meLLR == NULL) {
		fprintf(stderr, "WARN:%s:%s: Passed LostPacketRanges ll not yet setup\n", __func__, sTag);
	} else {
		iRet = _ll_save(meLLR, iFile);
		fprintf(stderr, "INFO:%s:%s: LostPacketRanges ll saved [%d of %d] to [%s]\n", __func__, sTag, iRet, meLLR->iNodeCnt, sFName);
	}
}

void snm_save_context(struct snm *me, char *sTag) {
	char sFName[MAIN_FPATH_LEN];
	int iFile;
	char sTmp[128];

	strncpy(sFName, me->sContextFileBase, MAIN_FPATH_LEN);
	strncat(sFName, ".context.", MAIN_FPATH_LEN);
	strncat(sFName, sTag, MAIN_FPATH_LEN);
	iFile = open(sFName, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	if (iFile == -1) {
		perror("ERRR:save_context: Open");
		return;
	}
	snprintf(sTmp, 128, "#%s:%d\n", SC_MAXDATASEQGOT, me->iMaxDataSeqNumGot);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%d\n", SC_MAXDATASEQPROC, me->iMaxDataSeqNumProcessed);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%d\n", SC_DONEMODES, me->iDoneModes);
	write(iFile, sTmp, strlen(sTmp));
	_save_ll_context(&me->llLostPkts, iFile, sTag, sFName);
	close(iFile);
}

//#define ENABLE_MULTICAST_ALL 1
#undef ENABLE_MULTICAST_ALL
#define MCAST_JOIN 0
#define MCAST_DROP 1
int sock_mcast_ctrl(int sockMCast, int ifIndex, char *sMCastAddr, char *sLocalAddr, int mode) {
	int iRet;
	struct ip_mreqn mreqn;
	char *sReason;

	if ((mode == MCAST_JOIN) || (mode == IP_ADD_MEMBERSHIP)) {
		sReason = "join";
		mode = IP_ADD_MEMBERSHIP;
	} else {
		sReason = "drop";
		mode = IP_DROP_MEMBERSHIP;
	}

	iRet = inet_aton(sMCastAddr, &mreqn.imr_multiaddr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to understand MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
		return -1;
	}
	//fprintf(stderr, "INFO:%s: for IP_ADD_MEMBERSHIP use MCastAddr[%s], ret=[%d]\n", __func__, sMCastAddr, iRet);
	iRet = inet_aton(sLocalAddr, &mreqn.imr_address);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to understand localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
		return -2;
	}
	//fprintf(stderr, "INFO:%s: for IP_ADD_MEMBERSHIP use localAddr[%s], ret=[%d]\n", __func__, sLocalAddr, iRet);
	mreqn.imr_ifindex = ifIndex;
	iRet = setsockopt(sockMCast, IPPROTO_IP, mode, &mreqn, sizeof(mreqn));
	if (iRet != 0) {
		fprintf(stderr, "ERROR:%s: sockMCast [%d] Failed to %s group[%s] with localAddr[%s] & ifIndex[%d], ret=[%d]\n", __func__, sockMCast, sReason, sMCastAddr, sLocalAddr, ifIndex, iRet);
		perror("ERRR:mcast_ctrl: Failed join/drop group");
		return -3;
	}
	fprintf(stderr, "INFO:%s: sockMCast [%d] %sed [%s] on (LocalAddr [%s] & ifIndex [%d]), ret=[%d]\n", __func__, sockMCast, sReason, sMCastAddr, sLocalAddr, ifIndex, iRet);

#ifdef ENABLE_MULTICAST_ALL
	if (mode == IP_ADD_MEMBERSHIP) {
		uint8_t iEnable = 1;
		iRet = setsockopt(sockMCast, IPPROTO_IP, IP_MULTICAST_ALL, &iEnable, sizeof(iEnable));
		if (iRet != 0) {
			fprintf(stderr, "ERROR:%s: Failed Enabling MulticastALL, ret=[%d]\n", __func__, iRet);
			perror("ERRR:mcast_ctrl: Failed MulticastALL");
			return -4;
		}
		fprintf(stderr, "INFO:%s: Enabled MulticastALL, ret=[%d]\n", __func__, iRet);
	}
#endif
	return 0;
}

int snm_sock_mcast_ctrl(struct snm *me, int mode) {
	int iRet;
	char *sReason;

	if ((mode == MCAST_JOIN) || (mode == IP_ADD_MEMBERSHIP)) {
		sReason = "join";
	} else {
		sReason = "drop";
	}

	iRet = sock_mcast_ctrl(me->sockMCast, me->iLocalIFIndex, me->sMCastAddr, me->sLocalAddr, mode);
	if (iRet < 0) {
		fprintf(stderr, "ERROR:%s: [%d] couldn't %s [%s] throu [%d] [%s]\n", __func__, me->sockMCast, sReason, me->sMCastAddr, me->iLocalIFIndex, me->sLocalAddr);
	}
	return iRet;
}

#define ENABLE_BROADCAST 1
int sock_ucast_init(char *sLocalAddr, int port, int bcastEnable) {
	int iRet;
	int sockUCast = -1;
	struct sockaddr_in myAddr;

	sockUCast = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockUCast == -1) {
		fprintf(stderr, "ERROR:%s: Failed to create socket, ret=[%d]\n", __func__, sockUCast);
		perror("ERRR:ucast_init: Failed socket");
		exit(-1);
	}

	if (bcastEnable != 0) {
		int iEnable = 1;
		iRet = setsockopt(sockUCast, SOL_SOCKET, SO_BROADCAST, &iEnable, sizeof(iEnable));
		if (iRet != 0) {
			fprintf(stderr, "ERROR:%s: Failed Enabling Broadcast, ret=[%d]\n", __func__, iRet);
			perror("ERRR:ucast_init: Failed enabling Broadcast");
			exit(-1);
		}
		fprintf(stderr, "INFO:%s: Enabled Broadcast, ret=[%d]\n", __func__, iRet);
	}

	myAddr.sin_family=AF_INET;
	myAddr.sin_port=htons(port);
	myAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	iRet = bind(sockUCast, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (iRet < 0) {
		fprintf(stderr, "ERROR:%s: Failed bind localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, INADDR_ANY, port, iRet);
		perror("ERRR:ucast_init: Failed bind");
		exit(-1);
	}
	fprintf(stderr, "INFO:%s: sockUCast [%d] bound to localAddr[0x%X] & port[%d], ret=[%d]\n", __func__, sockUCast, INADDR_ANY, port, iRet);
	return sockUCast;
}

int snm_sock_ucast_init(struct snm *me) {
	me->sockUCast = sock_ucast_init(me->sLocalAddr, me->portClient, ENABLE_BROADCAST);
	return me->sockUCast;
}

int sock_mcast_init_ex(int ifIndex, char *sMCastAddr, int port, char *sLocalAddr)
{
	int iRet;
	int sockMCast = -1;

	sockMCast = sock_ucast_init(sLocalAddr, port, ENABLE_BROADCAST);
	iRet = sock_mcast_ctrl(sockMCast, ifIndex, sMCastAddr, sLocalAddr, IP_ADD_MEMBERSHIP);
	if (iRet < 0) {
		exit(-1);
	}
	return sockMCast;
}

int snm_sock_mcast_init(struct snm *me) {
	me->sockMCast = sock_mcast_init_ex(me->iLocalIFIndex, me->sMCastAddr, me->portMCast, me->sLocalAddr);
	return me->sockMCast;
}

int filedata_save(int fileData, char *buf, int iBlockOffset, int len) {
	int iRet;
	off64_t iRet64;
	int iExpectedDataLen = giDataSize-PKT_DATA_OFFSET;

	if (len != iExpectedDataLen) {
		fprintf(stderr, "WARN:%s:ExpectedDataSize[%d] != pktDataLen[%d]\n", __func__, iExpectedDataLen, len);
	}
	iRet64 = lseek64(fileData, (off64_t)iBlockOffset*iExpectedDataLen, SEEK_SET);
	if (iRet64 == -1) {
		perror("WARN:filedata_save: lseek failed");
		return -1;
	}
	iRet = write(fileData, buf, len);
	if (iRet == -1) {
		perror("WARN:filedata_save: write failed");
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

void _account_lostpackets(struct LLR *llrLostPkts, int start, int end, int *piDisjointSeqs, int *piDisjointPktCnt) {
	ll_add_sorted_startfrom_lastadded(llrLostPkts, start, end);
	*piDisjointSeqs += 1;
	*piDisjointPktCnt += (end-start+1);
}

int mcast_recv(int sockMCast, int fileData, struct LLR *llLostPkts, int *piMaxDataSeqNumGot, int *piMaxDataSeqNumProcessed, struct snm *me) {

	int iRet;
	int iPrevSeq = *piMaxDataSeqNumGot - 1;
	int iSeq = *piMaxDataSeqNumGot;
	int iPktCnt = 0;
	int iOldSeqs = 0;
	int iDisjointSeqs = 0;
	int iDisjointPktCnt = 0;
	int iSeqDelta;
	time_t prevSTime, curSTime;
	time_t prevDTime, curDTime;
	time_t deltaSTime;
	time_t prevMTime, deltaMTime;
	int iPrevPktCnt = 0;
	int iRecvdStop = 0;
	int iPrevRecvdStop = -1;
	int iMCastSlowExit = 0;
	int iActualLastSeqNum = -1;

	if (iPrevSeq == -2) {
		iPrevSeq = -1;
		iSeq = -1;
	}

	prevSTime = time(NULL);
	prevDTime = prevSTime;
	curDTime = prevDTime;
	prevMTime = prevSTime;
	gbDoMCast = 1;
	deltaMTime = -1;
	while (gbDoMCast) {

		iRet = recvfrom(sockMCast, gcBuf, giDataSize, MSG_DONTWAIT, NULL, NULL);
		curSTime = time(NULL);
		deltaSTime = curSTime - prevSTime;
		if (deltaSTime > STATS_TIMEDELTA) {
			deltaMTime = curSTime - prevMTime;
			print_stat(__func__, iPktCnt, iPrevPktCnt, iSeq, curSTime, curDTime, prevDTime, iDisjointSeqs, iDisjointPktCnt, iOldSeqs);
			prevSTime = curSTime;
			iPrevPktCnt = iPktCnt;
			prevDTime = curDTime;
			if (iRecvdStop > 0) {
				fprintf(stderr, "INFO:%s: In MCastStop phase... (LastPkt Got[%d] Actual[%d]) ", __func__, *piMaxDataSeqNumGot, iActualLastSeqNum);
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
				if (deltaMTime > MCASTREJOIN_TIMEDELTA) {
					deltaMTime = -1;
					prevMTime = curSTime;
					if (me != NULL) {
						fprintf(stderr, "INFO:%s: silent mcast channel, rejoining again\n", __func__);
						snm_sock_mcast_ctrl(me, IP_DROP_MEMBERSHIP);
						snm_sock_mcast_ctrl(me, IP_ADD_MEMBERSHIP);
					} else {
						fprintf(stderr, "INFO:%s: silent mcast channel, simply continuing to wait\n", __func__);
					}
				}
			} else {
				perror("WARN:mcast_recv: revfrom failed");
			}
			continue;
		}
		curDTime = time(NULL);
#ifndef MCASTREJOIN_ALWAYS
		prevMTime = curDTime;
#endif
		iPktCnt += 1;
		iSeq = *((uint32_t*)&gcBuf[PKT_SEQNUM_OFFSET]);
		if (iSeq == MCASTSTOPSeqNum) {
			iActualLastSeqNum = *((uint32_t*)&gcBuf[PKT_MCASTSTOP_TOTALBLOCKS_OFFSET]) - 1;
			if (iRecvdStop == 0) {
				if (*piMaxDataSeqNumGot != iActualLastSeqNum) {
					_account_lostpackets(llLostPkts, *piMaxDataSeqNumGot+1, iActualLastSeqNum, &iDisjointSeqs, &iDisjointPktCnt);
					if (*piMaxDataSeqNumProcessed < iActualLastSeqNum) {
						fprintf(stderr, "WARN:%s: ProcessedMaxDataSeq [%d] < ActualLastSeq[%d], updated\n", __func__, *piMaxDataSeqNumProcessed, iActualLastSeqNum);
						*piMaxDataSeqNumProcessed = iActualLastSeqNum;
					} else {
						fprintf(stderr, "DBUG:%s: ProcessedMaxDataSeq [%d] >= ActualLastSeq[%d]\n", __func__, *piMaxDataSeqNumProcessed, iActualLastSeqNum);
					}
				}
			}
			iRecvdStop += 1;
			continue;
		}
		iSeqDelta = iSeq - iPrevSeq;
		if (iSeqDelta <= 0) {
			iOldSeqs += 1;
			continue;
		}
		*piMaxDataSeqNumGot = iSeq;
		*piMaxDataSeqNumProcessed = iSeq;
		if (iSeqDelta > 1) {
			//fprintf(stderr, "DEBUG:%s: iSeq[%d] iPrevSeq[%d] iSeqDelta[%d] iDisjointSeqs[%d] iDisjointPktCnt[%d]\n", __func__, iSeq, iPrevSeq, iSeqDelta, iDisjointSeqs, iDisjointPktCnt);
			_account_lostpackets(llLostPkts, iPrevSeq+1, iSeq-1, &iDisjointSeqs, &iDisjointPktCnt);
		}
		if (fileData != -1) {
			filedata_save(fileData, &gcBuf[PKT_DATA_OFFSET], iSeq, iRet-PKT_DATA_OFFSET);
		}
		iPrevSeq = iSeq;

	}

	return 0;
}

int snm_mcast_recv(struct snm *me) {
	int iRet = -1;

	if ((me->iRunModes & RUNMODE_MCAST) == RUNMODE_MCAST) {
		iRet = mcast_recv(me->sockMCast, me->fileData, &me->llLostPkts, &me->iMaxDataSeqNumGot, &me->iMaxDataSeqNumProcessed, me);
		me->iDoneModes |= RUNMODE_MCAST;
		snm_save_context(me, "mcast");
	} else {
		fprintf(stderr, "INFO:%s: Skipping mcast:recv...\n", __func__);
	}
	me->iDoneModes |= RUNMODE_MCAST;
	return iRet;
}

int ucast_pi(int sockUCast, char *sPINwBCast, int portServer, uint32_t *theSrvrPeer, int iTotalLostPkts, char *sCID) {
	int iRet;
	char bufS[32], bufR[32];
	struct sockaddr_in addrS, addrR;

	addrS.sin_family=AF_INET;
	addrS.sin_port=htons(portServer);
	iRet = inet_aton(sPINwBCast, &addrS.sin_addr);
	if (iRet == 0) {
		fprintf(stderr, "ERROR:%s: Failed to convert [%s] to nw addr, ret=[%d]\n", __func__, sPINwBCast, iRet);
		perror("WARN:ucast_pi: sPINwBCast aton failed, PIPhase will be ignored");
#ifdef EXITON_NWERROR
		exit(-1);
#else
		return -1;
#endif
	}
	*(uint32_t *)bufS = PIReqSeqNum;
	strncpy(&bufS[PKT_PIREQ_NAME_OFFSET], sCID, CID_MAXLEN);
	*(uint32_t *)&bufS[PKT_PIREQ_LOSTPKTS_OFFSET] = iTotalLostPkts;

	for(int i = 0; i < PI_RETRYCNT; i++) {
		fprintf(stderr, "INFO:%s: PI[%s:%d] :find peer srvr: try %d\n", __func__, &bufS[PKT_PIREQ_NAME_OFFSET], iTotalLostPkts, i);
		iRet = sendto(sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
		if (iRet == -1) {
			perror("WARN:ucast_pi: Failed sending PIReq, may try again");
#ifdef EXITON_NWERROR
			exit(-1);
#else
			usleep(UCAST_PI_USLEEP);
			continue;
#endif
		}
		unsigned int iAddrLen = sizeof(addrR);
		iRet = recvfrom(sockUCast, bufR, sizeof(bufR), MSG_DONTWAIT, (struct sockaddr *)&addrR, &iAddrLen);
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep(UCAST_PI_USLEEP);
			} else {
				perror("WARN:ucast_pi: revfrom failed");
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
			usleep(UCAST_PI_USLEEP);
		}
	}
	return -1;
}

int snm_ucast_pi(struct snm *me) {
	int iRet = -1;

	if ((me->iRunModes & RUNMODE_UCASTPI) == RUNMODE_UCASTPI) {
		iRet = ucast_pi(me->sockUCast, me->sBCastAddr, me->portServer, &(me->theSrvrPeer), me->llLostPkts.iTotalFromRanges, me->sCID);
		if (iRet < 0) {
			fprintf(stderr, "WARN:%s: No Server found during PI Phase, however giving ucast_recover a chance...\n", __func__);
		}
	} else {
		fprintf(stderr, "INFO:%s: Skipping ucast:pi...\n", __func__);
	}
	me->iDoneModes |= RUNMODE_UCASTPI;
	return iRet;
}

#define UR_BUFS_LEN 800
#define UR_BUFR_LEN 1500
int ucast_recover(int sockUCast, int fileData, uint32_t theSrvrPeer, int portServer, struct LLR *llLostPkts, int *piMaxDataSeqNumProcessed) {
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
				perror("WARN:ucast_recover: revfrom failed");
			}
			continue;
		}
		iPktCnt += 1;
		int iSeq = *((uint32_t*)&bufR[PKT_SEQNUM_OFFSET]);
		if (iSeq == URReqSeqNum) {
			if (theSrvrPeer != addrR.sin_addr.s_addr) {
				fprintf(stderr, "WARN:%s: prev peer srvr[0x%X] cmdGot from[0x%X], adjusting...\n", __func__, theSrvrPeer, addrR.sin_addr.s_addr);
				theSrvrPeer = addrR.sin_addr.s_addr;
				addrS.sin_addr.s_addr = theSrvrPeer;
			}
			int iIgnore;
			int iActualLastSeqNum = *((uint32_t*)&bufR[PKT_URREQ_TOTALBLOCKS_OFFSET]) - 1;
			if (*piMaxDataSeqNumProcessed < iActualLastSeqNum) {
				_account_lostpackets(llLostPkts, *piMaxDataSeqNumProcessed+1, iActualLastSeqNum, &iIgnore, &iIgnore);
				fprintf(stderr, "WARN:%s: ProcessedMaxDataSeq [%d] < ActualLastSeq[%d], updated\n", __func__, *piMaxDataSeqNumProcessed, iActualLastSeqNum);
				*piMaxDataSeqNumProcessed = iActualLastSeqNum;
			} else if (*piMaxDataSeqNumProcessed > iActualLastSeqNum) {
				fprintf(stderr, "DBUG:%s: ProcessedMaxDataSeq [%d] > ActualLastSeq[%d]\n", __func__, *piMaxDataSeqNumProcessed, iActualLastSeqNum);
			}

			int iRecords = ll_getdata(llLostPkts, &bufS[4], UR_BUFS_LEN-4, 20);
#ifdef PRG_UR_VERBOSE
			ll_print_summary(llLostPkts, "LostPackets");
#endif
			iRet = sendto(sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
			if (iRet == -1) {
				perror("WARN:ucast_recover: Failed sending URAck currently");
#ifdef EXITON_NWERROR
				exit(-1);
#endif
			} else {
				fprintf(stderr, "INFO:%s: URAck [%d]records", __func__, iRecords);
			}
			fprintf(stderr, ": Lost Ranges[%d] Pkts[%d]\n", llLostPkts->iNodeCnt, llLostPkts->iTotalFromRanges);
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

int snm_ucast_recover(struct snm *me) {
	int iRet = -1;
	if ((me->iRunModes & RUNMODE_UCASTUR) == RUNMODE_UCASTUR) {
		iRet = ucast_recover(me->sockUCast, me->fileData, me->theSrvrPeer, me->portServer, &me->llLostPkts, &me->iMaxDataSeqNumProcessed);
	} else {
		fprintf(stderr, "INFO:%s: Skipping ucast:ur...\n", __func__);
	}
	me->iDoneModes |= RUNMODE_UCASTUR;
	return iRet;
}

#define STATE_DO 0
#define STATE_DONE 1
int snm_run(struct snm *me) {
	int iRet;
	fd_set socks;
	struct timeval tv;
	int sock;
	char bufR[UR_BUFR_LEN];
	struct sockaddr_in addrR;
	int iDataCnt, iPktCnt;

	iPktCnt = 0;
	iDataCnt = 0;
	gbSNMRun = 1;
	while(gbSNMRun) {
		FD_ZERO(&socks);
		FD_SET(me->sockMCast, &socks);
		FD_SET(me->sockUCast, &socks);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		iRet = select(me->sockUCast+1, &socks, NULL, NULL, &tv);
		if (iRet == -1) {
			perror("ERRR:snm_run: select failed");
			continue;
		}
		if (iRet == 0) {
			// Handle MCast ReJoin, when required
			continue;
		}
		// UCast commands / data get priority
		if (FD_ISSET(me->sockUCast, &socks)) {
			sock = me->sockUCast;
		} else {
			sock = me->sockMCast;
		}
		unsigned int iAddrLen = sizeof(addrR);
		iRet = recvfrom(sock, bufR, sizeof(bufR), MSG_DONTWAIT, (struct sockaddr *)&addrR, &iAddrLen);
		if (iRet == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				//usleep(SNM_RUN_USLEEP);
				fprintf(stderr, "WARN:%s: recvfrom EAGAIN/EWOULDBLOCK[%d]\n", __func__, errno);
			} else {
				perror("WARN:snm_run: revfrom failed");
			}
			continue;
		}
		iPktCnt += 1;
		// Handle recieved packet
		int iSeq = *((uint32_t*)&bufR[PKT_SEQNUM_OFFSET]);
		unsigned int uCTXTId = *((uint32_t*)&bufR[PKT_CTXTID_OFFSET]);
		unsigned int uTotalBlocks = *((uint32_t*)&bufR[PKT_TOTALBLOCKS_OFFSET]);
		if ((me->state == STATE_DO) && (me->uCtxtId == -1)) {
			me->uCtxtId = uCTXTId;
			ll_add_sorted_startfrom_lastadded(&me->llLostPkts, 0, uTotalBlocks-1);
		}
		if (iSeq == URReqSeqNum) {
		} else if (iSeq == PIReqSeqNum) {
		} else {
			iDataCnt += 1;
		}
		if ((me->state == STATE_DO) && (me->llLostPkts.iNodeCnt == 0)) {
			me->state = STATE_DONE;
		}
	}
	return 0;
}

int snm_datafile_open(struct snm *me) {
	me->fileData = open(me->sDataFile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); // Do I need truncate to think later. Also if writing to device files, then have to re-evaluate the flags
	if (me->fileData == -1) {
		fprintf(stderr, "WARN:%s: Failed to open data file [%s], saving data will be skipped\n", __func__, me->sDataFile);
		perror("ERRR:datafile_open: Failed datafile open");
	} else {
		fprintf(stderr, "INFO:%s: opened data file [%s]\n", __func__, me->sDataFile);
	}
	return me->fileData;
}

void _snm_context_load(char *sLine, int iLineLen, void *meMaya) {
	struct snm *me = (struct snm*)meMaya;

	if (strncmp(sLine, SC_MAXDATASEQGOTEX, SC_MAXDATASEQGOTEX_LEN) == 0) {
		me->iMaxDataSeqNumGot = strtol(&sLine[SC_MAXDATASEQGOTEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded iMaxDataSeqNumGot [%d]\n", __func__, me->iMaxDataSeqNumGot);
	}
	if (strncmp(sLine, SC_MAXDATASEQPROCEX, SC_MAXDATASEQPROCEX_LEN) == 0) {
		me->iMaxDataSeqNumProcessed = strtol(&sLine[SC_MAXDATASEQPROCEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded iMaxDataSeqNumProcessed [%d]\n", __func__, me->iMaxDataSeqNumProcessed);
	}
	if (strncmp(sLine, SC_DONEMODESEX, SC_DONEMODESEX_LEN) == 0) {
		me->iDoneModes = strtol(&sLine[SC_DONEMODESEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded iDoneModes [%d]\n", __func__, me->iDoneModes);
	}
}

#define AUTOADJUST_RUNMODESFROMDONEMODES
int snm_context_load(struct snm *me) {
	int iRet = 0;

	if (me->sContextFile != NULL) {
		fprintf(stderr, "INFO:%s: About to load context including lostpacketRanges from [%s]...\n", __func__, me->sContextFile);
		iRet = ll_load_append_ex(&me->llLostPkts, me->sContextFile, '#', _snm_context_load, me);
#ifdef AUTOADJUST_RUNMODESFROMDONEMODES
		if (me->iRunModes == RUNMODE_AUTO) {
			me->iRunModes = RUNMODE_ALL & ~me->iDoneModes;
			fprintf(stderr, "INFO:%s: AutoAdjusted RunModes [%d]\n", __func__, me->iRunModes);
		} else {
			fprintf(stderr, "INFO:%s: Ignoring DoneModes [%d], RunModes [%d]\n", __func__, me->iDoneModes, me->iRunModes);
		}
#endif
	}
	if (me->iRunModes == RUNMODE_AUTO) {
		me->iRunModes = RUNMODE_ALL;
	}
	return iRet;
}

void snm_args_process_p1(struct snm *me) {
	if (snm_datafile_open(me) < 0) {
		exit(1);
	}
	_snm_ports_update(me);
	if (snm_context_load(me) < 0) {
		exit(2);
	}
}

void signal_handler(int arg) {
	snm_save_context(&snmCur, "quit");
	exit(2);
}

#define ARG_MADDR "--maddr"
#define ARG_FILE "--file"
#define ARG_LOCAL "--local"
#define ARG_BCAST "--bcast"
#define ARG_CONTEXT "--context"
#define ARG_CONTEXTBASE "--contextbase"
#define ARG_RUNMODES "--runmodes"
#define ARG_NWGROUP "--nwgroup"
#define ARG_CID "--cid"

void print_usage(void) {
	fprintf(stderr, "usage: simpnwmon02 <--maddr mcast_addr> <--local ifIndex4mcast local_addr> <--file datafile> <--bcast pi_nw_bcast_addr>\n");
	fprintf(stderr, "\t Optional args\n");
	fprintf(stderr, "\t --context contextfile_to_load # If specified, the list of lost pkt ranges is initialised with its contents\n");
	fprintf(stderr, "\t --contextbase contextfile_basename_forsaving\n");
	fprintf(stderr, "\t --runmodes 1|2|4|6|7|65536\n");
	fprintf(stderr, "\t --nwgroup id\n");
	fprintf(stderr, "\t --cid theClientID_limitTo15Chars\n");
}

int snm_parse_args(struct snm *me, int argc, char **argv) {

	int iArg = 1;
	while(iArg < argc) {
		if (strcmp(argv[iArg], ARG_MADDR) == 0) {
			iArg += 1;
			me->sMCastAddr = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_LOCAL) == 0) {
			iArg += 1;
			me->iLocalIFIndex = strtol(argv[iArg], NULL, 0);
			iArg += 1;
			me->sLocalAddr = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_FILE) == 0) {
			iArg += 1;
			me->sDataFile = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_BCAST) == 0) {
			iArg += 1;
			me->sBCastAddr = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_CONTEXT) == 0) {
			iArg += 1;
			me->sContextFile = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_CONTEXTBASE) == 0) {
			iArg += 1;
			me->sContextFileBase = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_RUNMODES) == 0) {
			iArg += 1;
			me->iRunModes = strtol(argv[iArg], NULL, 0);
		}
		if (strcmp(argv[iArg], ARG_NWGROUP) == 0) {
			iArg += 1;
			me->iNwGroup = strtol(argv[iArg], NULL, 0);
		}
		if (strcmp(argv[iArg], ARG_CID) == 0) {
			iArg += 1;
			me->sCID = argv[iArg];
		}
		if (iArg >= argc) {
			print_usage();
			exit(-1);
		}
		iArg += 1;
	}
	if ((me->sMCastAddr == NULL) || (me->sLocalAddr == NULL) || (me->sDataFile == NULL) || (me->sBCastAddr == NULL)) {
		print_usage();
		exit(-1);
	}
	fprintf(stderr, "INFO:CID:%s\n", me->sCID);
	return 0;
}

int main(int argc, char **argv) {

	snm_init(&snmCur);
	snm_parse_args(&snmCur, argc, argv);

	if (signal(SIGINT, &signal_handler) == SIG_ERR) {
		perror("WARN:main: Failed setting SIGINT handler");
	}

	snm_args_process_p1(&snmCur);

	snm_sock_mcast_init(&snmCur);
	snm_sock_ucast_init(&snmCur);

	snm_run(&snmCur);

	ll_print(&snmCur.llLostPkts, "LostPackets before exit");
	ll_free(&snmCur.llLostPkts);
	return 0;
}

