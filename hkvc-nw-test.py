#!/usr/bin/env python3

import os
import sys
import time
import socket
import struct


sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

pktid=0
iArg=1
port=1111
N=11
dataSize=1024
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
	iArg += 1

print(" port [{}]\n sqmat-dim [{}]\n dataSize [{}]\n".format(port, N, dataSize))

while True:
	data=struct.pack("<Is", pktid, bytes(dataSize))
	sock.sendto(data, ("127.0.0.1", port))
	pktid += 1
	if ((pktid%(N*N)) == 0):
		time.sleep(1)



