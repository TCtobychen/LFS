#! /bin/bash

cd testfolder

mkdir OSFS

cd MyLFS
mkdir files
cd files
touch t1
echo "testing" > t1
cat t1
mkdir a1
mv t1 a1
cat a1/t1

cd ../../OSFS
mkdir files
cd files
touch t1
echo "testing" > t1
cat t1
mkdir a1
mv t1 a1
cat a1/t1

cd ../..

diff -r MyLFS/files OSFS/files
tree
rm -r MyLFS/files
rm -r OSFS