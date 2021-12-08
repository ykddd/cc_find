MYDIR=`pwd`/cc_find
cd $MYDIR/third_party/tbb
echo $MYDIR
pwd
mv build/ build_bak/
mkdir build
cp build_bak/version_string.ver.in build/
cd build
cmake ..
make -j

cd  $MYDIR/third_party/mimalloc
rm -rf build
mkdir build
cd build
cmake ..
make -j

cd $MYDIR
rm -rf build
mkdir build
cd build
make clean
cmake ..
make -j
