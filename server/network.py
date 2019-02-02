
#
# network.py - A module to support network related logics
# v20190201IST2345
# HanishKVC, GPL, 19XY
#

import socket
import struct
import time

import status


CtxtId=0x5a5aa5a5
maddr="230.0.0.1"
portMul = 5
portMCast = 1111
portServer = 1112
portClient = 1113

PITotalTimeSecs = 2*60
PIReqSeqNum = 0xffffff00
PIAckSeqNum = 0xffffff01
MCASTSTOPSeqNum = 0xffffffff
MCASTSTOPAdditionalCheck = 0xf5a55a5f

dataSize=1024


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
		data=struct.pack("<IIII5s", PIReqSeqNum, CtxtId, totalBlocksInvolved, totalBlocksInvolved, bytes("PIReq", 'utf-8'))
		sock.sendto(data, (addr, port))
		time.sleep(1)


def _presence_info(sock, clients, clientsDB):
	sock.settimeout(10.0)
	dprint(9, "PresenceInfo: Listening on [{}:{}]".format(laddr, portServer))
	startTime = time.time()
	deltaTime = 0
	iCnt = 0
	while(deltaTime < PITotalTimeSecs):
		dprint(8, "PI:{}_{}".format(iCnt,deltaTime))
		try:
			d = sock.recvfrom(128)
			dataC = d[0]
			peer = d[1][0]
			try:
				(tCmd, tName, tLostPkts) = struct.unpack("<I16sI8s", dataC)[0:3]
				i = clients.index(peer)
				if (tCmd == PIAckSeqNum):
					dprint(9, "Rcvd from known client:{}:{}".format(peer, dataC))
					clientsDB[peer]['cnt'] += 1
					clientsDB[peer]['lostpkts'] = tLostPkts
					clientsDB[peer]['name'] = tName
			except ValueError as e:
				dprint(9, "Rcvd from new client:{}:{}".format(peer, dataC))
				clients.append(peer)
				clientsDB[peer] = {'type':'new', 'cnt': 1, 'lostpkts': tLostPkts, 'name': tName}
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
	for r in clients:
		dprint(9, r)
	return iSilentClients


def presence_info(sock, maddr, portMCast, totalBlocksInvolved, clients):
	clientsDB = {}
	for r in clients:
		clientsDB[r] = {'type':'known', 'cnt': 0, 'lostpkts': -1, 'name':'UNKNOWN'}
	for i in range(10):
		send_pireq(sock, maddr, portMCast, totalBlocksInvolved, i, 1)
		iSilentClients = _presence_info(sock, clients, clientsDB)
		status.ucast_pi(clientsDB)
		if (iSilentClients == 0):
			dprint(9, "INFO:PI: handshaked with all known clients")
			return
		else:
			dprint(9, "WARN:PI: [{}] known clients didnt talk, trying again [{}]...".format(iSilentClients, i))

