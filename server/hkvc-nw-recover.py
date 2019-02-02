#!/usr/bin/env python3

#
# hkvc-nw-recover.py
# A test script which collates all available active clients in the network
# which talk the same language. In turn it tries to help recover lost packets
# wrt each of the client using unicast tranfers.
# v20190201IST2345
# HanishKVC, GPL, 19XY
#

import os
import sys
import time
import socket
import struct
import select

import context
import network
import status


portMCast = 1111
portServer = 1112
portClient = 1113

URDeltaTimeSecs = int(1.5*60)
URReqSeqNum = 0xffffff02
URAckSeqNum = 0xffffff03
URCLIENT_MAXCHANCES_PERATTEMPT = 512
URATTEMPTS_MIN = 4
giNumOfAttempts = URATTEMPTS_MIN
iTestBlocks=1e6


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


def guess_numofattempts(totalBlocksInvolved):
	dLossPercent = 0.08
	dBuffer = 1.5
	iAvgNumOfPacketsPerRange = 8
	iNumOfRangesPerChance = 20
	iAvgNumOfPacketsPerChance = iNumOfRangesPerChance*iAvgNumOfPacketsPerRange
	iLost = int(totalBlocksInvolved*dLossPercent)
	iAttempts = int((iLost/(URCLIENT_MAXCHANCES_PERATTEMPT*iAvgNumOfPacketsPerChance))*dBuffer)
	if (iAttempts < URATTEMPTS_MIN):
		iAttempts = URATTEMPTS_MIN
	return iAttempts


status.open()
nwGroup=0
N=11
dataSize=network.dataSize
Bps=2e6
laddr="0.0.0.0"
maddr=network.maddr
sfData=None
sPIMode="NORMAL"
gsContext=None

iArg=1
while iArg < len(sys.argv):
	if (sys.argv[iArg] == "--nwgroup"):
		iArg += 1
		nwGroup = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--dim"):
		iArg += 1
		N = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--datasize"):
		iArg += 1
		dataSize = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--Bps"):
		iArg += 1
		Bps = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--laddr"):
		iArg += 1
		laddr = sys.argv[iArg]
	elif (sys.argv[iArg] == "--maddr"):
		iArg += 1
		maddr = sys.argv[iArg]
	elif (sys.argv[iArg] == "--file"):
		iArg += 1
		sfData = sys.argv[iArg]
	elif (sys.argv[iArg] == "--testblocks"):
		iArg += 1
		iTestBlocks = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--ncid"):
		iArg += 1
		network.CtxtId = int(sys.argv[iArg], 0)
	elif (sys.argv[iArg] == "--context"):
		iArg += 1
		gsContext = sys.argv[iArg]
	elif (sys.argv[iArg] == "--slow"):
		sPIMode="SLOW"
	iArg += 1


portMCast, portServer, portClient = network.ports_ngupdate(nwGroup)

if (gsContext != None):
	dprint(9, "INFO:Context: importing from [{}]".format(gsContext))
	gContext = context.open(gsContext)
	gClients = context.load_clients(gContext)
	context.close(gContext)
else:
	dprint(9, "INFO:Context: None")
	gClients = []
dprint(9, "INFO:Clients[{}]".format(gClients))

fData=None
if (sfData != None):
	print("MODE:FileTransfer:{}".format(sfData))
	fData = open(sfData, 'br')
	fileSize = os.stat(fData.fileno()).st_size;
	giTotalBlocksInvolved = int(fileSize/dataSize)
	if ((fileSize%dataSize) != 0):
		giTotalBlocksInvolved += 1
else:
	print("MODE:TestBlocks:{}".format(iTestBlocks))
	giTotalBlocksInvolved = int(iTestBlocks)

giNumOfAttempts = guess_numofattempts(giTotalBlocksInvolved)

perPktTime=1/(Bps/dataSize)
dprint(9, "laddr [{}]".format(laddr))
dprint(9, "maddr [{}], portMCast [{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(maddr, portMCast, N, dataSize, Bps, perPktTime))

if (sPIMode == "SLOW"):
	PITotalTimeSecs=10*60
dprint(9, " portServer [{}], portClient [{}]".format(portServer, portClient))
dprint(9, " sPIMode=[{}], PITotalTimeSecs=[{}]\n".format(sPIMode, PITotalTimeSecs))

dprint(9, "giTotalBlocksInvolved [{}], URCLIENT_MAXCHANCES_PERATTEMPT [{}] giNumOfAttempts [{}]\n".format(giTotalBlocksInvolved, URCLIENT_MAXCHANCES_PERATTEMPT, giNumOfAttempts))

socket.setdefaulttimeout(1)
sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)
dprint(9, "Listening on [{}:{}]".format(laddr, portServer))
sock.bind((laddr, portServer))

dprint(9, "Will start in 10 secs...")
time.sleep(10)


def gen_lostpackets_array(lostPackets):
	iRangesCnt = -1
	iLostPkts = -1
	lp = lostPackets
	lp = lp.split(b'\n')
	lpa = []
	for r in lp:
		if (r == b''):
			continue
		if (r[0] == 0):
			continue
		r = r.split(b'-')
		if (r[0].startswith(b"IRanges")):
			iRangesCnt = int(r[1])
			continue
		if (r[0].startswith(b"ILost")):
			iLostPkts = int(r[1])
			continue
		s = int(r[0])
		e = int(r[1])
		for i in range(s,e+1):
			lpa.append(i)
	return lpa, iRangesCnt, iLostPkts


def ur_send_packets(client, lostPackets):
	dprint(2, "ur_send_packets: client[{}] LostPackets:[{}]".format(client, lostPackets))
	lpa, iRangesCnt, iLostPkts = gen_lostpackets_array(lostPackets)
	dprint(9, "\nur_send_packets: client[{}] lostPackets curCount [{}], StillInTotal(Ranges[{}], LostPkts[{}])".format(client, len(lpa), iRangesCnt, iLostPkts))
	iPkts = len(lpa)
	status.ucast_ur(client, iPkts, iLostPkts)
	if (iPkts != 0):
		send_file_data(client, lpa)
	return iPkts, iLostPkts


def ur_client(client):
	global sock
	sock.settimeout(10.0)
	dprint(9, "UnicastRecovery: Listening on [{}:{}] for client [{}:{}]".format(laddr, portServer, client, portClient))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	giveupReason = "No Response"
	iRemLostPkts = -1
	while(deltaTime < URDeltaTimeSecs):
		if ((iCnt % 30) == 0):
			dprint(8, "UR:{}_{}".format(iCnt,deltaTime))
		data=struct.pack("<IIII4s", URReqSeqNum, network.CtxtId, giTotalBlocksInvolved, giTotalBlocksInvolved, bytes("Helo", 'utf8'))
		sock.sendto(data, (client, portClient))
		try:
			d = sock.recvfrom(1024)
			dataC = d[0]
			peer = d[1][0]
			if client != peer:
				dprint(8, "UR:WARN:WrongPeer: Expected from Peer [{}], Got from Peer [{}]".format(client, peer))
				continue
			dprint(2, dataC)
			cmd = struct.unpack("<I", dataC[0:4])[0]
			if cmd != URAckSeqNum:
				dprint(8, "UR:WARN: Peer has sent wrong resp[{}], skipping the same...".format(hex(cmd)))
				continue
			data = dataC[4:]
			if (data == b''):
				dprint(9, "UR:INFO: No MORE lost packets for [{}]".format(client))
				return 0, -1
			iCurLostPkts, iRemLostPkts = ur_send_packets(client, data)
			if (iCurLostPkts == 0):
				dprint(9, "UR:INFO: No MORE lost packets for [{}]".format(client))
				return 0, iRemLostPkts
			startTime = time.time() # This ensures that if the client doesn't respond for the specified amount of time, it will skip to the next client
		except socket.timeout as e:
			d = None
			dprint(7, ".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
		if (iCnt > URCLIENT_MAXCHANCES_PERATTEMPT):
			status.ucast_ur(client, -1, iRemLostPkts)
			giveupReason = "TooMany LostPackets???"
			break
	dprint(9, "UR:WARN: Giving up on [{}] temporarily bcas [{}]".format(client, giveupReason))
	return -1, iRemLostPkts


def unicast_recovery(clients):
	remainingClients = []
	remClientsDB = {}
	for client in clients:
		iRet, iRemLostPkts = ur_client(client)
		if (iRet != 0):
			remainingClients.append(client)
			remClientsDB[client] = iRemLostPkts
	dprint(9, "Remaining clients:")
	for r in remainingClients:
		dprint(9, "{} potential RemPkts[{}]".format(r, remClientsDB[r]))
	status.ucast_ur_summary(remClientsDB)
	return remainingClients


def print_throughput(prevTime, pktid, prevPktid):
	curTime=time.time()
	numPkts = pktid - prevPktid
	timeDelta = curTime - prevTime
	nwSpeed= ((numPkts*dataSize)/timeDelta)/1e6
	dprint(8, "Transfer speed [{}]MBps, pktCnt[{}]".format(nwSpeed, pktid))
	return curTime, pktid


def send_file_data(peer, indexList):
	iNumPkts = len(indexList)
	iStatMod = ((iNumPkts/(N*N))/4)
	prevPktid=0
	prevTime=time.time()
	curTime=0.0
	prevTimeThrottle=time.time()
	pktid = 0
	iStatCnt = 0
	for i in indexList:
		dprint(2, "send_file_data: sending data for index [{}]\n".format(i))
		if (fData != None):
			fData.seek(i*dataSize)
			curData = fData.read(dataSize)
			if (curData == b''):
				break
		else:
			tmpData = bytes(dataSize-4)
			curData = struct.pack("<I{}s".format(dataSize-4), i, tmpData)
		data=struct.pack("<IIII{}s".format(dataSize), i, network.CtxtId, giTotalBlocksInvolved, giTotalBlocksInvolved, curData)
		sock.sendto(data, (peer, portClient))
		pktid += 1
		if ((pktid%N) == 0):
			curTime = time.time()
			timeAlloted = (perPktTime*N)
			timeUsed = curTime-prevTimeThrottle
			timeRemaining = timeAlloted - timeUsed
			dprint(0, "timeAlloted [{}], timeRemaining[{}]".format(timeAlloted, timeRemaining))
			if (timeRemaining > 0):
				(rlist, wlist, xlist) = select.select([0],[],[],timeRemaining)
				if (len(rlist) == 1):
					a=input()
			prevTimeThrottle = time.time()
		if ((pktid%(N*N*512)) == 0):
			prevTime, prevPktid = print_throughput(prevTime, pktid, prevPktid)
	print_throughput(prevTime, pktid, prevPktid)




presence_info(gClients)
for i in range(giNumOfAttempts):
	dprint(9, "INFO: UCastRecovery Global Attempt [{} of {}]".format(i, giNumOfAttempts))
	gClients = unicast_recovery(gClients)
	if(len(gClients) == 0):
		break

if (len(gClients) == 0):
	dprint(9, "INFO: Done with transfer")
else:
	dprint(9, "INFO: Giving up, clients still with lost packets [{}]".format(gClients))

