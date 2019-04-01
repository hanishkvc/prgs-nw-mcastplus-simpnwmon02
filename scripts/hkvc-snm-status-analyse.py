#!/usr/bin/env python3
#
# snm-status-analyse : Verifies how many times a given ip/device has responded to PI request from server
# HanishKVC
#

import sys
import devs2check

sInFile = sys.argv[1]
fInFile = open(sInFile)

def parse_file(fFile):
	dC = {}
	for l in fFile:
		l = l.strip()
		if (not l.startswith("UCAST_PI")):
			continue
		la = l.split(':')
		sIP = la[1].split('=')[1]
		iLP = int(la[2].split('=')[1])
		iCnt = int(la[3].split('=')[1])
		sName = la[4].split('=')[1]
		if (iCnt == 0):
			continue
		try:
			sNameInD = dC[sIP][0]
			iLPInD = dC[sIP][2]
			if (sNameInD != sName):
				print("WARN:NameChange: for IP[{}] from [{}] to [{}]".format(sIP, sNameInD, sName))
			if (iLPInD < iLP):
				print("WARN:LPIncrease: for IP[{}] from [{}] to [{}]".format(sIP, iLPInD, iLP))
			dC[sIP] = [ sName, dC[sIP][1]+1, iLP ]
		except:
			dC[sIP] = [ sName, 0, iLP ]
			print("INFO:NewDevice: IP[{}] [{}]".format(sIP, dC[sIP]))
	return dC

dC = parse_file(fInFile)
#print(dC)

def devname_cleanup1(dC):
	for k in dC:
		sName = dC[k][0]
		for i in range(len(sName)):
			print(i)
			if (sName[i] == 0):
				sName = sName[:i]
				print(i, sName)
				break
		dC[k][0] = sName
	return dC

def devname_cleanup(dC):
	for k in dC:
		sName = dC[k][0]
		sName = sName.split('\\x')[0]
		sName = sName.split("'")[1]
		dC[k][0] = sName
	return dC

dC = devname_cleanup(dC)

def dict2sortlist(dIn):
	lstNew = []
	for k in dIn:
		bDone = False
		for i in range(len(lstNew)):
			if (lstNew[i][1][1] < dIn[k][1]):
				continue
			lstNew.insert(i, [ k, dIn[k] ])
			bDone = True
			break
		if (not bDone):
			lstNew.append([k, dIn[k]])
	return lstNew

def devices_check(dC):
	for k in dC:
		sName = dC[k][0]
		try:
			devs2check.devs2check.remove(sName)
		except ValueError:
			print("WARN:devices_check:UnknownDevice:[{}:{}]".format(k, sName))
	print("WARN:devices_check:MissingDevices:[{}]".format(devs2check.devs2check))

devices_check(dC)

lC = dict2sortlist(dC)
for l in lC:
	#print(l)
	sIP = l[0]
	sName = l[1][0]
	iCnt = l[1][1]
	iLP = l[1][2]
	print("{:15}: {:8} {:12}    {}".format(sIP, iCnt, iLP, sName))

print("NumOfDevices [{}]".format(len(lC)))

