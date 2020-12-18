#define FUSE_USE_VERSION 31

#include <fuse.h>
#include "inode.h"
#include "LFS.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

char * mount_point;

int check(int inode_num){
	if (inode_num == -ENOENT) return -ENOENT;
	if (inode_num == -EACCES) return -EACCES;
	if (inode_num == -ENOMEM) return -ENOMEM;
	return 1;
}

void readDisk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pread(suBlock->fd, buf, size, offset);
}

void writeDisk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pwrite(suBlock->fd, buf, size, offset);
	suBlock->logTail += size;
}

void updateTime(int inodeNum, struct timespec ctime, struct timespec atime, struct timespec mtime){
	suBlock->imapTable.timeTable[inodeNum][0] = ctime;
	suBlock->imapTable.timeTable[inodeNum][1] = atime;
	suBlock->imapTable.timeTable[inodeNum][2] = mtime;
}

void updateSuperBLK(int inodeNum, long inodeTail, struct timespec ctime, struct timespec atime, struct timespec mtime){
	suBlock->imapTable.addrTable[inodeNum] = inodeTail - sizeof(struct Inode);
	updateTime(inodeNum, ctime, atime, mtime);
}

mode_t getMode(uid_t fileUid, uid_t fileGid, mode_t mode){
	mode = mode - (mode & __S_IFMT);
	if (getuid() == 0)
		return 07;	// root has all permission.
	if (fileUid == getuid())
		return (mode&0700)/0100;		// Owner
	else if (fileGid == getgid()) 
		return (mode&0070)/0010;		// Group
	return (mode&0007);
}

int findInodeNum(int baseInodeNum, const char *suffix){
	//printf("FindInodeNum, %s, %d\n", suffix, baseInodeNum);
	if ( strcmp( suffix, "/" ) == 0 )
		return 0;

	struct Inode baseInode;
	struct DirtDataBLK buf;
	char tmp[MAX_PATH_LEN];
	char *path;
	path = tmp;
	memcpy(path, suffix, strlen(suffix)+1);
	char itemName[MAX_FILE_NAME];

	// get name and exam whether is a file
	int dirNum = 0, idx=0;
	while(*path != '\0' && dirNum<1) {
		path++;
		itemName[idx] = *path;
		idx++;
		if (*path == '/'){
			dirNum += 1; 
			itemName[idx-1] = '\0';
		}
    }

	if (idx > MAX_FILE_NAME)
		return -ENOMEM;

	// recursively find the inodeNum 
	readDisk(&baseInode, sizeof(struct Inode), suBlock->imapTable.addrTable[baseInodeNum]);

	int permission = getMode(baseInode.uid, baseInode.gid,  baseInode.mode);
	if (permission < 04)
		return -EACCES;

	for (int i=0; i<baseInode.numBLK; i++){
		readDisk(&buf, BLKSIZE, baseInode.inodeAddrTable[i]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (strcmp(itemName, buf.fileNameTable[j]) == 0){
				// Todo: need to exam whether a dirt.
				int nextInodeNum = buf.inodeNumTable[j];
				if (dirNum == 1)	return findInodeNum(nextInodeNum, path);
				return nextInodeNum;
			}
		}
	}
	// file does not exist.
	return -ENOENT;
}

void LFS_init()
{
	suBlock = (struct  LFS_superBlock*)malloc(sizeof(struct  LFS_superBlock));
	int fd;
	char *buf;
    fd = open("log/LFS_Log", O_RDWR|O_CREAT|O_EXCL, 0700);
	// Log exists
    if (fd == -1){
        fd = open("log/LFS_Log", O_RDWR);
        pread(fd, suBlock, sizeof(struct LFS_superBlock), 0);
		suBlock->fd = fd;
    }
	else{
		// Initial log
		buf = malloc(LOG_SIZE);
		memset((void*)buf,0,LOG_SIZE);
		pwrite(fd, buf, LOG_SIZE, 0);
		//printf("LFS Init 1\n");
		free(buf);
		suBlock->fd = fd;

		/********** Init for '/' **************/
		suBlock->inodeNum = 1;
		suBlock->logTail = sizeof(struct LFS_superBlock);
		// empty data block
		buf = malloc(BLKSIZE);
		memset((void*)buf,0,BLKSIZE);
		writeDisk(buf, BLKSIZE, suBlock->logTail);
		free(buf);
		// '/' inode
		buf = malloc(sizeof(struct Inode));
		// Initially, no data block is allocated to dir, so size == 0
		newInode(buf, getuid(), getgid(),S_IFDIR | 0755, 0, 2, 0, NULL);
		writeDisk(buf, sizeof(struct Inode), suBlock->logTail);
		free(buf);
		// update super block
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		updateSuperBLK(0, suBlock->logTail,ts,ts,ts);
		// Finally, push suBLK to disk
		pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
		/********** Init for '/' **************/
	}
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	//printf("CREATE, <%s%s>, <%d>\n",mount_point, path, mode);
	int fileInodeNum = findInodeNum(0, path);
	/*
	if (fileInodeNum != -ENOENT) return -EEXIST;
	if (fileInodeNum == -EACCES) return -EACCES;
	if (fileInodeNum == -ENOMEM) return -ENOMEM;
	*/
	if (check(fileInodeNum)!=1) return check(fileInodeNum);

	char parent_dir[MAX_PATH_LEN];
	memcpy(parent_dir, path, strlen(path)+1);
	int dirInodeNum = 0, filename = 0;
	for (filename=strlen(path)-1; filename>0; filename--)
		if (*(parent_dir+filename) == '/'){
			*(parent_dir+filename) = '\0';
			dirInodeNum = findInodeNum(0, parent_dir);
			break;
		}
	if (dirInodeNum == -ENOENT) return -ENOENT;
	
	struct Inode new_Inode, dirInode;
	struct DirtDataBLK dir_data;

	readDisk(&dirInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int permission = getMode(dirInode.uid, dirInode.gid, dirInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03) return -EACCES;
	// new file's inode and write it into disk
	newInode(&new_Inode, getuid(), getgid(), mode, 0, 1, 0, NULL);
	writeDisk(&new_Inode, sizeof(struct Inode), suBlock->logTail);

	// insert new suBLK entry
	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
	updateSuperBLK(suBlock->inodeNum, suBlock->logTail, timestamp,timestamp,timestamp);
	suBlock->inodeNum += 1;

	// Add new entry into dir table
	readDisk(&new_Inode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	if (new_Inode.numBLK >= 1){
		readDisk(&dir_data, BLKSIZE, new_Inode.inodeAddrTable[new_Inode.numBLK-1]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (dir_data.fileNameTable[j][0] == 0){
				dir_data.inodeNumTable[j] = suBlock->inodeNum - 1;
				memcpy(dir_data.fileNameTable[j], path+filename+1, strlen(path+filename+1)+1);
				successInsert = 1;
				break;
			}
		}
	}
	if (successInsert == 1){
		new_Inode.inodeAddrTable[new_Inode.numBLK-1] = suBlock->logTail;
		writeDisk(&dir_data, BLKSIZE, suBlock->logTail);
		writeDisk(&new_Inode, sizeof(struct Inode), suBlock->logTail);
	}
	// New data block
	else{
		memset(&dir_data,0,BLKSIZE);
		dir_data.inodeNumTable[0] = suBlock->inodeNum - 1;
		memcpy(dir_data.fileNameTable[0], path+filename+1, strlen(path+filename+1)+1);
		new_Inode.numBLK += 1;
		new_Inode.size = new_Inode.numBLK*BLKSIZE; // dir's size update
		new_Inode.inodeAddrTable[new_Inode.numBLK-1] = suBlock->logTail;
		writeDisk(&dir_data, BLKSIZE, suBlock->logTail);
		writeDisk(&new_Inode, sizeof(struct Inode), suBlock->logTail);
		
	}
	updateSuperBLK(dirInodeNum, suBlock->logTail, timestamp, timestamp, timestamp);
	// Finally, push suBLK to disk
	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}

static int do_remove(const char *path){
	printf("UNLINK, <%s%s>\n",mount_point, path);

	// find the path
	char p[MAX_PATH_LEN];
	memcpy(p, path, strlen(path)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(path)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = findInodeNum(0, p);
			break;
		}
	/*
	if (dirInodeNum == -ENOENT){
		return -ENOENT;	// dir does not exist.
	}
	if (dirInodeNum == -EACCES)
		return -EACCES;
	if (dirInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.
	*/
	if (check(dirInodeNum)!=1) return check(dirInodeNum);

	struct Inode tmpInode;
	struct DirtDataBLK buf;
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int permission = getMode(tmpInode.uid, tmpInode.gid, tmpInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03)
		return -EACCES;

	int fileInodeNum = findInodeNum(dirInodeNum, path+suffix), ret;
	if (fileInodeNum == -ENOENT){
		return -ENOENT;	// dir does not exist.
	}
	if (fileInodeNum == -EACCES)
		return -EACCES;	// file does not exist.
	if (fileInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.
	struct Inode fileInode;
	readDisk(&fileInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	/*	Not used since we use unlink to rm a dir's inode
	if (!S_ISREG(fileInode.mode)){
		return -EISDIR; // Not a file.
	}
	*/
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (fileInode.nLink > 1){
		fileInode.nLink -= 1;
		writeDisk(&fileInode, sizeof(struct Inode), suBlock->logTail);
		updateSuperBLK(fileInodeNum, suBlock->logTail, ts,
				  suBlock->imapTable.timeTable[fileInodeNum][1], suBlock->imapTable.timeTable[fileInodeNum][2]);
	}

	// Todo: Need to recycle unlinked entries
	// update dir's datablock and inode
	
	int successRm = 0, i;
	for (i = 0; i<tmpInode.numBLK; i++){
		readDisk(&buf, BLKSIZE, tmpInode.inodeAddrTable[i]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (strcmp(buf.fileNameTable[j], path+suffix+1) == 0){
				buf.inodeNumTable[j] = 0;
				memset(buf.fileNameTable[j], 0, sizeof(buf.fileNameTable[j]));
				buf.fileNameTable[j][0]=-1;	// Indicate that the entry is removed.
				successRm = 1;
				break;
			}
		}
		if (successRm == 1) break;
	}
	tmpInode.inodeAddrTable[i] = suBlock->logTail;
	writeDisk(&buf, BLKSIZE, suBlock->logTail);
	writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);

	// update super Block
	updateSuperBLK(dirInodeNum, suBlock->logTail, ts,
				  ts, ts);
	// Finally, push suBLK to disk
	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}

int base_getattr(const char *path, struct stat *st, int baseInodeNum, int isDirect){
	//printf("*****%ld********\n", suBlock->logTail);
	int inodeNum;
	if (isDirect==1) inodeNum = baseInodeNum;
	else	inodeNum = findInodeNum(baseInodeNum, path);
	if (inodeNum == -ENOENT){
		// file not found.
		st = NULL;
		return -ENOENT;
	}
	if (inodeNum == -EACCES){
		st = NULL;
		return -EACCES;
	}
	if (inodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.
	st->st_ino = inodeNum;
	st->st_ctim = suBlock->imapTable.timeTable[inodeNum][0];
	st->st_atim = suBlock->imapTable.timeTable[inodeNum][1];
	st->st_mtim = suBlock->imapTable.timeTable[inodeNum][2];
	//st->st_
	struct Inode fileInode;
	readDisk(&fileInode, sizeof(struct Inode), suBlock->imapTable.addrTable[inodeNum]);
	st->st_uid = fileInode.uid;
	st->st_gid = fileInode.gid;
	st->st_mode = fileInode.mode;
	st->st_nlink = fileInode.nLink;
	st->st_size = fileInode.size;
	st->st_blksize = BLKSIZE;
	st->st_blocks = fileInode.numBLK;	// need to change into 512 bytes block?
	return 0;
}

static int do_getattr( const char *path, struct stat *st ){
    printf("GETATTR, <%s%s>\n",mount_point, path);
	return base_getattr( path, st, 0, 0);
}

static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	printf("READDIR, <%s%s>, <%x>,<%ld>\n",mount_point, path, buffer,offset);
	int dirInodeNum = findInodeNum(0, path);
	if (check(dirInodeNum)!=1) return check(dirInodeNum);
	struct Inode dirInode;
	readDisk(&dirInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	if (!S_ISDIR(dirInode.mode)){
		return -ENOTDIR; // Not a dir.
	}

	int permission = getMode(dirInode.uid, dirInode.gid,  dirInode.mode);
	if (permission < 04)
		return -EACCES;

	struct DirtDataBLK buf;
	struct stat statbuf;
	base_getattr(NULL, &statbuf, dirInodeNum,1);
	filler( buffer, ".", &statbuf, 0 , 0); // Current Directory

	// Load files' attr
	for (int i=0; i<dirInode.numBLK; i++){
		readDisk(&buf, BLKSIZE, dirInode.inodeAddrTable[i]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			// 0 means after that no entry, -1 means the entry is removed 
			if (buf.fileNameTable[j][0] == 0 ) break;
			else if (buf.fileNameTable[j][0] != -1){
				base_getattr(NULL, &statbuf, buf.inodeNumTable[j],1);
				filler( buffer, buf.fileNameTable[j], &statbuf, 0 , 0);
			}
		}
	}
	return 0;
}

int inodeBaseRead(const struct Inode *fileInode, char *buffer, size_t size, off_t offset){
	if (offset > fileInode->size)	return 0; // No data to be read.
	
	// off+size not larger than file's size
	size = MIN(size, fileInode->size-offset);
	char *buf = malloc(size);
	int start = offset/BLKSIZE, end = (offset+size)/BLKSIZE;
	long startpart = BLKSIZE-(offset-start*BLKSIZE), endpart = size - (end*BLKSIZE - offset);

	// [off,off+size] tranverse more than one data block
	if (end-start>0){
		readDisk(buf, startpart, fileInode->inodeAddrTable[start]+BLKSIZE-startpart);
		int j=0;
		for (int i = start+1; i<end; i++, j++){
			readDisk(buf+j*BLKSIZE+startpart, BLKSIZE, fileInode->inodeAddrTable[i]);
		}
		readDisk(buf+j*BLKSIZE+startpart, endpart, fileInode->inodeAddrTable[end]);
	}
	// [off,off+size] int the same data block
	else{
		readDisk(buf, size, fileInode->inodeAddrTable[start]+(offset-start*BLKSIZE));
	}
	memcpy(buffer, buf, size);
	free(buf);
	return size;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi ){
	printf("READ, <%s%s>, <%x>, <%ld>, <%ld>\n",mount_point, path, buffer, size, offset);
	int fileInodeNum = findInodeNum(0, path), ret;
	/*
	if (fileInodeNum != -ENOENT) return -EEXIST;
	if (fileInodeNum == -EACCES) return -EACCES;
	if (fileInodeNum == -ENOMEM) return -ENOMEM;
	*/
	if (check(fileInodeNum)!=1) return check(fileInodeNum);
	struct Inode fileInode;
	readDisk(&fileInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	if (!S_ISREG(fileInode.mode))return -EISDIR;

	struct timespec timestamp;
	clock_gettime(CLOCK_REALTIME, &timestamp);
	int permission = getMode(fileInode.uid, fileInode.gid,  fileInode.mode);
	if (permission < 04) return -EACCES;
	updateTime(fileInodeNum, suBlock->imapTable.timeTable[fileInodeNum][0], timestamp, suBlock->imapTable.timeTable[fileInodeNum][2]);
	ret = inodeBaseRead(&fileInode, buffer, size, offset);
	return ret;
}

static int do_write( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi ){
	printf("WRITE, <%s%s>, <%x>, <%ld>, <%ld>\n", mount_point, path, buffer, size, offset);
	int fileInodeNum = findInodeNum(0, path);
	/*
	if (fileInodeNum != -ENOENT) return -EEXIST;
	if (fileInodeNum == -EACCES) return -EACCES;
	if (fileInodeNum == -ENOMEM) return -ENOMEM;
	*/
	if (check(fileInodeNum)!=1) return check(fileInodeNum);
	

	struct Inode fileInode;
	readDisk(&fileInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	if (!S_ISREG(fileInode.mode)){
		return -EISDIR; // Not a file.
	}
	if (offset>fileInode.size){
		return 0;	// Something wrong! No data written.
	}

	int permission = getMode(fileInode.uid, fileInode.gid, fileInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03)
		return -EACCES;

	int start = offset/BLKSIZE, end = (offset+size)/BLKSIZE, newBlkNum, noModified;
	if (end > MAX_FILE_SIZE_INBLK){
		//do_remove(path);
		return -EFBIG;
	}
	long startpart = BLKSIZE-(offset-start*BLKSIZE), endpart = size - (end*BLKSIZE - offset);
	if (endpart == 0)	newBlkNum = end-start, noModified = fileInode.numBLK-end;
	else 	newBlkNum = end-start+1, noModified = fileInode.numBLK-1-end;
	char buf[newBlkNum*BLKSIZE];
	// Load data from disk, modified in memory
	memset(buf,0,newBlkNum*BLKSIZE);
	inodeBaseRead(&fileInode, buf, newBlkNum*BLKSIZE, start*BLKSIZE);
	memcpy(buf+(offset-start*BLKSIZE), buffer, size);

	// update inode and write new data blocks, inode into disk
	fileInode.numBLK = start + newBlkNum + MAX(0, noModified);
	fileInode.size = MAX(offset+size, fileInode.size);
	for (int i = start; i<fileInode.numBLK; i++){
		fileInode.inodeAddrTable[i] = suBlock->logTail;
		writeDisk(buf+(i-start)*BLKSIZE, BLKSIZE, suBlock->logTail);
	}
	writeDisk(&fileInode, sizeof(struct Inode), suBlock->logTail);

	// update suBLK
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	updateSuperBLK(fileInodeNum, suBlock->logTail, ts,
				  ts, ts);

	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return size;
}
static int do_mkdir(const char *path, mode_t mode){
	printf("MKDIR, <%s%s>, <%d>\n",mount_point, path, mode);
	// use do_create to make new inode and insert entry into dir.
	return do_create(path, mode|S_IFDIR, NULL); 
}

static int do_rmdir(const char *path){
	printf("RMDIR, <%s%s>\n",mount_point, path);
	// use do_rmove to delete entry in the dir.
	return do_remove(path);
}

static int do_chmod(const char *path, mode_t mode, struct fuse_file_info *fi){
	//realpath(path+1, mount_point);
	printf("CHMOD, <%s%s>, <%d>\n",mount_point, path,mode);
	int fileInodeNum = findInodeNum(0, path);
	/*
	if (fileInodeNum != -ENOENT) return -EEXIST;
	if (fileInodeNum == -EACCES) return -EACCES;
	if (fileInodeNum == -ENOMEM) return -ENOMEM;
	*/
	if (check(fileInodeNum)!=1) return check(fileInodeNum);

	struct Inode srcInode;
	readDisk(&srcInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	if (srcInode.uid != getuid() && getuid() != 0)
		return -EPERM;
	if (S_ISDIR(srcInode.mode)){
		srcInode.mode = mode | S_IFDIR;
	}
	else if (S_ISREG(srcInode.mode)){
		srcInode.mode = mode | S_IFREG;
	}
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	writeDisk(&srcInode, sizeof(struct Inode), suBlock->logTail);
	updateSuperBLK(fileInodeNum, suBlock->logTail, ts,
				  suBlock->imapTable.timeTable[fileInodeNum][1], suBlock->imapTable.timeTable[fileInodeNum][2]);
	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}
static int do_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	printf("CHOWN, <%s%s>, <%d>, <%d>\n",mount_point, path, uid, gid);
	if (getuid() != 0)	return -EPERM; 	// Not root
	int fileInodeNum = findInodeNum(0, path);
	static int do_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	printf("CHOWN, <%s%s>, <%d>, <%d>\n",mount_point, path, uid, gid);
	if (getuid() != 0)	return -EPERM; 	// Not root
	int fileInodeNum = findInodeNum(0, path);
	/*
	if (fileInodeNum != -ENOENT) return -EEXIST;
	if (fileInodeNum == -EACCES) return -EACCES;
	if (fileInodeNum == -ENOMEM) return -ENOMEM;
	*/
	if (check(fileInodeNum)!=1) return check(fileInodeNum);

	struct Inode tmpInode;
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	if (uid != -1)	tmpInode.uid = uid;
	if (gid != -1)	tmpInode.gid = gid;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	updateSuperBLK(fileInodeNum, suBlock->logTail, ts,
				  suBlock->imapTable.timeTable[fileInodeNum][1], suBlock->imapTable.timeTable[fileInodeNum][2]);
	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}

	struct Inode tmpInode;
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[fileInodeNum]);
	if (uid != -1)	tmpInode.uid = uid;
	if (gid != -1)	tmpInode.gid = gid;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	updateSuperBLK(fileInodeNum, suBlock->logTail, ts,
				  suBlock->imapTable.timeTable[fileInodeNum][1], suBlock->imapTable.timeTable[fileInodeNum][2]);
	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}


static int do_symlink (const char *srcPath, const char *destPath){
	printf("SOFTLINK, <%s%s>, <%s%s>\n",mount_point, srcPath,mount_point, destPath);
	int srcInodeNum = findInodeNum(0, srcPath), destInodeNum = findInodeNum(0, destPath);
	if (srcInodeNum == -ENOENT)
		return -ENOENT;	// src file does not exist.
	if (destInodeNum != -ENOENT)
		return -EEXIST;	// dest file exists.
	if (srcInodeNum == -EACCES || destInodeNum == -EACCES)
		return -EACCES; // permission denied.
	if (srcInodeNum == -ENOMEM || destInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.

	struct Inode srcInode, tmpInode;
	readDisk(&srcInode, sizeof(struct Inode), suBlock->imapTable.addrTable[srcInodeNum]);

	writeDisk(&srcInode, sizeof(struct Inode), suBlock->logTail);
	//st_
	// insert new suBLK entry
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	updateSuperBLK(suBlock->inodeNum, suBlock->logTail, ts,ts,ts);
	suBlock->inodeNum += 1;

	char p[MAX_PATH_LEN];
	memcpy(p, destPath, strlen(destPath)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(destPath)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = findInodeNum(0, p);
			break;
		}
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	struct DirtDataBLK buf;
	if (tmpInode.numBLK >= 1){
		readDisk(&buf, BLKSIZE, tmpInode.inodeAddrTable[tmpInode.numBLK-1]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (buf.fileNameTable[j][0] == 0){
				buf.inodeNumTable[j] = suBlock->inodeNum-1;
				memcpy(buf.fileNameTable[j], destPath+suffix+1, strlen(destPath+suffix+1)+1);
				successInsert = 1;
				break;
			}
		}
	}
	if (successInsert == 1){
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	// New data block
	else{
		memset(&buf,0,BLKSIZE);
		buf.inodeNumTable[0] = suBlock->inodeNum-1;
		memcpy(buf.fileNameTable[0], destPath+suffix+1, strlen(destPath+suffix+1)+1);
		tmpInode.numBLK += 1;
		tmpInode.size = tmpInode.numBLK*BLKSIZE; // dir's size update
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	updateSuperBLK(dirInodeNum, suBlock->logTail, ts, ts, ts);

	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}


static int do_link(const char *srcPath, const char *destPath){
	printf("LINK, <%s%s>, <%s%s>\n",mount_point, srcPath,mount_point, destPath);
	// Most the same as do_rename
	int srcInodeNum = findInodeNum(0, srcPath), destInodeNum = findInodeNum(0, destPath);
	if (srcInodeNum == -ENOENT)
		return -ENOENT;	// src file does not exist.
	if (destInodeNum != -ENOENT)
		return -EEXIST;	// dest file exists.
	if (srcInodeNum == -EACCES || destInodeNum == -EACCES)
		return -EACCES; // permission denied.
	if (srcInodeNum == -ENOMEM || destInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.

	struct Inode srcInode, tmpInode;
	readDisk(&srcInode, sizeof(struct Inode), suBlock->imapTable.addrTable[srcInodeNum]);

	// new src inode
	srcInode.nLink += 1;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	writeDisk(&srcInode, sizeof(struct Inode), suBlock->logTail);
	updateSuperBLK(srcInodeNum, suBlock->logTail, ts,
				  suBlock->imapTable.timeTable[srcInodeNum][1], suBlock->imapTable.timeTable[srcInodeNum][2]);

	// add src name to the dest dir, copy from do_create part, better to create a new module
	char p[MAX_PATH_LEN];
	memcpy(p, destPath, strlen(destPath)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(destPath)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = findInodeNum(0, p);
			break;
		}
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	struct DirtDataBLK buf;
	if (tmpInode.numBLK >= 1){
		readDisk(&buf, BLKSIZE, tmpInode.inodeAddrTable[tmpInode.numBLK-1]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (buf.fileNameTable[j][0] == 0){
				buf.inodeNumTable[j] = srcInodeNum;
				memcpy(buf.fileNameTable[j], destPath+suffix+1, strlen(destPath+suffix+1)+1);
				successInsert = 1;
				break;
			}
		}
	}
	if (successInsert == 1){
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	// New data block
	else{
		memset(&buf,0,BLKSIZE);
		buf.inodeNumTable[0] = srcInodeNum;
		memcpy(buf.fileNameTable[0], destPath+suffix+1, strlen(destPath+suffix+1)+1);
		tmpInode.numBLK += 1;
		tmpInode.size = tmpInode.numBLK*BLKSIZE; // dir's size update
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	updateSuperBLK(dirInodeNum, suBlock->logTail, ts,
				  ts, ts);

	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}



static int do_utimens (const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
	printf("UTIMENS, <%s%s>\n",mount_point, path);
	int fileInodeNum = findInodeNum(0, path);
	if (fileInodeNum == -ENOENT)
		return -ENOENT;	// file or dir does not exist.
	if (fileInodeNum == -EACCES)
		return -EACCES;	// permission denied.
	if (fileInodeNum == -ENOMEM)
		return -ENOMEM;	// permission denied.
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	updateTime(fileInodeNum, ts, tv[0], tv[1]);
	return 0;
}

static int do_rename(const char *srcPath, const char *destPath, unsigned int flags){
	printf("RENAME, <%s%s>, <%s%s>, <%d>\n",mount_point, srcPath,mount_point, destPath, flags);
	int srcInodeNum = findInodeNum(0, srcPath), destInodeNum = findInodeNum(0, destPath);
	if (srcInodeNum == -ENOENT)
		return -ENOENT;	// src file does not exist.
	if (destInodeNum != -ENOENT)
		return -EEXIST;	// dest file exists.
	if (srcInodeNum == -EACCES || destInodeNum == -EACCES)
		return -EACCES; // permission denied.
	if (srcInodeNum == -ENOMEM || destInodeNum == -ENOMEM)
		return -ENOMEM; // permission denied.

	struct Inode srcInode, tmpInode;
	readDisk(&srcInode, sizeof(struct Inode), suBlock->imapTable.addrTable[srcInodeNum]);
	// unlink src entry in src path, update src inode. 
	do_remove(srcPath);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	updateTime(srcInodeNum, ts, ts, ts);

	// add src inode to the dest dir, copy from do_create part, better to create a new module
	char p[MAX_PATH_LEN];
	memcpy(p, destPath, strlen(destPath)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(destPath)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = findInodeNum(0, p);
			break;
		}
	readDisk(&tmpInode, sizeof(struct Inode), suBlock->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	struct DirtDataBLK buf;
	if (tmpInode.numBLK >= 1){
		readDisk(&buf, BLKSIZE, tmpInode.inodeAddrTable[tmpInode.numBLK-1]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (buf.fileNameTable[j][0] == 0){
				buf.inodeNumTable[j] = srcInodeNum;
				memcpy(buf.fileNameTable[j], destPath+suffix+1, strlen(destPath+suffix+1)+1);
				successInsert = 1;
				break;
			}
		}
	}
	if (successInsert == 1){
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	// New data block
	else{
		memset(&buf,0,BLKSIZE);
		buf.inodeNumTable[0] = srcInodeNum;
		memcpy(buf.fileNameTable[0], destPath+suffix+1, strlen(destPath+suffix+1)+1);
		tmpInode.numBLK += 1;
		tmpInode.size = tmpInode.numBLK*BLKSIZE; // dir's size update
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = suBlock->logTail;
		writeDisk(&buf, BLKSIZE, suBlock->logTail);
		writeDisk(&tmpInode, sizeof(struct Inode), suBlock->logTail);
	}
	updateSuperBLK(dirInodeNum, suBlock->logTail, ts,
				  ts, ts);

	pwrite(suBlock->fd, suBlock, sizeof(struct LFS_superBlock), 0);
	return 0;
}


static struct fuse_operations operations = {
	.create		= do_create,
	.unlink		= do_remove,
    .getattr	= do_getattr,
    .readdir	= do_readdir,
	.read		= do_read,
	.write		= do_write,
	.link		= do_link,
	.chmod		= do_chmod,
	.mkdir		= do_mkdir,
	.rmdir		= do_rmdir,
	.rename		= do_rename,
	.utimens	= do_utimens,
	.chown		= do_chown,
	//.symlink	= do_symlink,
};

int main( int argc, char *argv[] )
{
	mount_point = argv[argc-1];
	//memcpy(mount_point, argv[argc-1], strlen(argv[argc-1])+1);
	LFS_init();
	return fuse_main(argc, argv, &operations, NULL);
}