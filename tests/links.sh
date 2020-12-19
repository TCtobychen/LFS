cd /tmp/fuse2
touch link_source
echo "LINKS" > link_source
ln link_source l1
echo "Testing hard link contents -- Expected LINKS"
cat l1
echo "Test over"
rm link_source 
echo "creating new link l2"
ln l1 l2
echo "Testing hard link contents after the source is deleted -- Expected LINKS LINKS"
cat l1 l2
echo "Test over"
tree 
rm l1
rm l2
