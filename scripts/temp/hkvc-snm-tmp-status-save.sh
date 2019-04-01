#!/usr/bin/env bash

while /bin/true; do
sDate=`date +%Y%m%d%Z%H%M`
echo $sDate
cp /tmp/snm02*status*log snm02.srv.status.log-$sDate
sleep 5m
done
