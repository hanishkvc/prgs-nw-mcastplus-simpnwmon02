
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

PIReqSeqNum = 0xffffff00
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

