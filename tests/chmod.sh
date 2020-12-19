cd /tmp/fuse2
touch exe
touch t.txt
echo "execute" > exe
echo "text" > t.txt
ls
echo "Testing changing mode of executable files"
chmod 777 exe
./exe
chmod 666 exe
./exe
chmod 111 exe
./exe
chmod 777 exe
echo "Test over"
echo "Testing changing mode of text files" 
chmod 777 t.txt
cat t.txt
chmod 666 t.txt
cat t.txt
chmod 111 t.txt
cat t.txt
chmod 000 t.txt
cat t.txt
chmod 777 t.txt
echo "Test over"
rm exe
rm t.txt

