#!/usr/bin/env python3

import os
import sys
import time
import socket
import struct
import select


sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

pktid=0
iArg=1
port=1111
N=11
dataSize=1024
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
	iArg += 1

Bps=2e6
perPktTime=1/(Bps/dataSize)
print(" port [{}]\n sqmat-dim [{}]\n dataSize [{}]\n perPktTime [{}]\n".format(port, N, dataSize, perPktTime))

prevPktid=0
prevTime=0.0
curTime=0.0
prevTimeThrottle=0.0
while True:
	data=struct.pack("<Is", pktid, bytes(dataSize))
	sock.sendto(data, ("127.0.0.1", port))
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



