MYDIR=`pwd`/src
cd $MYDIR/third_party/tbb
echo $MYDIR
pwd
cd build
make clean
cmake ..
make -j

cd $MYDIR
rm -rf build
mkdir build
cd build
make clean
cmake ..
make -j