#!/usr/bin/env python3

#
# ucast-recover.py
# A module which collates all available active clients in the network
# which talk the same language. In turn it tries to help recover lost packets
# wrt each of the client using unicast tranfers.
# HanishKVC, GPL, 19XY
#

import time
import socket
import struct
import select

import network
import status


URDeltaTimeSecs = int(1.5*60)
URReqSeqNum = 0xffffff02
URAckSeqNum = 0xffffff03
URCLIENT_MAXCHANCES_PERATTEMPT = 512
URATTEMPTS_MIN = 4


fData = None
sock = None
laddr = None
portServer = None
portClient = None
giTotalBlocksInvolved = None
dataSize = None
N = None


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)



def init(fDataIn, sockIn, laddrIn, portServerIn, portClientIn, iTotalBlocksInvolved, dataSizeIn, Nin):
	global fData
	global sock
	global laddr, portServer, portClient
	global giTotalBlocksInvolved, dataSize, N
	fData = fDataIn
	sock = sockIn
	laddr = laddrIn
	portServer = portServerIn
	portClient = portClientIn
	giTotalBlocksInvolved = iTotalBlocksInvolved
	dataSize = dataSizeIn
	N = Nin



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
		data=struct.pack("<IIII4s", URReqSeqNum, network.CtxtId, network.CtxtVer, giTotalBlocksInvolved, bytes("Helo", 'utf8'))
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
		data=struct.pack("<IIII{}s".format(dataSize), i, network.CtxtId, network.CtxtVer, giTotalBlocksInvolved, curData)
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


