#! /bin/bash

cd testfolder

mkdir OSFS

cd MyLFS
mkdir files
cd files
mkdir a1
touch t1
touch a1/t2
mkdir a1/a2
touch a1/a2/t3


cd ../../OSFS
mkdir files
cd files
mkdir a1
touch t1
touch a1/t2
mkdir a1/a2
touch a1/a2/t3

cd ../..

diff -r MyLFS/files OSFS/files
tree

cd MyLFS/files
rm t1
rm a1/t2
rm -r a1/a2
cd ../../OSFS/files
rm t1
rm a1/t2
rm -r a1/a2

cd ../..

diff -r MyLFS/files OSFS/files
tree

rm -r MyLFS/files
rm -r OSFS