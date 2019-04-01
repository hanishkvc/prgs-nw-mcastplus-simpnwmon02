#!/usr/bin/env python3

import sys

sFile = sys.argv[1]
fFile = open(sFile)

dC = {}

for l in fFile:
	l = l.strip()
	if (not l.startswith("UCAST_PI")):
		continue
	la = l.split(':')
	dC[la[1].split('=')[1]] = int(la[2].split('=')[1])
print(dC)

lpMin = 1e6
lpMax = -1e6
lpAvg = 0
cnt = 0
for cC in dC:
	if (dC[cC] == -1):
		continue
	cnt += 1
	if (lpMin > dC[cC]):
		lpMin = dC[cC]
	if (lpMax < dC[cC]):
		lpMax = dC[cC]
	lpAvg += dC[cC]
lpAvg = lpAvg/cnt


def dict2sortlist(dC):
	lC = []
	for cC in dC:
		i = 0
		for c in lC:
			if (c['lp'] > dC[cC]):
				break
			i += 1
		lC.insert(i, {'ip': cC, 'lp' : dC[cC]})

	for c in lC:
		print(c)
	print(len(lC))
	return lC

lC = dict2sortlist(dC)
lpThres = lpAvg + (lpMax-lpAvg)/2
for cC in dC:
	if (dC[cC] > lpThres):
		print("{}={}".format(cC, dC[cC]))
print("lpAvg = {}".format(lpAvg))

