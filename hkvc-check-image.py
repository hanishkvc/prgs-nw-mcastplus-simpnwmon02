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


f = open(sys.argv[1], 'rb')
while True:
	d = f.read(1024)
	if (d == ''):
		break
	i = struct.unpack('<I',d[0:4])
	print(i)

