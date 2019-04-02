#!/bin/bash
# Helper script for hkvc-nw-send-mcast based nw testing
# HanishKVC
#

PRG="srv/hkvc-nw-send-mcast.py"

if [ -x $PRG ]; then
  echo "Ok [$PRG]";
else
  echo "Missing [$PRG]";
  exit
fi
$PRG --maddr 238.188.188.188 --Bps 40000 --dim 1 --testblocks 1000000 > /dev/null &
thePID=$!
echo "thePID [$thePID]"
sFilePrev=""
sFile=""
while /bin/true; do
	sTime=`date +%Y%m%d%Z%H%M`
	sFilePrev=$sFile
	sFile="snm02.srvr.status.$sTime.log"
	if [ -d /proc/$thePID ]; then
		echo "alive:$sTime"
		bDone=false
	else
		echo "Done:$sTime"
		bDone=true
	fi
	cp -a /tmp/snm02.srvr.status.log $sFile
	if [ "$sFilePrev" != "" ]; then
		rm $sFilePrev
	fi
	if $bDone; then
		break
	fi
	sleep 2m
done
