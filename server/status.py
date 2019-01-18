
#
# A module which consolidates all exported status info
# v20190118IST1010
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
	_print("MODE:MCAST_TX")
	_print("{}/{}".format(iCurBlock, iTotalBlocks))
	_after()


def mcast_stop(cur, totalTime):
	_before()
	_print("MODE:MCAST_STOP")
	_print("{}/{}".format(cur, totalTime))
	_after()


def ucast_pi(clientsDB):
	_before()
	_print("MODE:UCAST_PI")
	for r in clientsDB:
		_print("IP={}:LP={}:C={}".format(r, clientsDB[r]['LostPkts'], clientsDB[r]['Cnt']))
	_after()


def ucast_ur(clientsDB):
	_before()
	_print("MODE:UCAST_UR")
	for r in clientsDB:
		_print("IP={}:LP={}".format(r, clientsDB[r]))
	_after()

