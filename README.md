# Log-structured file system

Zhixue Shen 2017012261; Yiheng Shen 2017010340

## Main Data Structure

The LFS has a LOG with size 100MB and dir’s name is at most 128 Bytes.

A file or dir can occupy at most 10240 data blocks.

The LFS can have at most 2048 inodes. 

Important elements in the LFS system:

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

## About the test case

The test cases are in the tests folder; Since the time is limited, we only test some basic operations in our test case; We need to first enable the run.sh root right and run it as the following  (run in the main directory)

    make clean; make; chmod u+x run.sh; ./run.sh

This would mount our point to the directory /tmp/fuse2
Then we open a **new** terminal to enter the tests folder and run the tests.

    cd tests; chmod u+x test.sh; ./test.sh

The test.sh runs three substests as the follows:

This **basics.sh** tests the `mkdir`; `touch`; `read` & `write` and some normal errors, like newing a folder with an existing name and opening a deleted file. 

The **chmod.sh** tests the `chmod`; We created two files, one is for execute (`exe`) and one is for `cat` (`t.txt`), then we use chmod to change their permissions (by `chmod 777/444/111`) and test if we have correctly change the permissions.

The **link.sh** tests the `ln` operation (building hard links); Though in the manual, it said we can hard link to directories, but the Linux system does not allows that. We do not allow that in our system, either. 

## Block_dump

We have one script to `pretty_print` the whole log. In the script we print all `Inode`'s metadata in order, including uid, gid and so forth. 

To output this, we run `./bin/block_dump.o`

## How we achieve requirements 7 - 10
Requirement 7 is achieved by implementing the function `do_chown` and `do_chmod`. We first exam the authority of the current user and if the authority is owned we make a new inode with new permissions. For requirement 9, we have not implement the `sync` in this project yet. For requirements 8 and 10, the threads safety and crash-recovery are guaranteed by the super block, since we first load the super block and modify it in the memory. It only writes to the disk after all the other data and metadata are written. This could prevent inconsistent states.  