
#
# mcast.py - A module providing network related mcast test/transfer logic
# v20190330IST0037
# HanishKVC, GPL, 19XY
#

import time
import socket
import struct
import select
import random

import network
import status


PIInitTotalAttempts = 10
PIInitTime4Clients = 10
PIInBtwTotalAttempts = 4
PIInBtwInterval = 10*60
PIInBtwTime4Clients = 15



DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)




def print_throughput(prevTime, pktid, prevPktid, iTotalBlocksInvolved, dataSize):
	curTime=time.time()
	numPkts = pktid - prevPktid
	timeDelta = curTime - prevTime
	nwSpeed= ((numPkts*dataSize)/timeDelta)/1e6
	dprint(8, "Transfer speed [{}]MBps, pktid [{}]".format(nwSpeed, pktid))
	status.mcast_tx(pktid, iTotalBlocksInvolved)
	return curTime, pktid



def simloss_random():
	iSimLossMod = random.randint(5000, 15000)
	iSimLossRange = random.randint(1,10)
	return iSimLossMod, iSimLossRange



def status_state(sState):
	t=time.gmtime()
	sTime="{:04}{:02}{:02}{:02}{:02}{:02}".format(t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)
	status._print("SNM02:NSM:STATE:{}:{}".format(sState, sTime))



def mcast(sock, fData, maddr, portMCast, giTotalBlocksInvolved, pktid, dataSize, perPktTime, N, bSimLoss, bSimLossRandom):
	status_state("Start-FindClients")
	gClients = []
	network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInitTotalAttempts, PIInitTime4Clients, pktid)
	if (bSimLossRandom):
		iSimLossMod, iSimLossRange = simloss_random()
	else:
		iSimLossMod = 10023
		iSimLossRange = 2
	prevPktid=pktid
	prevTime=time.time()
	curTime=0.0
	prevPITime=prevTime
	prevTimeThrottle=time.time()
	while True:
		if (fData != None):
			curData = fData.read(dataSize)
			if (curData == b''):
				break
		else:
			if (pktid >= giTotalBlocksInvolved):
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
				(rlist, wlist, xlist) = select.select([],[],[],timeRemaining)
			prevTimeThrottle = time.time()
		if ((pktid%(N*N*256)) == 0):
			prevTime, prevPktid = print_throughput(prevTime, pktid, prevPktid, giTotalBlocksInvolved, dataSize)
			deltaPITime = curTime-prevPITime
			if (deltaPITime > PIInBtwInterval):
				status_state("Btw-GetStatus")
				savedFlag = network.CurFlag
				network.CurFlag = network.FV_FLAG_SAVECLNTCTXT
				network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInBtwTotalAttempts, PIInBtwTime4Clients, pktid)
				network.CurFlag = savedFlag
				prevPITime = curTime

	print_throughput(prevTime, pktid, prevPktid, giTotalBlocksInvolved, dataSize)
	status_state("End-GetStatus")
	network.presence_info(sock, maddr, portMCast, giTotalBlocksInvolved, gClients, PIInBtwTotalAttempts, PIInBtwTime4Clients)

