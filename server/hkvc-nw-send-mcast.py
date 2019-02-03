#!/usr/bin/env python3

#
# hkvc-nw-send-mcast.py
# A test script which sends contents of a file or dummy data over a multicast
# channel, at a predetermined data rate, using a predetermined packet size
PRGVER="v20190204IST0252"
# HanishKVC, GPL, 19XY
#

import os
import sys
import time
import socket
import struct
import select
import random
import signal

import network
import status
import context


PIInitTotalAttempts = 10
PIInitTime4Clients = 10
PIInBtwTotalAttempts = 2
PIInBtwInterval = 15*60
PIInBtwTime4Clients = 30


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


status.open()
status._print("PRGVER:nw-send-mcast:{}".format(PRGVER))

nwGroup=0
pktid=0
iArg=1
portMCast=1111
N=11
dataSize=network.dataSize
Bps=2e6
laddr="0.0.0.0"
laddrms="0.0.0.0"
maddr=network.maddr
sfData=None
iTestBlocks=1e6
bSimLoss=False
bSimLossRandom=True
gsContext = None

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
	elif (sys.argv[iArg] == "--laddrms"):
		iArg += 1
		laddrms = sys.argv[iArg]
	elif (sys.argv[iArg] == "--maddr"):
		iArg += 1
		maddr = sys.argv[iArg]
	elif (sys.argv[iArg] == "--file"):
		iArg += 1
		sfData = sys.argv[iArg]
	elif (sys.argv[iArg] == "--testblocks"):
		iArg += 1
		iTestBlocks = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--simloss"):
		bSimLoss=True
	elif (sys.argv[iArg] == "--startblock"):
		iArg += 1
		pktid = int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--ncid"):
		iArg += 1
		network.CtxtId = int(sys.argv[iArg], 0)
	elif (sys.argv[iArg] == "--ncver"):
		iArg += 1
		network.CtxtVer = int(sys.argv[iArg], 0)
	elif (sys.argv[iArg] == "--context"):
		iArg += 1
		gsContext = sys.argv[iArg]
	iArg += 1

portMCast, portServer, _portClient = network.ports_ngupdate(nwGroup)

if (gsContext != None):
	dprint(9, "INFO:Context: importing from [{}]".format(gsContext))
	gContext = context.open(gsContext)
	tPktid, tTotalBlocksInvolved = context.load_mcast(gContext)
	if (tPktid == -1):
		dprint(9, "WARN: No relavent data in Context file...")
	else:
		pktid, giTotalBlocksInvolved = tPktid, tTotalBlocksInvolved
	context.close(gContext)
else:
	gsContext = "/tmp/snm02.srvr.context.mcast"
	dprint(9, "INFO:Context: None, Will save to [{}]".format(gsContext))

fData=None
if (sfData != None):
	print("MODE:FileTransfer:{}".format(sfData))
	fData = open(sfData, 'br')
	fileSize = os.stat(fData.fileno()).st_size;
	giTotalBlocksInvolved = int(fileSize/dataSize)
	if ((fileSize%dataSize) != 0):
		giTotalBlocksInvolved += 1
	fData.seek(pktid*dataSize)
else:
	print("MODE:TestBlocks:{}".format(iTestBlocks))
	giTotalBlocksInvolved = int(iTestBlocks)

if (bSimLoss):
	print("Simulate losses is Enabled")
else:
	print("Simulate losses is Disabled")

perPktTime=1/(Bps/dataSize)
dprint(9, "laddr [{}]".format(laddr))
print("maddr [{}], portMCast [{}], laddrms[{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(maddr, portMCast, laddrms, N, dataSize, Bps, perPktTime))
print("TotalBlocksToTransfer [{}], StartingBlock [{}]\n".format(giTotalBlocksInvolved, pktid))
if (giTotalBlocksInvolved > (dataSize*2e9)):
	print("ERROR: Too large a content size, not supported...")
	exit(-2)

sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(laddrms))
dprint(9, "Listening on [{}:{}]".format(laddr, portServer))
sock.bind((laddr, portServer))

print("Will start in 10 secs...")
time.sleep(10)
print("Started")



def print_throughput(prevTime, pktid, prevPktid):
	curTime=time.time()
	numPkts = pktid - prevPktid
	timeDelta = curTime - prevTime
	nwSpeed= ((numPkts*dataSize)/timeDelta)/1e6
	dprint(8, "Transfer speed [{}]MBps, pktid [{}]".format(nwSpeed, pktid))
	status.mcast_tx(pktid, giTotalBlocksInvolved)
	return curTime, pktid



def simloss_random():
	iSimLossMod = random.randint(5000, 15000)
	iSimLossRange = random.randint(1,10)
	return iSimLossMod, iSimLossRange



def save_context(sContext, pktid, iTotalBlocks):
	tContext = context.open(sContext, "w+")
	context.save_mcast(tContext, pktid, iTotalBlocks)
	context.close(tContext)
	dprint(9, "INFO: Saved context to [{}]...".format(sContext))



def handle_sigint(sigNum, sigStack):
	save_context(gsContext, pktid, giTotalBlocksInvolved)
	dprint(9, "INFO:sigint: quiting...".format(gsContext))
	exit(10)



signal.signal(signal.SIGINT, handle_sigint)
gClients = []
network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInitTotalAttempts, PIInitTime4Clients)
if (bSimLossRandom):
	iSimLossMod, iSimLossRange = simloss_random()
else:
	iSimLossMod = 10023
	iSimLossRange = 2
prevPktid=pktid
prevTime=time.time()
curTime=0.0
prevPITime=0.0
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
	if (bSimLoss):
		iRem = pktid%iSimLossMod
		if ((pktid > 100) and (iRem < iSimLossRange)):
			print("INFO: Dropping [{}]".format(pktid))
			pktid += 1
			if (bSimLossRandom and ((iRem+1) == iSimLossRange)):
				iSimLossMod, iSimLossRange = simloss_random()
			continue
	data=struct.pack("<IIII{}s".format(dataSize), pktid, network.CtxtId, network.CtxtVer, giTotalBlocksInvolved, curData)
	sock.sendto(data, (maddr, portMCast))
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
	if ((pktid%(N*N*256)) == 0):
		prevTime, prevPktid = print_throughput(prevTime, pktid, prevPktid)
		deltaPITime = curTime-prevPITime
		if (deltaPITime > PIInBtwInterval):
			savedFlag = network.CurFlag
			network.CurFlag = network.FV_FLAG_SAVECLNTCTXT
			network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInBtwTotalAttempts, PIInBtwTime4Clients)
			network.CurFlag = savedFlag
			prevPITime = curTime

print_throughput(prevTime, pktid, prevPktid)
network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInBtwTotalAttempts, PIInBtwTime4Clients)

save_context(gsContext, pktid, giTotalBlocksInvolved)
print("INFO: Done with mcast transfer, quiting...")

