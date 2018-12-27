#!/usr/bin/env python3

import os
import sys
import time
import socket
import struct
import select


portServer = 1112
portClient = 1113

PITotalTimeSecs = 10*60
PITotalTimeSecs = 3*60
PIReqSeqNum = 0xffffff00
PIAckSeqNum = 0xffffff01
URDeltaTimeSecs = 3*60
URReqSeqNum = 0xffffff02
URAckSeqNum = 0xffffff03


pktid=0
port=1111
N=11
dataSize=1024
Bps=2e6
addr="127.0.0.1"
sfData=None

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
	iArg += 1

fData=None
if (sfData != None):
	fData = open(sfData, 'br')

perPktTime=1/(Bps/dataSize)
print(" addr [{}], port [{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(addr, port, N, dataSize, Bps, perPktTime))

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


def ur_send_packets(lostPackets):
	print("LostPackets:[{}]".format(lostPackets))


def ur_client(client):
	global sock
	print("UnicastRecovery: Listening on [{}:{}] for client [{}]".format(addr, portServer, client))
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
			cmd = struct.unpack("<I", dataC[0:4])
			if cmd != URAckSeqNum:
				print("UR:WARN: Peer has sent wrong resp[{}], skipping the same...".format(cmd))
				continue
			data = dataC[4:]
			ur_send_packets(data)
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
	print("Remaining clients:")
	for r in clients:
		print(r)


presence_info()
unicast_recovery()
exit()


prevPktid=0
prevTime=0.0
curTime=0.0
prevTimeThrottle=0.0
while True:
	if (fData != None):
		curData = fData.read(dataSize)
		if (curData == b''):
			break
	else:
		curData = bytes(dataSize)
	data=struct.pack("<Is", pktid, curData)
	sock.sendto(data, (addr, port))
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
	if ((pktid%(N*N*10)) == 0):
		curTime=time.time()
		numPkts = pktid - prevPktid
		timeDelta = curTime - prevTime
		nwSpeed= ((numPkts*dataSize)/timeDelta)/1e6
		print("Transfer speed [{}]MBps\n".format(nwSpeed))
		#time.sleep(1)
		prevTime = time.time()
		prevPktid = pktid


print("INFO: Done with transfer")

