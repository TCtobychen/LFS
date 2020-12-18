#! /bin/bash

echo "ln test"
cd testfolder/MyLFS
mkdir a
touch t
echo 'testing' > t
ln t a/t1

tree
cat a/t1
echo 'changed' > a/t1
cat t

rm t
tree
cat a/t1
rm -r a