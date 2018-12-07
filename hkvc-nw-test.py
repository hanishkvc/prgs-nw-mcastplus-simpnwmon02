#!/usr/bin/env python3

import os
import sys
import time
import socket
import struct


sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

pktid=0
N=11
while True:
	data=struct.pack("<Is", pktid, bytes("Hello world",'utf8'))
	sock.sendto(data, ("127.0.0.1", 4531))
	pktid += 1
	if ((pktid%(N*N)) == 0):
		time.sleep(1)



