cd /tmp/fuse2
rm -vf *
mkdir 1
mkdir 2
echo "testing making directory with existing name"
mkdir 1
touch file_1
echo "AAA" > file_1
cd 1
touch file_2
echo "BBB" > file_2
cd ..
echo "testing reading the two file contents together"
cat file_1 1/file_2
rm file_1
rm 1/file_2
echo "testing reading deleted file"
cat file_1
touch file_3
