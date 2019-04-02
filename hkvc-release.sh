

typeSB="bin"
typeSCA="srv"

for i in $@; do
  echo $i
  if [ "$i" == "src" ]; then
    typeSB="src"
  fi
  if [ "$i" == "bin" ]; then
    typeSB="bin"
  fi
  if [ "$i" == "srv" ]; then
    typeSCA="srv"
  fi
  if [ "$i" == "cli" ]; then
    typeSCA="cli"
  fi
  if [ "$i" == "all" ]; then
    typeSCA="all"
  fi
done

echo "typeSB [$typeSB]"
echo "typeSCA [$typeSCA]"


base="../tmp/release"
date=`date +%Y%m%d%Z%H%M`
dir="SimpNwMon02_v$date-$typeSB-$typeSCA"
fdir=$base/$dir

echo "dir [$dir]"

mkdir -p $fdir
if [ "$typeSCA" == "srv" ]; then
  if [ "$typeSB" == "bin" ]; then
    mkdir $fdir/srv
    cp -a server/python/*so $fdir/srv/
    pushd $fdir/srv/
    strip *so
    popd
    cp -a server/python/hkvc-nw-send-mcast.py $fdir/srv/
    cp -a scripts/hkvc-run-nw-send-mcast.sh $fdir/
    cp -a scripts/hkvc-snm-status-analyse.py $fdir/
    cp -a scripts/testDevs2Check.py $fdir/
  fi
elif [ "$typeSCA" == "all" ]; then
  cp -a . $fdir/
fi

echo "base [$base], type exit to continue"
pushd $base/
bash
7z -p a $dir.7z $dir
pwd
ls -al

