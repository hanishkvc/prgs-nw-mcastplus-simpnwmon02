#!/usr/bin/env python3

#
# hkvc-nw-recover.py
# A test script which collates all available active clients in the network
# which talk the same language. In turn it tries to help recover lost packets
# wrt each of the client using unicast tranfers.
# v20190104IST2239
# HanishKVC, GPL, 19XY
#

import os
import sys
import time
import socket
import struct
import select


portServer = 1112
portClient = 1113

PITotalTimeSecs = 10*60
PIReqSeqNum = 0xffffff00
PIAckSeqNum = 0xffffff01
URDeltaTimeSecs = 3*60
URReqSeqNum = 0xffffff02
URAckSeqNum = 0xffffff03
URCLIENT_MAXCHANCES_PERATTEMPT = 512


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


port=1111
N=11
dataSize=1024
Bps=2e6
addr="127.0.0.1"
sfData=None
sMode="NORMAL"

iArg=1
while iArg < len(sys.argv):
	if (sys.argv[iArg] == "--port"):
		iArg += 1
		port=int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--dim"):
		iArg += 1
		N = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--datasize"):
		iArg += 1
		dataSize = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--Bps"):
		iArg += 1
		Bps = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--addr"):
		iArg += 1
		addr = sys.argv[iArg]
	elif (sys.argv[iArg] == "--file"):
		iArg += 1
		sfData = sys.argv[iArg]
	elif (sys.argv[iArg] == "--fast"):
		sMode="FAST"
	iArg += 1

fData=None
if (sfData != None):
	fData = open(sfData, 'br')

perPktTime=1/(Bps/dataSize)
dprint(9, " addr [{}], port [{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(addr, port, N, dataSize, Bps, perPktTime))

if (sMode == "FAST"):
	PITotalTimeSecs=1*60
dprint(9, " portServer [{}], portClient [{}]".format(portServer, portClient))
dprint(9, " sMode=[{}], PITotalTimeSecs=[{}]\n".format(sMode, PITotalTimeSecs))

socket.setdefaulttimeout(1)
sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)
addr = "0.0.0.0"
dprint(9, "Listening on [{}:{}]".format(addr, portServer))
sock.bind((addr, portServer))

dprint(9, "Will start in 10 secs...")
time.sleep(10)


clients = []
def presence_info():
	global sock
	dprint(9, "PresenceInfo: Listening on [{}:{}]".format(addr, portServer))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < PITotalTimeSecs):
		dprint(8, "PI:{}_{}".format(iCnt,deltaTime))
		try:
			d = sock.recvfrom(128)
			dataC = d[0]
			peer = d[1][0]
			try:
				i = clients.index(peer)
				dprint(9, "Rcvd from known client:{}:{}".format(peer, dataC))
			except ValueError as e:
				dprint(9, "Rcvd from new client:{}:{}".format(peer, dataC))
				clients.append(peer)
			data=struct.pack("<Is", PIAckSeqNum, bytes("Hello", 'utf8'))
			sock.sendto(data, (peer, portClient))
		except socket.timeout as e:
			d = None
			dprint(7, ".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
	dprint(9, "PI identified following clients:")
	for r in clients:
		dprint(9, r)


def gen_lostpackets_array(lostPackets):
	lp = lostPackets
	lp = lp.split(b'\n')
	lpa = []
	for r in lp:
		if (r == b''):
			continue
		if (r[0] == 0):
			continue
		r = r.split(b'-')
		s = int(r[0])
		e = int(r[1])
		for i in range(s,e+1):
			lpa.append(i)
	return lpa


def ur_send_packets(client, lostPackets):
	dprint(2, "ur_send_packets: client[{}] LostPackets:[{}]".format(client, lostPackets))
	lpa = gen_lostpackets_array(lostPackets)
	dprint(9, "ur_send_packets: client[{}] lostPackets count [{}]".format(client, len(lpa)))
	iPkts = len(lpa)
	if (iPkts != 0):
		send_file_data(client, lpa)
	return iPkts


def ur_client(client):
	global sock
	dprint(9, "UnicastRecovery: Listening on [{}:{}] for client [{}:{}]".format(addr, portServer, client, portClient))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	giveupReason = "No Response"
	while(deltaTime < URDeltaTimeSecs):
		dprint(8, "UR:{}_{}".format(iCnt,deltaTime))
		data=struct.pack("<Is", URReqSeqNum, bytes("Hello", 'utf8'))
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
				return 0
			if (ur_send_packets(client, data) == 0):
				dprint(9, "UR:INFO: No MORE lost packets for [{}]".format(client))
				return 0
			startTime = time.time() # This ensures that if the client doesn't respond for the specified amount of time, it will skip to the next client
		except socket.timeout as e:
			d = None
			dprint(7, ".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
		if (iCnt > URCLIENT_MAXCHANCES_PERATTEMPT):
			giveupReason = "TooMany LostPackets???"
			break
	dprint(9, "UR:WARN: Giving up on [{}] temporarily bcas [{}]".format(client, giveupReason))
	return -1


def unicast_recovery(clients):
	remainingClients = []
	for client in clients:
		if (ur_client(client) != 0):
			remainingClients.append(client)
	dprint(9, "Remaining clients:")
	for r in remainingClients:
		dprint(9, r)
	return remainingClients


def send_file_data(peer, indexList):
	prevPktid=0
	prevTime=time.time()
	curTime=0.0
	prevTimeThrottle=time.time()
	pktid = 0
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
		data=struct.pack("<I{}s".format(dataSize), i, curData)
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
		if ((pktid%(N*N*2)) == 0):
			curTime=time.time()
			numPkts = pktid - prevPktid
			timeDelta = curTime - prevTime
			nwSpeed= ((numPkts*dataSize)/timeDelta)/1e6
			dprint(8, "Transfer speed [{}]MBps\n".format(nwSpeed))
			#time.sleep(1)
			prevTime = time.time()
			prevPktid = pktid




presence_info()
for i in range(3):
	clients = unicast_recovery(clients)
	if(len(clients) == 0):
		break

if (len(clients) == 0):
	dprint(9, "INFO: Done with transfer")
else:
	dprint(9, "INFO: Giving up, clients still with lost packets [{}]".format(clients))

