
#
# network.py - A module to support network related logics
# v20190203IST1654
# HanishKVC, GPL, 19XY
#

import socket
import struct
import time
import enum

import status


FV_FLAG_NONE         =0x00000000
FV_FLAG_SAVECLNTCTXT =0x80000000
FV_FLAGMASK          =0xFF000000
FV_CTXTVERMASK       =0x00FFFFFF

CurFlag=FV_FLAG_NONE
CtxtId=0x5a5aa5a5
CtxtVer=0x000001
maddr="230.0.0.1"
portMul = 5
portMCast = 1111
portServer = 1112
portClient = 1113

PIDefaultAttempts=4
PITime4Clients = 15
PIReqSeqNum = 0xffffff00
PIAckSeqNum = 0xffffff01
MCASTSTOPSeqNum = 0xffffffff
MCASTSTOPAdditionalCheck = 0xf5a55a5f

dataSize=1024


DBGLVL = 7
def dprint(lvl, msg):
	if (lvl > DBGLVL):
		print(msg)


def ports_ngupdate(nwGroup):
	tPortMCast = portMCast + portMul*nwGroup
	tPortServer = portServer + portMul*nwGroup
	tPortClient = portClient + portMul*nwGroup
	return tPortMCast, tPortServer, tPortClient


def mkfv(ctxtVer):
	return ((CurFlag & FV_FLAGMASK) | (ctxtVer & FV_CTXTVERMASK))


def send_pireq(sock, addr, port, totalBlocksInvolved, piSeqId, times=1):
	for i in range(times):
		if (i%10) == 0:
			print("INFO: PIReq Num[{}:{}] sending".format(piSeqId, i))
		data=struct.pack("<IIII5s", PIReqSeqNum, CtxtId, mkfv(CtxtVer), totalBlocksInvolved, bytes("PIReq", 'utf-8'))
		sock.sendto(data, (addr, port))
		time.sleep(1)


def _pi_silentclients(clientsDB, msgLvl):
	iSilentClients = 0
	for r in clientsDB:
		dprint(msgLvl, "{} = {}".format(r, clientsDB[r]))
		if (clientsDB[r]['cnt'] == 0):
			iSilentClients += 1
	return iSilentClients


def _presence_info(sock, clients, clientsDB, time4Clients, mode):
	sock.settimeout(5.0)
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < time4Clients):
		if (time4Clients > 10):
			dprint(8, "PI:{}_{}".format(iCnt,deltaTime))
		try:
			d = sock.recvfrom(128)
			dataC = d[0]
			peer = d[1][0]
			try:
				(tCmd, tName, tLostPkts) = struct.unpack("<I16sI8s", dataC)[0:3]
				i = clients.index(peer)
				if (tCmd == PIAckSeqNum):
					dprint(5, "Rcvd from known client:{}:{}".format(peer, dataC))
					clientsDB[peer]['cnt'] += 1
					clientsDB[peer]['lostpkts'] = tLostPkts
					clientsDB[peer]['name'] = tName
			except ValueError as e:
				dprint(5, "Rcvd from new client:{}:{}".format(peer, dataC))
				clients.append(peer)
				clientsDB[peer] = {'type':'new', 'cnt': 1, 'lostpkts': tLostPkts, 'name': tName}
			if (time4Clients > 10):
				dprint(8, "Rcvd from client:{}:{}".format(peer, clientsDB[peer]))
		except socket.timeout as e:
			d = None
			dprint(7, ".")
			iSilentClients = _pi_silentclients(clientsDB, 7)
			if (mode == PIModes.KNOWN):
				if (iSilentClients == 0):
					dprint(7, "INFO:_PI: handshaked with all known clients")
					break
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
	dprint(9, "PI:END: Status:")
	iSilentClients = _pi_silentclients(clientsDB, 9)
	return iSilentClients


def p100(val, valMax, msgLvl=7):
	if (valMax == 0):
		valMax = 1
		dprint(msgLvl, "INFO:P100: valMax is 0, so reset to 1")
	return round((val/valMax)*100, 2)


def _pi_statuslog(clients, clientsDB, iSilentClients, totalBlocksInvolved, curBlocksSent):
	lpMin = 1e9
	lpMax = -1e9
	lpTotal = 0
	dprint(7, "PI: Clients list")
	gotBkts = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
	gotBktsR = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
	iOddGots = 0
	for r in clients:
		lpCur = clientsDB[r]['lostpkts']
		gotCur = totalBlocksInvolved-lpCur
		index = int((gotCur/totalBlocksInvolved)*10)
		indexR = round((gotCur/totalBlocksInvolved)*10)
		try:
			gotBkts[index] += 1
			gotBktsR[indexR] += 1
		except IndexError:
			iOddGots += 1
		if (lpMin > lpCur):
			lpMin = lpCur
		if (lpMax < lpCur):
			lpMax = lpCur
		lpTotal += lpCur
		dprint(7, r)
	numClients = len(clients)
	if (numClients > 0):
		lpAvg = lpTotal/numClients
		tbi = totalBlocksInvolved
		cbs = curBlocksSent
		dprint(9, "INFO:PI:CurSummary: iSilent={}/{}: sent={}/{}: lpMin={}:lpAvg={}:lpMax={}:lpTotal={}".format(iSilentClients, numClients, cbs, tbi, lpMin, lpAvg, lpMax, lpTotal))
		dprint(9, "INFO:PI:CurSummary:Relative2Total[%]: sent={}: lpMin={}:lpAvg={}:lpMax={}:lpTotal={}".format(p100(cbs,tbi), p100(lpMin,tbi), p100(lpAvg,tbi), p100(lpMax,tbi), p100(lpTotal,tbi)))
		#if (cbs != 0) and (cbs != tbi):
		dprint(9, "INFO:PI:CurSummary:RcvdRelatv2CurSent[%]: lpMin={}:lpAvg={}:lpMax={}:lpTotal={}".format(p100(tbi-lpMin,cbs), p100(tbi-lpAvg,cbs), p100(tbi-lpMax,cbs), p100(lpTotal,cbs)))
		status._print("PI_SUM:S={}:lm={}:la={}:lM={}".format(p100(cbs,tbi), p100(lpMin,tbi), p100(lpAvg,tbi), p100(lpMax,tbi)))
	gB=gotBkts
	sGB="PI_SUM:I:00={:03} 10={:03} 20={:03} 30={:03} 40={:03} 50={:03} 60={:03} 70={:03} 80={:03} 90={:03} 100={:03} Odd={:03}".format(gB[0], gB[1], gB[2], gB[3], gB[4], gB[5], gB[6], gB[7], gB[8], gB[9], gB[10], iOddGots)
	gB=gotBktsR
	sGBR="PI_SUM:R:00={:03} 10={:03} 20={:03} 30={:03} 40={:03} 50={:03} 60={:03} 70={:03} 80={:03} 90={:03} 100={:03} Odd={:03}".format(gB[0], gB[1], gB[2], gB[3], gB[4], gB[5], gB[6], gB[7], gB[8], gB[9], gB[10], iOddGots)
	status._print(sGB)
	status._print(sGBR)
	dprint(9, "INFO:PI:CurSummary:I:{}".format(sGB))
	dprint(9, "INFO:PI:CurSummary:R:{}".format(sGBR))


PIModes=enum.Enum("PIModes", "BLIND, KNOWN")
def presence_info(sock, maddr, portMCast, totalBlocksInvolved, clients, attempts=PIDefaultAttempts, time4Clients=PITime4Clients, curBlocksSent=None):
	if (curBlocksSent == None):
		curBlocksSent = totalBlocksInvolved
	clientsDB = {}
	if (len(clients) == 0):
		mode = PIModes.BLIND
	else:
		mode = PIModes.KNOWN
	for r in clients:
		clientsDB[r] = {'type':'known', 'cnt': 0, 'lostpkts': -1, 'name':'UNKNOWN'}
	for i in range(attempts):
		dprint(9, "INFO:PI: Attempt [{}/{}], time4Clients[{}]...".format(i+1, attempts, time4Clients))
		send_pireq(sock, maddr, portMCast, totalBlocksInvolved, i, 1)
		status.pireq(maddr, i, attempts, time4Clients)
		iSilentClients = _presence_info(sock, clients, clientsDB, time4Clients, mode)
		status.ucast_pi(clientsDB, iSilentClients)

		_pi_statuslog(clients, clientsDB, iSilentClients, totalBlocksInvolved, curBlocksSent)

		if (mode == PIModes.KNOWN):
			if (iSilentClients == 0):
				dprint(9, "INFO:PI: handshaked with all known clients")
				return
			else:
				dprint(9, "WARN:PI: [{}] known clients didnt talk...".format(iSilentClients))

