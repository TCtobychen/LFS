#! /bin/bash

cd testfolder
cd MyLFS
mkdir files

cd files
mkdir a1
mkdir a2
mkdir a3
mkdir a4
mkdir a1/b1
mkdir a2/b2
mkdir a3/b3
mkdir a4/b4
mkdir a1/b5
mkdir a3/b6
tree

cd ..
rm -r files