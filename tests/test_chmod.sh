#! /bin/bash
echo "chmod test"
cd testfolder/MyLFS

touch hello
echo "I'm the executable" > hello
touch hello.txt
echo "I'm the normal file" > hello.txt
mkdir hellodir
tree

echo "\n************"
echo "Executable"
echo "************"
chmod 777 hello
./hello
chmod 666 hello
./hello
chmod 111 hello
./hello
chmod 777 hello

echo "\n************"
echo "Normal file"
echo "************"

chmod 777 hello.txt
cat hello.txt
chmod 666 hello.txt
cat hello.txt
chmod 111 hello.txt
cat hello.txt
chmod 000 hello.txt
cat hello.txt
chmod 777 hello.txt

echo "\n************"
echo "Normal dir"
echo "************"
chmod 744 hellodir
touch hellodir/hellodirtest.txt
echo "test" > hellodir/hellodirtest.txt

ls hellodir
chmod 444 hellodir #read
ls hellodir
cat hellodir/hellodirtest.txt
#echo "test2" > hellodir/hellodirtest.txt
touch hellodir/test2
ls hellodir

chmod 222 hellodir #write
ls hellodir
cat hellodir/hellodirtest.txt
touch hellodir/hellodirtest.txt

echo "test3" > hellodir/hellodirtest.txt
chmod 744 hellodir

cat hellodir/hellodirtest.txt
tree 

rm -r hellodir
rm hello.txt
rm hello
