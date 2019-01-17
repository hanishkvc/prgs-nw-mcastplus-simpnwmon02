
VM_SHARE_DIR=/tmp/share
mkdir $VM_SHARE_DIR
cp -a server/*.py $VM_SHARE_DIR/
cp -a client/c_lang/simpnwmon02 $VM_SHARE_DIR/
cp -a testing/hkvc-check-image.py $VM_SHARE_DIR/
cp -a testing/t.context $VM_SHARE_DIR/
cp -a testing/qemu-tftp-get-files.input $VM_SHARE_DIR/
echo "Transfered files to [$VM_SHARE_DIR]"

