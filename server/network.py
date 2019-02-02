
#
# network.py - A module to support network related logics
# v20190202IST2336
# HanishKVC, GPL, 19XY
#

import socket
import struct
import time
import enum

import status


CtxtId=0x5a5aa5a5
CtxtVer=0x01
maddr="230.0.0.1"
portMul = 5
portMCast = 1111
portServer = 1112
portClient = 1113

PIDefaultAttempts=2
PITime4Clients = 30
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


def send_pireq(sock, addr, port, totalBlocksInvolved, piSeqId, times=1):
	for i in range(times):
		if (i%10) == 0:
			status.pireq(addr, piSeqId, i, times)
			print("INFO: PIReq Num[{}:{}] sending".format(piSeqId, i))
		data=struct.pack("<IIII5s", PIReqSeqNum, CtxtId, CtxtVer, totalBlocksInvolved, bytes("PIReq", 'utf-8'))
		sock.sendto(data, (addr, port))
		time.sleep(1)


def _presence_info(sock, clients, clientsDB, time4Clients):
	sock.settimeout(10.0)
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < time4Clients):
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
			dprint(9, "Rcvd from client:{}:{}".format(peer, dataC))
		except socket.timeout as e:
			d = None
			dprint(7, ".")
		curTime = time.time()
		deltaTime = int(curTime - startTime)
		iCnt += 1
	dprint(9, "PI:END: Status:")
	iSilentClients = 0
	for r in clientsDB:
		dprint(9, "{} = {}".format(r, clientsDB[r]))
		if (clientsDB[r]['cnt'] == 0):
			iSilentClients += 1
	dprint(9, "PI:END: Clients list")
	return iSilentClients


PIModes=enum.Enum("PIModes", "BLIND, KNOWN")
def presence_info(sock, maddr, portMCast, totalBlocksInvolved, clients, attempts=PIDefaultAttempts, time4Clients=PITime4Clients):
	clientsDB = {}
	if (len(clients) == 0):
		mode = PIModes.BLIND
	else:
		mode = PIModes.KNOWN
	for r in clients:
		clientsDB[r] = {'type':'known', 'cnt': 0, 'lostpkts': -1, 'name':'UNKNOWN'}
	for i in range(attempts):
		dprint(9, "INFO:PI: Attempt [{}/{}], time4Clients[{}]...".format(i, attempts, time4Clients))
		send_pireq(sock, maddr, portMCast, totalBlocksInvolved, i, 1)
		iSilentClients = _presence_info(sock, clients, clientsDB, time4Clients)
		status.ucast_pi(clientsDB)
		for r in clients:
			dprint(9, r)
		if (mode == PIModes.KNOWN):
			if (iSilentClients == 0):
				dprint(9, "INFO:PI: handshaked with all known clients")
				return
			else:
				dprint(9, "WARN:PI: [{}] known clients didnt talk...".format(iSilentClients))

