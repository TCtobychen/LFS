cd /tmp/fuse2
rm -vf *
mkdir 1
mkdir 2
echo "Testing making directory with existing name -- error expected"
mkdir 1
echo "Test over"
touch file_1
echo "AAA" > file_1
cd 1
touch file_2
echo "BBB" > file_2
cd ..
echo "Testing reading the two file contents together -- AAA BBB expected"
cat file_1 1/file_2
rm file_1
echo "Test over"
rm 1/file_2
echo "Testing reading deleted file -- error expected"
cat file_1
echo "Test over"
tree
rmdir 1
rmdir 2

