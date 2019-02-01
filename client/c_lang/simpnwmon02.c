/*
    Simple Network Monitor 02 - C version
    v20190130IST2335
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
const int RUN_USLEEP=1000;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_CTXTID_OFFSET=4;
const int PKT_TOTALBLOCKS_OFFSET=12;
const int PKT_DATA_OFFSET=16;
const int PKT_URACK_DATA_OFFSET=4;
const int PKT_PIACK_NAME_OFFSET=4;
const int PKT_PIACK_LOSTPKTS_OFFSET=20;

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
int giDataSize=(1024+4*4);
char gcBuf[BUF_MAXSIZE];
#define MAIN_FPATH_LEN 256
char gsContextFileBase[MAIN_FPATH_LEN] = "/tmp/snm02";
#define CID_MAXLEN 16
char gsCID[CID_MAXLEN] = "v20190130iAMwho";

// Saved SNM Contexts
#define SC_CTXTID "CtxtId"
#define SC_CTXTIDEX "#CtxtId:"
#define SC_CTXTIDEX_LEN 8
#define SC_CTXTFILEBASE "CtxtFileBase"
#define SC_CTXTFILEBASEEX "#CtxtFileBase:"
#define SC_CTXTFILEBASEEX_LEN 14
#define SC_DATAFILE "DataFile"
#define SC_DATAFILEEX "#DataFile:"
#define SC_DATAFILEEX_LEN 10
#define SC_MAXDATASEQGOT "MaxDataSeqNumGot"
#define SC_MAXDATASEQGOTEX "#MaxDataSeqNumGot:"
#define SC_MAXDATASEQGOTEX_LEN 18
#define SC_THESRVRPEER "TheSrvrPeer"
#define SC_THESRVRPEEREX "#TheSrvrPeer:"
#define SC_THESRVRPEEREX_LEN 13
#define SC_STATE "State"
#define SC_STATEEX "#State:"
#define SC_STATEEX_LEN 7

#define CTXTLOAD_INIT 0
#define CTXTLOAD_AUTO 1

int gbSNMRun = 1;
char gstDataFile[MAIN_FPATH_LEN];
char gstContextFile[MAIN_FPATH_LEN];

#define CTXTAUTOLOAD  1

struct snm {
	int state;
	unsigned int uCtxtId;
	int sockMCast, sockUCast;
	int iLocalIFIndex;
	char *sMCastAddr;
	char *sLocalAddr;
	char *sDataFile;
	char *sContextFile;
	char *sContextFileBase;
	struct LLR llLostPkts;
	int iNwGroup;
	int portMCast, portServer, portClient;
	int fileData;
	uint32_t theSrvrPeer;
	int iMaxDataSeqNumGot;
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
	me->sDataFile = NULL;
	me->sContextFile = NULL;
	me->sContextFileBase = gsContextFileBase;
	me->iNwGroup = 0;
	_snm_ports_update(me);
	me->fileData = -1;
	me->theSrvrPeer = 0;
	ll_init(&me->llLostPkts);
	me->iMaxDataSeqNumGot = -1;
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

void snm_context_fname(struct snm *me, char *sFName, char *sTag) {
	char sTmp[128];

	strncpy(sFName, me->sContextFileBase, MAIN_FPATH_LEN);
	strncat(sFName, ".context.", MAIN_FPATH_LEN);
	snprintf(sTmp, 128, "%X.", me->uCtxtId);
	strncat(sFName, sTmp, MAIN_FPATH_LEN);
	strncat(sFName, sTag, MAIN_FPATH_LEN);
	fprintf(stderr, "INFO:%s:%s\n", __func__, sFName);
}

void snm_save_context(struct snm *me, char *sTag) {
	char sFName[MAIN_FPATH_LEN];
	int iFile;
	char sTmp[128];

	snm_context_fname(me, sFName, sTag);
	iFile = open(sFName, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	if (iFile == -1) {
		perror("ERRR:save_context: Open");
		return;
	}
	snprintf(sTmp, 128, "#%s:%d\n", SC_CTXTID, me->uCtxtId);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%s\n", SC_CTXTFILEBASE, me->sContextFileBase);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%s\n", SC_DATAFILE, me->sDataFile);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%d\n", SC_MAXDATASEQGOT, me->iMaxDataSeqNumGot);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%d\n", SC_THESRVRPEER, me->theSrvrPeer);
	write(iFile, sTmp, strlen(sTmp));
	snprintf(sTmp, 128, "#%s:%d\n", SC_STATE, me->state);
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

#define ENABLE_BROADCAST 0
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

void print_gap_ifseq(int iSeq, char *sMsg) {
	static int iOldSeqs = 0;
	static int iPrevSeq = 0;

	int iSeqDelta = iSeq - iPrevSeq;
	if (iSeqDelta <= 0) {
		iOldSeqs += 1;
	}
	if (iSeqDelta > 1) {
		fprintf(stderr, "INFO:%s: gap [%d - %d]\n", sMsg, iPrevSeq+1, iSeq-1);
	}
}

int snm_handle_pi(struct snm *me, uint32_t theSrvrPeer) {
	int iRet;
	char bufS[32];
	struct sockaddr_in addrS;

	addrS.sin_family=AF_INET;
	addrS.sin_port=htons(me->portServer);
	addrS.sin_addr.s_addr = theSrvrPeer;
	*(uint32_t *)bufS = PIAckSeqNum;
	strncpy(&bufS[PKT_PIACK_NAME_OFFSET], me->sCID, CID_MAXLEN);
	*(uint32_t *)&bufS[PKT_PIACK_LOSTPKTS_OFFSET] = me->llLostPkts.iTotalFromRanges;

	if (me->theSrvrPeer != theSrvrPeer) {
		fprintf(stderr, "WARN:%s: prev peer srvr[0x%X] cmdGot from[0x%X], adjusting...\n", __func__, me->theSrvrPeer, theSrvrPeer);
		me->theSrvrPeer = theSrvrPeer;
	}

	fprintf(stderr, "INFO:%s: me [%s, %d], peerSrvr[0x%X]\n", __func__, &bufS[PKT_PIACK_NAME_OFFSET], me->llLostPkts.iTotalFromRanges, theSrvrPeer);
	iRet = sendto(me->sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
	if (iRet == -1) {
		perror("WARN:handle_pi: Failed sending PIAck");
#ifdef EXITON_NWERROR
		exit(-1);
#endif
	}
	return iRet;
}

#define UR_BUFS_LEN 800
#define UR_BUFR_LEN 1500

int snm_handle_urreq(struct snm *me, struct sockaddr_in *addrR) {
	struct sockaddr_in addrS;
	int iRet;
	char bufS[UR_BUFS_LEN];

	addrS.sin_family=AF_INET;
	addrS.sin_port=htons(me->portServer);
	addrS.sin_addr.s_addr = me->theSrvrPeer;
	*(uint32_t *)bufS = URAckSeqNum;

	if (me->theSrvrPeer != addrR->sin_addr.s_addr) {
		fprintf(stderr, "WARN:%s: prev peer srvr[0x%X] cmdGot from[0x%X], adjusting...\n", __func__, me->theSrvrPeer, addrR->sin_addr.s_addr);
		me->theSrvrPeer = addrR->sin_addr.s_addr;
		addrS.sin_addr.s_addr = me->theSrvrPeer;
	}
	int iRecords = ll_getdata(&me->llLostPkts, &bufS[PKT_URACK_DATA_OFFSET], UR_BUFS_LEN-PKT_URACK_DATA_OFFSET, 20);
#ifdef PRG_UR_VERBOSE
	ll_print_summary(&me->llLostPkts, "LostPackets");
#endif
	iRet = sendto(me->sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
	if (iRet == -1) {
		perror("WARN:handle_urreq: Failed sending URAck currently");
		return iRet;
	} else {
		fprintf(stderr, "INFO:%s: URAck [%d]records", __func__, iRecords);
	}
	fprintf(stderr, ": Lost Ranges[%d] Pkts[%d]\n", me->llLostPkts.iNodeCnt, me->llLostPkts.iTotalFromRanges);
	return 0;
}

int snm_handle_data(struct snm *me, int iSeq, char *buf, int len) {
	ll_delete(&me->llLostPkts, iSeq);
	if (me->fileData != -1) {
		filedata_save(me->fileData, &buf[PKT_DATA_OFFSET], iSeq, len-PKT_DATA_OFFSET);
	}
	return 0;
}

#define STATE_DO 0
#define STATE_DONE 1
#define STATE_ERROR 7
int snm_context_load(struct snm *me, int mode);

int snm_run(struct snm *me) {
	int iRet;
	fd_set socks;
	struct timeval tv;
	int sock;
	char bufR[UR_BUFR_LEN];
	struct sockaddr_in addrR;
	int iDataCnt, iPktCnt, iPrevPktCnt;
	time_t prevSTime, curSTime, prevMTime;

	iPktCnt = 0;
	iPrevPktCnt = 0;
	iDataCnt = 0;
	gbSNMRun = 1;
	prevSTime = time(NULL);
	prevMTime = prevSTime;
	while(gbSNMRun) {
		curSTime = time(NULL);
		int iDeltaTimeSecs = curSTime - prevSTime;
		if (iDeltaTimeSecs > STATS_TIMEDELTA) {
			int iDeltaPkts = iPktCnt - iPrevPktCnt;
			int iBytesPerSec = (iDeltaPkts*giDataSize)/iDeltaTimeSecs;
			fprintf(stderr, "INFO:%s: iPktCnt[%d] iDataCnt[%d] iSeqNo[%d] iDisjoint[%d] iRemainPkts[%d] PktBPS[%d]\n",
				__func__, iPktCnt, iDataCnt, me->iMaxDataSeqNumGot, me->llLostPkts.iNodeCnt, me->llLostPkts.iTotalFromRanges, iBytesPerSec);
			prevSTime = curSTime;
			iPrevPktCnt = iPktCnt;
		}
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
			int deltaMTime = curSTime - prevMTime;
			if (deltaMTime > MCASTREJOIN_TIMEDELTA) {
				prevMTime = curSTime;
				fprintf(stderr, "INFO:%s: silent mcast channel, rejoining again\n", __func__);
				snm_sock_mcast_ctrl(me, IP_DROP_MEMBERSHIP);
				snm_sock_mcast_ctrl(me, IP_ADD_MEMBERSHIP);
			}
			continue;
		}
		// UCast commands / data get priority
		if (FD_ISSET(me->sockUCast, &socks)) {
			sock = me->sockUCast;
#ifndef MCASTREJOIN_ALWAYS
			prevMTime = curSTime;
#endif
		} else {
			sock = me->sockMCast;
			prevMTime = curSTime;
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
			fprintf(stderr, "INFO:%s: Starting out on a new NwContext [0x%X]\n", __func__, me->uCtxtId);
			me->uCtxtId = uCTXTId;
			ll_add_sorted_startfrom_lastadded(&me->llLostPkts, 0, uTotalBlocks-1);
		}
		if (me->uCtxtId != uCTXTId) {
#ifdef CTXTAUTOLOAD
			fprintf(stderr, "WARN:%s: Switching client context from NwContext [0x%x] to [0x%X]\n", __func__, me->uCtxtId, uCTXTId);
			snm_save_context(&snmCur, "quit");
			me->uCtxtId = uCTXTId;
			int clRet = snm_context_load(&snmCur, CTXTLOAD_AUTO);
			if (clRet < 0) {
				fprintf(stderr, "WARN:%s: Issue with NwContext [0x%X] client context loading, skipping\n", __func__, me->uCtxtId);
				me->state = STATE_ERROR;
			}
#else
			fprintf(stderr, "WARN:%s: Wrong NwContext [0x%x] Expected NwContext [0x%X], Skipping\n", __func__, uCTXTId, me->uCtxtId);
			continue;
#endif
		}
		if (me->state == STATE_ERROR) {
			fprintf(stderr, "ERROR:%s: NwContext [0x%X] in error state, skipping\n", __func__, me->uCtxtId);
		}
		if (iSeq == URReqSeqNum) {
			iRet = snm_handle_urreq(me, &addrR);
#ifdef EXITON_NWERROR
			if (iRet != 0) {
				exit(-1);
			}
#endif
		} else if (iSeq == PIReqSeqNum) {
			snm_handle_pi(me, addrR.sin_addr.s_addr);
		} else {
			if ((iSeq & SeqNumCmdsCmn) == SeqNumCmdsCmn) {
				fprintf(stderr, "DEBG:%s: Unexpected command [0x%X], skipping, check why\n", __func__, iSeq);
				continue;
			}
			if (me->iMaxDataSeqNumGot < iSeq) {
				me->iMaxDataSeqNumGot = iSeq;
			}
			iDataCnt += 1;
			snm_handle_data(me, iSeq, bufR, iRet);
		}
		if ((me->state == STATE_DO) && (me->llLostPkts.iNodeCnt == 0)) {
			fprintf(stderr, "INFO:%s: All data got\n", __func__);
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

	if (strncmp(sLine, SC_CTXTIDEX, SC_CTXTIDEX_LEN) == 0) {
		me->uCtxtId = strtol(&sLine[SC_CTXTIDEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded uCtxtId [%d]\n", __func__, me->uCtxtId);
	}
	if (strncmp(sLine, SC_DATAFILEEX, SC_DATAFILEEX_LEN) == 0) {
		gstDataFile[0] = 0;
		strncpy(gstDataFile, &sLine[SC_DATAFILEEX_LEN], MAIN_FPATH_LEN);
		gstDataFile[iLineLen-SC_DATAFILEEX_LEN-1] = 0;
		me->sDataFile = gstDataFile;
		fprintf(stderr, "INFO:%s: loaded sDataFile [%s]\n", __func__, me->sDataFile);
	}
	if (strncmp(sLine, SC_CTXTFILEBASEEX, SC_CTXTFILEBASEEX_LEN) == 0) {
		gsContextFileBase[0] = 0;
		strncpy(gsContextFileBase, &sLine[SC_CTXTFILEBASEEX_LEN], MAIN_FPATH_LEN);
		gsContextFileBase[iLineLen-SC_CTXTFILEBASEEX_LEN-1] = 0;
		me->sContextFileBase = gsContextFileBase;
		fprintf(stderr, "INFO:%s: loaded sContextFileBase [%s]\n", __func__, me->sContextFileBase);
	}
	if (strncmp(sLine, SC_MAXDATASEQGOTEX, SC_MAXDATASEQGOTEX_LEN) == 0) {
		me->iMaxDataSeqNumGot = strtol(&sLine[SC_MAXDATASEQGOTEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded iMaxDataSeqNumGot [%d]\n", __func__, me->iMaxDataSeqNumGot);
	}
	if (strncmp(sLine, SC_THESRVRPEEREX, SC_THESRVRPEEREX_LEN) == 0) {
		me->theSrvrPeer = strtol(&sLine[SC_THESRVRPEEREX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded theSrvrPeer [%d]\n", __func__, me->theSrvrPeer);
	}
	if (strncmp(sLine, SC_STATEEX, SC_STATEEX_LEN) == 0) {
		me->state = strtol(&sLine[SC_STATEEX_LEN], NULL, 10);
		fprintf(stderr, "INFO:%s: loaded state [%d]\n", __func__, me->state);
	}
}

int snm_context_load(struct snm *me, int mode) {
	int iRet = 0;

	if (mode == CTXTLOAD_AUTO) {
		snm_context_fname(me, gstContextFile, "quit");
		me->sContextFile = gstContextFile;
	}

	if (me->sContextFile != NULL) {
		fprintf(stderr, "INFO:%s: About to load context including lostpacketRanges from [%s]...\n", __func__, me->sContextFile);
		iRet = ll_load_append_ex(&me->llLostPkts, me->sContextFile, '#', _snm_context_load, me);
	}
	return iRet;
}

void snm_args_process_p1(struct snm *me) {
	_snm_ports_update(me);
	if (snm_context_load(me, CTXTLOAD_INIT) < 0) {
		exit(2);
	}
	if (snm_datafile_open(me) < 0) {
		exit(1);
	}
}

void signal_handler(int arg) {
	snm_save_context(&snmCur, "quit");
	exit(2);
}

#define ARG_MADDR "--maddr"
#define ARG_FILE "--file"
#define ARG_LOCAL "--local"
#define ARG_CONTEXT "--context"
#define ARG_CONTEXTBASE "--contextbase"
#define ARG_NWGROUP "--nwgroup"
#define ARG_CID "--cid"

void print_usage(void) {
	fprintf(stderr, "usage: simpnwmon02 <--maddr mcast_addr> <--local ifIndex4mcast local_addr> <--file datafile>\n");
	fprintf(stderr, "\t Optional args\n");
	fprintf(stderr, "\t --context contextfile_to_load # If specified, the list of lost pkt ranges is initialised with its contents\n");
	fprintf(stderr, "\t --contextbase contextfile_basename_forsaving\n");
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
		if (strcmp(argv[iArg], ARG_CONTEXT) == 0) {
			iArg += 1;
			me->sContextFile = argv[iArg];
		}
		if (strcmp(argv[iArg], ARG_CONTEXTBASE) == 0) {
			iArg += 1;
			me->sContextFileBase = argv[iArg];
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
	if ((me->sMCastAddr == NULL) || (me->sLocalAddr == NULL) || (me->sDataFile == NULL)) {
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
	snm_save_context(&snmCur, "end");

	ll_print(&snmCur.llLostPkts, "LostPackets before exit");
	ll_free(&snmCur.llLostPkts);
	return 0;
}

