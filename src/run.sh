MYDIR=`pwd`
cd $MYDIR/build
echo "执行的文件名：$0";
echo "第一个参数为：$1";
echo "第二个参数为：$2";
echo "第三个参数为：$3";

./cycle_detect $1 $2 $3
