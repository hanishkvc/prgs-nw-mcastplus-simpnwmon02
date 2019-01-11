

# Test that server identifies the total number of blocks involved wrt file transfers properly
dd if=/dev/zero of=/tmp/test.9999.10 bs=10 count=1
dd if=/dev/zero of=/tmp/test.9999.1034 bs=1034 count=1
ll /tmp/
echo "**** **** **** **** TotalBlocks to Transfer should be printed as 1"
../server/hkvc-nw-send-mcast.py --file /tmp/test.9999.10
echo "**** **** **** **** TotalBlocks to Transfer should be printed as 2"
../server/hkvc-nw-send-mcast.py --file /tmp/test.9999.1034

dd if=/dev/zero of=/tmp/test.9999.1024 bs=1024 count=1

dd if=/dev/zero of=/tmp/test.9999.5G bs=1024 seek=5242879 count=1
dd if=/dev/zero of=/tmp/test.9999.10G bs=1024 seek=10485759 count=1

echo "**** **** **** **** TotalBlocks to Transfer should be printed as 2"
../server/hkvc-nw-send-mcast.py --file /tmp/test.9999.1034
echo "**** **** **** **** TotalBlocks to Transfer should be printed as 2"
../server/hkvc-nw-recover.py --file /tmp/test.9999.1034

echo "**** **** **** **** TotalBlocks to Transfer should be printed as 5242880"
../server/hkvc-nw-send-mcast.py --file /tmp/test.9999.5G
echo "**** **** **** **** TotalBlocks to Transfer should be printed as 5242880"
../server/hkvc-nw-recover.py --file /tmp/test.9999.5G

../server/hkvc-nw-recover.py --file /tmp/test.9999.5G --context ./t.context
