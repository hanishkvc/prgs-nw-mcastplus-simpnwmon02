
#
# A module which consolidates all exported status info
# v20190130IST2335
# HanishKVC, 19XY
#

import io


sStatusFile = "/tmp/snm02.srvr.status.log"
statusFile = None
PRINT_LVL = 10


def open(sFile=sStatusFile):
	global statusFile
	statusFile = io.open(sFile, "wt")
	return statusFile


def _print(msg, lvl=0):
	if (lvl < PRINT_LVL):
		print(msg, file=statusFile)


def _before():
	_print("\n**START**\n", 50)


def _after():
	_print("\n**END**\n", 50)
	statusFile.flush()


def mcast_tx(iCurBlock, iTotalBlocks):
	_before()
	_print("MCAST_TX:{}/{}".format(iCurBlock, iTotalBlocks))
	_after()


def mcast_stop(cur, totalTime):
	_before()
	_print("MCAST_STOP:{}/{}".format(cur, totalTime))
	_after()


def pireq(addr, attempt, attempts, time4Clients):
	_before()
	_print("PI_REQ:{}:{}/{}:{}".format(addr, attempt+1, attempts, time4Clients))
	_after()


def ucast_pi(clientsDB):
	_before()
	_print("SET:UCAST_PI:ClientCnt:{}".format(len(clientsDB)))
	for r in clientsDB:
		_print("UCAST_PI:IP={}:LP={}:C={}:N={}".format(r, clientsDB[r]['lostpkts'], clientsDB[r]['cnt'], clientsDB[r]['name']))
	_after()


def ucast_ur(client, iCur, iTotal):
	_before()
	_print("UCAST_UR:{}:{}/{}".format(client, iCur, iTotal))
	_after()


def ucast_ur_summary(clientsDB):
	_before()
	_print("SET:UCAST_UR:RemainingClients:{}".format(len(clientsDB)))
	for r in clientsDB:
		_print("UCAST_UR:RC:IP={}:LP={}".format(r, clientsDB[r]))
	_after()

