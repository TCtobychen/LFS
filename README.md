## Main Data Structure

The LFS has a LOG with size 100MB and dir’s name is at most 128 Bytes.

A file or dir can occupy at most 10240 data blocks.

The LFS can have at most 2048 inodes. 

Elements in the LFS system:

### LFS superBlock

SuperBlock that record next inode number, log tail and imapTable. SuperBlock has pre-determined size, store in the head of the log. Each time the LFS is mounted, we load the SuperBlock into the Mem and modify it in the Mem.

### ImapTable
The ImapTable record the disk address related to the inode number and the record create, access, modify time.

### Inode
Record mode, size, number of data blocks, number of links, data blocks’ address, uid, gid. 

## Code Organization

1. the src folder contains our sourdce code, the main document is LFS.c, which include our implementation of all the operations. 

2. the include folder contains our .h files, the inode.h contains information for the inode structure and LFS.h contains the information of our file system.


##How to run our code
First we should compile our code by
```
make clean; make
```
Suppose we are going to mount our file to the directory /tmp/fuse, then we run the following comand
```
mkdir /tmp/fuse; ./bin/lfs.o -d /tmp/fuse
```
Here for convenience we use -d option to test the system so that the system could be entered by linux terminal. Then we enter the file system by using a new terminal and type in
```
cd /tmp/fuse;
```
You could do the first trivial test by
```
mkdir 1; ls
```
As the terminal outputs 1, you could now enjoy our system!

## About the test case;

The test cases are in the tests folder; Since the time is limited, we only test some basic operations in our test case; We need to first enable the run.sh root right and run it as the following  (run in the main directory)

    make clean; make; chmod u+x run.sh; ./run.sh

This would mount our point to the directory /tmp/fuse2
Then we open a **new** terminal to enter the tests folder and run the tests.

    cd tests; chmod u+x test.sh; ./test.sh

The test.sh runs two subtests, one is the **basics.sh** and the other is **advanced.sh**

This **basics.sh** tests the `mkdir`; `touch`; `read` & `write` and some normal errors, like newing a folder with an existing name and opening a deleted file. 

The **advanced.sh** tests the `chmod`;

