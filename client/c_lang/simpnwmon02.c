/*
    Simple Network Monitor 02 - C version
    v20190113IST0200
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
const int MCASTSLOWEXIT_CNT=3;		// 20*3 = atleast 60secs of No MCast stop commands, after recieving atleast 1 stop command
const int MCAST_USLEEP=1000;
const int UCAST_PI_USLEEP=1000000;
const int PI_RETRYCNT = 600;		// 600*1e6uSecs = Client will try PI for atleast 600 seconds
const int UCAST_UR_USLEEP=1000;
const int PKT_SEQNUM_OFFSET=0;
const int PKT_DATA_OFFSET=4;
const int PKT_MCASTSTOP_TOTALBLOCKS_OFFSET=8;

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

#define BUF_MAXSIZE 1600
int giDataSize=(1024+4);
char gcBuf[BUF_MAXSIZE];

int gbDoMCast = 1;

struct snm {
	int sockMCast, sockUCast;
	int iLocalIFIndex;
	char *sMCastAddr;
	char *sLocalAddr;
	char *sBCastAddr;
	char *sDataFile;
	int iRunModes;
	char *sContextFile;
};

#define RUNMODE_MCAST 0x01
#define RUNMODE_UCASTPI 0x02
#define RUNMODE_UCASTUR 0x04
#define RUNMODE_UCAST 0x06
#define RUNMODE_ALL 0x07

void snm_init(struct snm *me) {
	me->sockMCast = -1;
	me->sockUCast = -1;
	me->iLocalIFIndex = 0;
	me->sMCastAddr = NULL;
	me->sLocalAddr = NULL;
	me->sBCastAddr = NULL;
	me->sDataFile = NULL;
	me->iRunModes = RUNMODE_ALL;
	me->sContextFile = NULL;
}

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
	off64_t iRet64;
	int iExpectedDataLen = giDataSize-PKT_DATA_OFFSET;

	if (len != iExpectedDataLen) {
		fprintf(stderr, "WARN:%s:ExpectedDataSize[%d] != pktDataLen[%d]\n", __func__, iExpectedDataLen, len);
	}
	iRet64 = lseek64(fileData, (off64_t)iBlockOffset*iExpectedDataLen, SEEK_SET);
	if (iRet64 == -1) {
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

void _account_lostpackets(struct LLR *llrLostPkts, int start, int end, int *piDisjointSeqs, int *piDisjointPktCnt) {
	ll_add_sorted_startfrom_lastadded(llrLostPkts, start, end);
	*piDisjointSeqs += 1;
	*piDisjointPktCnt += (end-start+1);
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
	int iMaxDataSeqNumGot = -1;
	int iActualLastSeqNum = -1;

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
				fprintf(stderr, "INFO:%s: In MCastStop phase... (LastPkt Got[%d] Actual[%d]) ", __func__, iMaxDataSeqNumGot, iActualLastSeqNum);
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
			iActualLastSeqNum = *((uint32_t*)&gcBuf[PKT_MCASTSTOP_TOTALBLOCKS_OFFSET]) - 1;
			if (iRecvdStop == 0) {
				if (iMaxDataSeqNumGot != iActualLastSeqNum) {
					_account_lostpackets(llLostPkts, iMaxDataSeqNumGot+1, iActualLastSeqNum, &iDisjointSeqs, &iDisjointPktCnt);
				}
			}
			iRecvdStop += 1;
			continue;
		}
		iMaxDataSeqNumGot = iSeq;
		iSeqDelta = iSeq - iPrevSeq;
		if (iSeqDelta <= 0) {
			iOldSeqs += 1;
			continue;
		}
		if (iSeqDelta > 1) {
			//fprintf(stderr, "DEBUG:%s: iSeq[%d] iPrevSeq[%d] iSeqDelta[%d] iDisjointSeqs[%d] iDisjointPktCnt[%d]\n", __func__, iSeq, iPrevSeq, iSeqDelta, iDisjointSeqs, iDisjointPktCnt);
			ll_add_sorted_startfrom_lastadded(llLostPkts, iPrevSeq+1, iSeq-1);
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
			usleep(UCAST_PI_USLEEP);
		}
	}
	return -1;
}

#define UR_BUFS_LEN 800
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
				fprintf(stderr, "WARN:%s: prev peer srvr[0x%X] cmdGot from[0x%X], adjusting...\n", __func__, theSrvrPeer, addrR.sin_addr.s_addr);
				theSrvrPeer = addrR.sin_addr.s_addr;
				addrS.sin_addr.s_addr = theSrvrPeer;
			}
			int iRecords = ll_getdata(llLostPkts, &bufS[4], UR_BUFS_LEN-4, 20);
#ifdef PRG_UR_VERBOSE
			ll_print_summary(llLostPkts, "LostPackets");
#endif
			iRet = sendto(sockUCast, bufS, sizeof(bufS), 0, (struct sockaddr *)&addrS, sizeof(addrS));
			if (iRet == -1) {
				perror("Failed sending URAck");
				exit(-1);
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

struct LLR *gpllLostPkts = NULL;
#define MAIN_FPATH_LEN 256
char gsContextFileBase[MAIN_FPATH_LEN] = "/tmp/snm02";

void save_context(struct LLR *meLLR, char *sBase, char *sTag) {
	int iRet;
	char sFName[MAIN_FPATH_LEN];

	strncpy(sFName, sBase, MAIN_FPATH_LEN);
	strncat(sFName, ".lostpackets.", MAIN_FPATH_LEN);
	strncat(sFName, sTag, MAIN_FPATH_LEN);

	if (meLLR == NULL) {
		fprintf(stderr, "WARN:%s:%s: Passed LostPacketRanges ll not yet setup\n", __func__, sTag);
	} else {
		iRet = ll_save(meLLR, sFName);
		fprintf(stderr, "INFO:%s:%s: LostPacketRanges ll saved [%d of %d] to [%s]\n", __func__, sTag, iRet, meLLR->iNodeCnt, sFName);
	}
}

void signal_handler(int arg) {
	save_context(gpllLostPkts, gsContextFileBase, "quit");
	exit(2);
}

#define ARG_MADDR "--maddr"
#define ARG_FILE "--file"
#define ARG_LOCAL "--local"
#define ARG_BCAST "--bcast"
#define ARG_CONTEXT "--context"
#define ARG_RUNMODES "--runmodes"

void print_usage(void) {
	fprintf(stderr, "usage: simpnwmon02 <--maddr mcast_addr> <--local ifIndex4mcast local_addr> <--file datafile> <--bcast pi_nw_bcast_addr> [--context lostpkts_infofile_4resume]\n");
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
		if (strcmp(argv[iArg], ARG_RUNMODES) == 0) {
			iArg += 1;
			me->iRunModes = strtol(argv[iArg], NULL, 0);
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
	return 0;
}

int main(int argc, char **argv) {
	struct snm snmCur;

	int sockMCast = -1;
	int sockUCast = -1;
	struct LLR llLostPkts;
	uint32_t theSrvrPeer = 0;
	int iRet = -1;

	snm_init(&snmCur);
	snm_parse_args(&snmCur, argc, argv);

	if (signal(SIGINT, &signal_handler) == SIG_ERR) {
		perror("WARN:main:Failed setting SIGINT handler");
	}

	int ifIndex = snmCur.iLocalIFIndex;
	char *sMCastAddr = snmCur.sMCastAddr;
	char *sLocalAddr = snmCur.sLocalAddr;
	char *sPINwBCast = snmCur.sBCastAddr;
	int fileData = open(snmCur.sDataFile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); // Do I need truncate to think later. Also if writing to device files, then have to re-evaluate the flags
	if (fileData == -1) {
		fprintf(stderr, "WARN:%s: Failed to open data file [%s], saving data will be skipped\n", __func__, snmCur.sDataFile);
		perror("Failed datafile open");
	} else {
		fprintf(stderr, "INFO:%s: opened data file [%s]\n", __func__, snmCur.sDataFile);
	}

	ll_init(&llLostPkts);
	gpllLostPkts = &llLostPkts;

	if (snmCur.sContextFile != NULL) {
		fprintf(stderr, "INFO:%s: About to load lostpacketRanges from [%s]...\n", __func__, snmCur.sContextFile);
		iRet = ll_load_append(&llLostPkts, snmCur.sContextFile);
		if (iRet == -1) {
			return 2;
		}
	}

	sockMCast = sock_mcast_init_ex(ifIndex, sMCastAddr, portMCast, sLocalAddr);
	if ((snmCur.iRunModes & RUNMODE_MCAST) == RUNMODE_MCAST) {
		mcast_recv(sockMCast, fileData, &llLostPkts);
		save_context(&llLostPkts, gsContextFileBase, "mcast");
	} else {
		fprintf(stderr, "INFO:%s: Skipping mcast:recv...\n", __func__);
	}
	ll_print(&llLostPkts, "LostPackets at end of MCast");

	sockUCast = sock_ucast_init(sLocalAddr, portClient);
	if ((snmCur.iRunModes & RUNMODE_UCASTPI) == RUNMODE_UCASTPI) {
		if (ucast_pi(sockUCast, sPINwBCast, portServer, &theSrvrPeer) < 0) {
			fprintf(stderr, "WARN:%s: No Server found during PI Phase, however giving ucast_recover a chance...\n", __func__);
		}
	} else {
		fprintf(stderr, "INFO:%s: Skipping ucast:pi...\n", __func__);
	}
	if ((snmCur.iRunModes & RUNMODE_UCASTUR) == RUNMODE_UCASTUR) {
		ucast_recover(sockUCast, fileData, theSrvrPeer, portServer, &llLostPkts);
	} else {
		fprintf(stderr, "INFO:%s: Skipping ucast:ur...\n", __func__);
	}

	ll_free(&llLostPkts);
	return 0;
}

