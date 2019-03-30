#!/usr/bin/env python3

#
# hkvc-nw-send-mcast.py
# A test script which sends contents of a file or dummy data over a multicast
# channel, at a predetermined data rate, using a predetermined packet size
PRGVER="v20190331IST0042"
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
import mcast


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


status.open()
status._print("PRGVER:SNM02:nw-send-mcast:{}".format(PRGVER))

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

status._print("SNM02:NSM:RUNCFG1:MAddr[{}]:PortMCast[{}]:LAddrMS[{}]:NwGroup[{}]:NCid[{}]:NCver[{}]".format(maddr, portMCast, laddrms, nwGroup, network.CtxtId, network.CtxtVer))
status._print("SNM02:NSM:RUNCFG2:Dim[{}]:DataSize[{}]:Bps[{}]".format(N, dataSize, Bps))
print("Will start in 10 secs...")
time.sleep(10)
print("Started")



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

mcast.mcast(sock, fData, maddr, portMCast, giTotalBlocksInvolved, pktid, dataSize, perPktTime, N, bSimLoss, bSimLossRandom)

save_context(gsContext, pktid, giTotalBlocksInvolved)
print("INFO: Done with mcast transfer, quiting...")

