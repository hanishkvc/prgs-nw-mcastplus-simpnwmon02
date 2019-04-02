#!/usr/bin/env python3

#
# hkvc-nw-recover.py
# A test script which collates all available active clients in the network
# which talk the same language. In turn it tries to help recover lost packets
# wrt each of the client using unicast tranfers.
PRGVER="v20190303IST0709"
# HanishKVC, GPL, 19XY
#

import os
import sys
import time
import socket
import struct

import context
import network
import status
import ucast_recov


portMCast = 1111
portServer = 1112
portClient = 1113

PITotalAttempts = 10
giNumOfURAttempts = URATTEMPTS_MIN
iTestBlocks=1e6


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


def guess_numofurattempts(totalBlocksInvolved):
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
status._print("PRGVER:nw-recover:{}".format(PRGVER))

nwGroup=0
N=11
dataSize=network.dataSize
Bps=2e6
laddr="0.0.0.0"
laddrms="0.0.0.0"
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
	elif (sys.argv[iArg] == "--ncid"):
		iArg += 1
		network.CtxtId = int(sys.argv[iArg], 0)
	elif (sys.argv[iArg] == "--ncver"):
		iArg += 1
		network.CtxtVer = int(sys.argv[iArg], 0)
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

giNumOfURAttempts = guess_numofurattempts(giTotalBlocksInvolved)

perPktTime=1/(Bps/dataSize)
dprint(9, "laddr [{}]".format(laddr))
dprint(9, "maddr [{}], portMCast [{}], laddrms [{}]\n sqmat-dim [{}]\n dataSize [{}]\n Bps [{}], perPktTime [{}]\n".format(maddr, portMCast, laddrms, N, dataSize, Bps, perPktTime))

if (sPIMode == "SLOW"):
	network.PITime4Clients=10*60
dprint(9, " portServer [{}], portClient [{}]".format(portServer, portClient))
dprint(9, " sPIMode=[{}], PITime4Clients=[{}]\n".format(sPIMode, network.PITime4Clients))

dprint(9, "giTotalBlocksInvolved [{}], URCLIENT_MAXCHANCES_PERATTEMPT [{}] giNumOfURAttempts [{}]\n".format(giTotalBlocksInvolved, URCLIENT_MAXCHANCES_PERATTEMPT, giNumOfURAttempts))

socket.setdefaulttimeout(1)
sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ttl_bin = struct.pack('@i', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl_bin)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(laddrms))
dprint(9, "Listening on [{}:{}]".format(laddr, portServer))
sock.bind((laddr, portServer))

dprint(9, "Will start in 10 secs...")
time.sleep(10)
print("Started")

ucast_recov.init(fData, sock, laddr, portServer, portClient, giTotalBlocksInvolved, dataSize, N)
dprint(9, "PresenceInfo: Listening on [{}:{}]".format(laddr, portServer))
network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PITotalAttempts)
for i in range(giNumOfURAttempts):
	dprint(9, "INFO: UCastRecovery Global Attempt [{} of {}]".format(i, giNumOfURAttempts))
	gClients = ucast_recov.unicast_recovery(gClients)
	if(len(gClients) == 0):
		break

if (len(gClients) == 0):
	dprint(9, "INFO: Done with transfer")
else:
	dprint(9, "INFO: Giving up, clients still with lost packets [{}]".format(gClients))

