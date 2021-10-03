MYDIR=`pwd`
cd $MYDIR/third_party/tbb
pwd
cd build
make clean
cmake ..
make -j

cd $MYDIR
mkdir build
cd build
make clean
cmake ..
make -j