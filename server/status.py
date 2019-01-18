
#
# A module which consolidates all exported status info
# v20190118IST0816
# HanishKVC, 19XY
#

import io


sStatusFile = "/tmp/snm02.srvr.status.log"
statusFile = None


def open(sFile=sStatusFile):
	global statusFile
	statusFile = io.open(sFile, "wt")
	return statusFile


def ucast_pi(clientsDB):
	print("MODE:UCAST_PI", file=statusFile)
	for r in clientsDB:
		print("IP={}:LP={}:C={}".format(r, clientsDB[r]['LostPkts'], clientsDB[r]['Cnt']), file=statusFile)


def ucast_ur(clientsDB):
	print("MODE:UCAST_UR", file=statusFile)

