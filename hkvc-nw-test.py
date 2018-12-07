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
while iArg < len(sys.argv):
	if (sys.argv[iArg] == "--port"):
		iArg += 1
		port=int(sys.argv[iArg])
	elif (sys.argv[iArg] == "--dim"):
		iArg += 1
		N = int(sys.argv[iArg])
	iArg += 1

print("port [{}]\ndim [{}]\n".format(port, N))

while True:
	data=struct.pack("<Is", pktid, bytes("Hello world",'utf8'))
	sock.sendto(data, ("127.0.0.1", 9999))
	pktid += 1
	if ((pktid%(N*N)) == 0):
		time.sleep(1)



