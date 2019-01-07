#!/usr/bin/env python3

#
# hkvc-check-image.py
# A test script which checks if the specified image matchs the test data sent
# by the network test scripts
# The test data/file is expected to contain a series of blocks of size
# dataSize, where each block has a marker at the begining of the block
# equivalent to the block id/num.
# v20190107IST0600
# HanishKVC, GPL, 19XY
#

import os
import sys
import struct

dataSize = 1024

f = open(sys.argv[1], 'rb')
iPrev = -1
iRef = 0
while True:
	d = f.read(dataSize)
	if (d == b''):
		print("I: EOF");
		break
	iCur = struct.unpack('<I',d[0:4])[0]
	iDelta = iCur - iPrev
	if (iCur != iRef):
		print("E: C[{}], R[{}] d[{}]".format(iCur, iRef, iDelta))
	elif (iDelta != 1):
		print("W: C[{}], R[{}] d[{}]".format(iCur, iRef, iDelta))
	if (iRef % (512*1024)) == 0:
		print("I: {}".format(iCur))
	iPrev = iRef
	iRef += 1

