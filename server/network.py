
#
# network.py - A module to support network related logics
# v20190111IST2144
# HanishKVC, GPL, 19XY
#

import socket
import struct
import time


maddr="230.0.0.1"
mcast_port=1111

MCASTSTOPSeqNum = 0xffffffff
MCASTSTOPAdditionalCheck = 0xf5a55a5f

dataSize=1024


def mcast_stop(sock, addr, port, totalBlocksInvolved, times=120):
	for i in range(times):
		if (i%10) == 0:
			print("INFO: MCastStop Num[{}] sending".format(i))
		tmpData = bytes(dataSize-8)
		curData = struct.pack("<II{}s".format(dataSize-8), MCASTSTOPAdditionalCheck, totalBlocksInvolved, tmpData)
		data=struct.pack("<I{}s".format(dataSize), MCASTSTOPSeqNum, curData)
		sock.sendto(data, (addr, port))
		time.sleep(1)

