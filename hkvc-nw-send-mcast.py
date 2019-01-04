#!/usr/bin/env python3

#
# hkvc-nw-send-mcast.py
# A test script which sends contents of a file or dummy data over a multicast
# channel, at a predetermined data rate, using a predetermined packet size
# v20181228IST0447
# HanishKVC, GPL, 19XY
#

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
iTestBlocks=1e6

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
	elif (sys.argv[iArg] == "--testblocks"):
		iArg += 1
		iTestBlocks = int(sys.argv[iArg])
	iArg += 1

fData=None
if (sfData != None):
	print("MODE:FileTransfer:{}".format(sfData))
	fData = open(sfData, 'br')
else:
	print("MODE:TestBlocks:{}".format(iTestBlocks))

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
		if (pktid >= iTestBlocks):
			break
		tmpData = bytes(dataSize-4)
		curData = struct.pack("<I{}s".format(dataSize-4), pktid, tmpData)
		#print(curData, len(curData))
	data=struct.pack("<I{}s".format(dataSize), pktid, curData)
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

for i in range(120):
	tmpData = bytes(dataSize-4)
	curData = struct.pack("<I{}s".format(dataSize-4), 0xf5a55a5f, tmpData)
	data=struct.pack("<I{}s".format(dataSize), 0xffffffff, curData)
	sock.sendto(data, (addr, port))
	time.sleep(1)

print("INFO: Done sending stops, quiting...")

