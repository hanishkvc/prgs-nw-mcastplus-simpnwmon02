#!/usr/bin/env python3

#
# hkvc-nw-recover.py
# A test script which collates all available active clients in the network
# which talk the same language. In turn it tries to help recover lost packets
# wrt each of the client using unicast tranfers.
# v20181228IST0447
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
print(" addr [{}], port [{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(addr, port, N, dataSize, Bps, perPktTime))

if (sMode == "FAST"):
	PITotalTimeSecs=1*60
print(" portServer [{}], portClient [{}]".format(portServer, portClient))
print(" sMode=[{}], PITotalTimeSecs=[{}]\n".format(sMode, PITotalTimeSecs))

socket.setdefaulttimeout(1)
sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)
addr = "0.0.0.0"
print("Listening on [{}:{}]".format(addr, portServer))
sock.bind((addr, portServer))

print("Will start in 10 secs...")
time.sleep(10)


clients = []
def presence_info():
	global sock
	print("PresenceInfo: Listening on [{}:{}]".format(addr, portServer))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < PITotalTimeSecs):
		print("PI:{}_{}".format(iCnt,deltaTime))
		try:
			d = sock.recvfrom(128)
			dataC = d[0]
			peer = d[1][0]
			try:
				i = clients.index(peer)
				print("Rcvd from known client:{}:{}".format(peer, dataC))
			except ValueError as e:
				print("Rcvd from new client:{}:{}".format(peer, dataC))
				clients.append(peer)
			data=struct.pack("<Is", PIAckSeqNum, bytes("Hello", 'utf8'))
			sock.sendto(data, (peer, portClient))
		except socket.timeout as e:
			d = None
			print(".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
	print("PI identified following clients:")
	for r in clients:
		print(r)


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
	print("ur_send_packets: client[{}] LostPackets:[{}]".format(client, lostPackets))
	lpa = gen_lostpackets_array(lostPackets)
	print("ur_send_packets: lostPackets count [{}]".format(len(lpa)))
	iPkts = len(lpa)
	send_file_data(client, lpa)
	return iPkts


def ur_client(client):
	global sock
	print("UnicastRecovery: Listening on [{}:{}] for client [{}:{}]".format(addr, portServer, client, portClient))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < URDeltaTimeSecs):
		print("UR:{}_{}".format(iCnt,deltaTime))
		data=struct.pack("<Is", URReqSeqNum, bytes("Hello", 'utf8'))
		sock.sendto(data, (client, portClient))
		try:
			d = sock.recvfrom(1024)
			dataC = d[0]
			peer = d[1][0]
			if client != peer:
				print("UR:WARN:WrongPeer: Expected from Peer [{}], Got from Peer [{}]".format(client, peer))
				continue
			print(dataC)
			cmd = struct.unpack("<I", dataC[0:4])[0]
			if cmd != URAckSeqNum:
				print("UR:WARN: Peer has sent wrong resp[{}], skipping the same...".format(hex(cmd)))
				continue
			data = dataC[4:]
			if (data == b''):
				print("UR:INFO: No MORE lost packets for [{}]".format(client))
				return
			if (ur_send_packets(client, data) == 0):
				print("UR:INFO: No MORE lost packets for [{}]".format(client))
				return
			startTime = time.time()
		except socket.timeout as e:
			d = None
			print(".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1


def unicast_recovery():
	global clients

	for client in clients:
		ur_client(client)
	print("Remaining clients:") # TODO: Need to update the clients list
	for r in clients:
		print(r)


def send_file_data(peer, indexList):
	prevPktid=0
	prevTime=time.time()
	curTime=0.0
	prevTimeThrottle=time.time()
	pktid = 0
	for i in indexList:
		#print("send_file_data: sending data for index [{}]\n".format(i))
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
			#print("timeAlloted [{}], timeRemaining[{}]".format(timeAlloted, timeRemaining))
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
			print("Transfer speed [{}]MBps\n".format(nwSpeed))
			#time.sleep(1)
			prevTime = time.time()
			prevPktid = pktid




presence_info()
unicast_recovery()
print("INFO: Done with transfer")

