#!/usr/bin/env python3
#
# snm-status-analyse : Verifies how many times a given ip/device has responded to PI request from server
# HanishKVC
#

import sys
import devs2check



def logfile_parse(fFile):
	dDevs = {}
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
			sNameInD = dDevs[sIP][0]
			iLPInD = dDevs[sIP][2]
			if (sNameInD != sName):
				print("WARN:NameChange: for IP[{}] from [{}] to [{}]".format(sIP, sNameInD, sName))
			if (iLPInD < iLP):
				print("WARN:LPIncrease: for IP[{}] from [{}] to [{}]".format(sIP, iLPInD, iLP))
			dDevs[sIP] = [ sName, dDevs[sIP][1]+1, iLP ]
		except:
			dDevs[sIP] = [ sName, 0, iLP ]
			print("INFO:NewDevice: IP[{}] [{}]".format(sIP, dDevs[sIP]))
	return dDevs



def devname_cleanup1(dDevs):
	for k in dDevs:
		sName = dDevs[k][0]
		for i in range(len(sName)):
			print(i)
			if (sName[i] == 0):
				sName = sName[:i]
				print(i, sName)
				break
		dDevs[k][0] = sName
	return dDevs



def devname_cleanup(dDevs):
	for k in dDevs:
		sName = dDevs[k][0]
		sName = sName.split('\\x')[0]
		sName = sName.split("'")[1]
		dDevs[k][0] = sName
	return dDevs



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



def devices_check(dDevs):
	for k in dDevs:
		sName = dDevs[k][0]
		try:
			devs2check.devs2check.remove(sName)
		except ValueError:
			print("WARN:devices_check:UnknownDevice:[{}:{}]".format(k, sName))
	print("WARN:devices_check:MissingDevices:[{}]".format(devs2check.devs2check))



def devlist_print(lDevs):
	for l in lDevs:
		#print(l)
		sIP = l[0]
		sName = l[1][0]
		iCnt = l[1][1]
		iLP = l[1][2]
		print("{:15}: {:8} {:12}    {}".format(sIP, iCnt, iLP, sName))



sLogFile = sys.argv[1]
fLogFile = open(sLogFile)
dDevs = logfile_parse(fLogFile)
#print(dDevs)
dDevs = devname_cleanup(dDevs)
lDevs = dict2sortlist(dDevs)
devlist_print(lDevs)
print("NumOfDevices [{}]".format(len(lDevs)))
devices_check(dDevs)

