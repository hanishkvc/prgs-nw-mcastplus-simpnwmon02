#!/usr/bin/env python3

#
# hkvc-check-image.py
# A test script which checks if the specified image matchs the test data sent
# by the network test scripts
# v20181228IST0419
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
	if (iCur != iRef):
		print("W: C[{}], R[{}]".format(iCur, iRef))
	iDelta = iCur - iPrev
	if (iDelta != 1):
		print("E: B[{}], d[{}]".format(iCur, iDelta))
	if (iRef % (512*1024)) == 0:
		print("I: {}".format(iCur))
	iPrev = iCur
	iRef += 1

