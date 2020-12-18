#! /bin/bash
cd testfolder

mkdir OSFS

cd MyLFS
mkdir files
cd files
mkdir a1
mkdir a1/a2
mkdir a1/a3
mkdir a1/a2/a4

cd ../../OSFS
mkdir files
cd files
mkdir a1
mkdir a1/a2
mkdir a1/a3
mkdir a1/a2/a4

cd ../..

diff -r MyLFS/files OSFS/files
tree
rm -r MyLFS/files
rm -r OSFS
