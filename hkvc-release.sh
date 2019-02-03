
base="../tmp/release"
date=`date +%Y%m%d%Z%H%M`
dir="SimpNwMon02_v$date"
fdir=$base/$dir

mkdir -p $fdir
cp -a . $fdir/
cd $base/
7z -p a $dir.7z $dir
pwd
ls -al

