#!/bin/bash
# Helper script for hkvc-nw-send-mcast based nw testing
# HanishKVC
#

PRGPATH="SimpNwMon02_v20190331IST0043-bin-srv/srv"

if [ -d $PRGPATH ]; then
  echo "Ok [$PRGPATH]";
else
  echo "Missing [$PRGPATH]";
  exit
fi
$PRGPATH/hkvc-nw-send-mcast.py --maddr 238.188.188.188 --Bps 400000 --dim 1 --testblocks 2000000 > /dev/null &
echo "thePID [$!]"
sFilePrev=""
sFile=""
while /bin/true; do
	sTime=`date +%Y%m%d%Z%H%M`
	echo $sTime
	sFilePrev=$sFile
	sFile="snm02.srvr.status.$sTime.log"
	cp -a /tmp/snm02.srvr.status.log $sFile
	if [ "$sFilePrev" != "" ]; then
		rm $sFilePrev
	fi
	sleep 2m
done
