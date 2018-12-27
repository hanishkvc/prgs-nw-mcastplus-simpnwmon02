#!/usr/bin/env python3

import os
import sys
import time
import socket
import struct
import select


sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)

pktid=0
iArg=1
port=1111
N=11
dataSize=1024
Bps=2e6
addr="127.0.0.1"
sfData=None

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
print("Will start in 10 secs...")
time.sleep(10)

prevPktid=0
prevTime=time.time()
curTime=0.0
prevTimeThrottle=time.time()
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

