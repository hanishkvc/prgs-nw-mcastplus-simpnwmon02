
function disk_create_base()
{
	qemu-img create -f qcow2 u1604.hda 4G
}

function disk_create_actual()
{
	qemu-img create -f qcow2 -o backing_file=u1604.hda u1604m10.hda
	qemu-img create -f qcow2 -o backing_file=u1604.hda u1604m11.hda
	qemu-img info u1604m10.hda
	qemu-img info u1604m11.hda
}

GENRARG="-m 512 -enable-kvm"

BOOTARG="-boot d"
BOOTARG=
BOOTARG="-boot c"

DISPARG="-vga std"
DISPARG=
DISPARG="-vga virtio"

NETWARG="-net nic,macaddr=00:01:02:03:04:10 -net user"
NETWARG="-net nic -net user"
echo "Remember to add 3333/tcp to enabled ports in your firewall"
NETARG10="-net nic,macaddr=00:01:02:03:04:10 -net socket,listen=:3333"
NETARG11="-net nic,macaddr=00:01:02:03:04:11 -net socket,connect=:3333"
echo "Remember to add 3333/udp to enabled ports in your firewall"
NETARG1="-net nic,macaddr=52:54:00:12:34:56 -net socket,mcast=230.0.0.1:3333"
NETARG2="-net nic,macaddr=52:54:00:12:34:57 -net socket,mcast=230.0.0.1:3333"
NETARG1="-net nic,macaddr=00:01:02:03:04:10 -net socket,mcast=230.0.0.1:3333,localaddr=127.0.0.1"
NETARG2="-net nic,macaddr=00:01:02:03:04:11 -net socket,mcast=230.0.0.1:3333,localaddr=127.0.0.1"


function vm_base_install()
{
	qemu-system-x86_64 $GENRARG $NETWARG -cdrom u1604lts-mini.iso -hda u1604.hda -boot d $DISPARG
	#qemu-system-x86_64 $GENRARG $NETWARG -cdrom u1604lts-mini.iso -hda u1604.hda -boot c $DISPARG
}

function vm_base_update()
{
	qemu-system-x86_64 $GENRARG $NETWARG -hda u1604.hda $DISPARG
}

function vm_childs_run_temp()
{
	qemu-system-x86_64 $GENRARG $NETARG1 -hda u1604m10.hda -snapshot &
	qemu-system-x86_64 $GENRARG $NETARG2 -hda u1604m11.hda -snapshot &
}

function vm_childs_update()
{
	echo "If using tftp to comm with vm, in vm RUN tftp THEN connect 10.0.2.2 THEN bin THEN get file THEN quit"
	echo "If using ssh to comm with vm1, in host RUN ssh test@localhost -p 1110"
	echo "If using ssh to comm with vm2, in host RUN ssh test@localhost -p 1111"
	NETARGCUPD1="$NETWARG,tftp=/tmp/share,hostfwd=tcp::1110-:22"
	NETARGCUPD2="$NETWARG,tftp=/tmp/share,hostfwd=tcp::1111-:22"

	echo "This provides access to host machine (mapped to 10.0.2.2 in host) and internet"
	echo "On 1st run remember to set ens3 to have static ip 10.0.2.10/24 gw 10.0.2.2 /etc/network/interfaces"
	qemu-system-x86_64 $GENRARG $NETARGCUPD1 -hda u1604m10.hda &
	echo "On 1st run remember to set ens3 to have static ip 10.0.2.11/24 gw 10.0.2.2 /etc/network/interfaces"
	qemu-system-x86_64 $GENRARG $NETARGCUPD2 -hda u1604m11.hda &
}

function vm_childs_run()
{
	echo "remember to run sudo mount -t 9p -o trans=virtio host100 /mnt, in the vm after linux boots"
	FSARG="-virtfs local,path=/home/hanishkvc/AndroidStudioProjects,readonly,mount_tag=host100,security_model=mapped-file"
	# As I noticied corruption in my host filesystem, after I used this virtfs logic of qemu, so I am avoiding use of the same now
	# All changes I had done to the shared folder on the host system and inturn accessed on the vm, got magically reverted, after I restarted the host. So avoiding
	FSARG=""

	# First time edit /etc/network/interfaces wrt ens3 to set the ip addr to 192.168.66.10 statically
	qemu-system-x86_64 $GENRARG $NETARG1 -hda u1604m10.hda -hdb /media/hanishkvc/CAM_SD/test10.hdb $FSARG &
	sleep 2
	# First time edit /etc/network/interfaces wrt ens3 to set the ip addr to 192.168.66.11 statically
	qemu-system-x86_64 $GENRARG $NETARG2 -hda u1604m11.hda -hdb /media/hanishkvc/CAM_SD/test11.hdb $FSARG &
}

$1

