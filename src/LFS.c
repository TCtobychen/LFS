#define FUSE_USE_VERSION 31

#include <fuse.h>
#include "inode.h"
#include "lfs.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

char mount_point[MAX_FILE_NAME*10] = {0};

int check_not_exist(int inodeNum){
	if (inodeNum != -ENOENT) return -EEXIST;
	if (inodeNum == -EACCES) return -EACCES;
	if (inodeNum == -ENOMEM) return -ENOMEM;
	return 1;
}

int check_exist(int inodeNum){
	if (inodeNum == -ENOENT) return -ENOENT;
	if (inodeNum == -EACCES) return -EACCES;
	if (inodeNum == -ENOMEM) return -ENOMEM;
	return 1;
}


void read_disk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pread(SBLK->fd, buf, size, offset);
}

void write_to_disk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pwrite(SBLK->fd, buf, size, offset);
	SBLK->logTail += size;
}

void update_time(int inodeNum, struct timespec tc, struct timespec ta, struct timespec tm){
	SBLK->imapTable.timeTable[inodeNum][0] = tc;
	SBLK->imapTable.timeTable[inodeNum][1] = ta;
	SBLK->imapTable.timeTable[inodeNum][2] = tm;
}

void update_sblk(int inodeNum, long inodeTail, struct timespec tc, struct timespec ta, struct timespec tm){
	SBLK->imapTable.addrTable[inodeNum] = inodeTail - sizeof(struct Inode);
	update_time(inodeNum, tc,ta,tm);
}

mode_t get_mode(uid_t fileUid, uid_t fileGid, mode_t mode){
	mode = mode - (mode & __S_IFMT);
	if (getuid() == 0)
		return 07;	// root has all permission.
	if (fileUid == getuid())
		return (mode&0700)/0100;		// Owner
	else if (fileGid == getgid()) 
		return (mode&0070)/0010;		// Group
	return (mode&0007);
}

int get_inode_num(int startInodeNum, const char *file){
	if (strcmp( file, "/" ) == 0) return 0;
	struct Inode startInode;
	struct DirtDataBLK dirbook;
	char tmp[MAX_FILE_NAME*10];
	char *path;
	path = tmp;
	memcpy(path, file, strlen(file)+1);
	char fileName[MAX_FILE_NAME];

	int ndir = 0, idx=0;
	while(*path != '\0') {
		path++;
		fileName[idx] = *path;
		if (*path == '/'){
			ndir += 1; 
			fileName[idx] = '\0';
			break;
		}
		idx++;
	}

	if (idx > MAX_FILE_NAME) return -ENOMEM;
	read_disk(&startInode, sizeof(struct Inode), SBLK->imapTable.addrTable[startInodeNum]);

	int permission = get_mode(startInode.uid, startInode.gid,  startInode.mode);
	if (permission < 04) return -EACCES;

	for (int i=0; i<startInode.numBLK; i++){
		read_disk(&dirbook, BLKSIZE, startInode.inodeAddrTable[i]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (strcmp(fileName, dirbook.fileNameTable[j]) == 0){
				if (ndir == 1) return get_inode_num(dirbook.inodeNumTable[j], path);
				return dirbook.inodeNumTable[j];
			}
		}
	}
	return -ENOENT;
}

void lfs_init()
{
	SBLK = (struct  LFS_superBlock*)malloc(sizeof(struct  LFS_superBlock));
	int fd;
	fd = open("log", O_RDWR|O_CREAT|O_EXCL, 0700);
	if (fd == -1){
		fd = open("log", O_RDWR);
        	pread(fd, SBLK, sizeof(struct LFS_superBlock), 0);
		SBLK->fd = fd;
	}
	else{
		char *tmp;
		tmp = malloc(LOG_SIZE);
		memset((void*)tmp,0,LOG_SIZE);
		pwrite(fd, tmp, LOG_SIZE, 0);
		free(tmp);
		SBLK->fd = fd;
		SBLK->inodeNum = 1;
		SBLK->logTail = sizeof(struct LFS_superBlock);
		tmp = malloc(BLKSIZE);
		memset((void*)tmp,0,BLKSIZE);
		write_to_disk(tmp, BLKSIZE, SBLK->logTail);
		free(tmp);
		// '/' inode
		tmp = malloc(sizeof(struct Inode));
		// Initially, no data block is allocated to dir, so size == 0
		newInode(tmp, getuid(), getgid(),S_IFDIR | 0755, 0, 2, 0, NULL);
		write_to_disk(tmp, sizeof(struct Inode), SBLK->logTail);
		free(tmp);
		// update super block
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		update_sblk(0, SBLK->logTail,ts,ts,ts);
		// Finally, push suBLK to disk
		pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	}
}

static int lfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	//printf("CREATE, <%s%s>, <%d>\n",mount_point, path, mode);
	int fileInodeNum = get_inode_num(0, path);
	if (check_not_exist(fileInodeNum)!=1) return check_not_exist(fileInodeNum);
	
	char p[MAX_FILE_NAME*10];
	memcpy(p, path, strlen(path)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(path)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = get_inode_num(0, p);
			break;
		}
	if (dirInodeNum == -ENOENT) return -ENOENT;
	
	struct Inode tmpInode, dirInode;
	struct DirtDataBLK buf;

	read_disk(&dirInode, sizeof(struct Inode), SBLK->imapTable.addrTable[dirInodeNum]);
	int permission = get_mode(dirInode.uid, dirInode.gid, dirInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03)
		return -EACCES;
	// new file's inode and write it into disk
	newInode(&tmpInode, getuid(), getgid(), mode, 0, 1, 0, NULL);
	write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
	//st_
	// insert new suBLK entry
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	update_sblk(SBLK->inodeNum, SBLK->logTail, ts,ts,ts);
	SBLK->inodeNum += 1;

	// Add new entry into dir table
	read_disk(&tmpInode, sizeof(struct Inode), SBLK->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	if (tmpInode.numBLK >= 1){
		read_disk(&buf, BLKSIZE, tmpInode.inodeAddrTable[tmpInode.numBLK-1]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			if (buf.fileNameTable[j][0] == 0){
				buf.inodeNumTable[j] = SBLK->inodeNum - 1;
				memcpy(buf.fileNameTable[j], path+suffix+1, strlen(path+suffix+1)+1);
				successInsert = 1;
				break;
			}
		}
	}
	if (successInsert == 1){
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = SBLK->logTail;
		write_to_disk(&buf, BLKSIZE, SBLK->logTail);
		write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
	}
	// New data block
	else{
		memset(&buf,0,BLKSIZE);
		buf.inodeNumTable[0] = SBLK->inodeNum - 1;
		memcpy(buf.fileNameTable[0], path+suffix+1, strlen(path+suffix+1)+1);
		tmpInode.numBLK += 1;
		tmpInode.size = tmpInode.numBLK*BLKSIZE; // dir's size update
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = SBLK->logTail;
		write_to_disk(&buf, BLKSIZE, SBLK->logTail);
		write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
		
	}
	update_sblk(dirInodeNum, SBLK->logTail, ts, ts, ts);
	// Finally, push suBLK to disk
	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return 0;
}


static int lfs_remove(const char *path){
	//printf("UNLINK, <%s%s>\n",mount_point, path);

	// find the path
	char p[MAX_FILE_NAME*10];
	memcpy(p, path, strlen(path)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(path)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = get_inode_num(0, p);
			break;
		}
	if (check_exist(dirInodeNum)!=1) return check_exist(dirInodeNum);

	struct Inode dirInode;
	struct DirtDataBLK buf;
	read_disk(&dirInode, sizeof(struct Inode), SBLK->imapTable.addrTable[dirInodeNum]);
	int permission = get_mode(dirInode.uid, dirInode.gid, dirInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03)
		return -EACCES;

	int fileInodeNum = get_inode_num(dirInodeNum, path+suffix);
	if (check_exist(fileInodeNum)!=1) return check_exist(fileInodeNum);
	struct Inode fileInode;
	read_disk(&fileInode, sizeof(struct Inode), SBLK->imapTable.addrTable[fileInodeNum]);
	/*	Not used since we use unlink to rm a dir's inode
	if (!S_ISREG(fileInode.mode)){
		return -EISDIR; // Not a file.
	}
	*/
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (fileInode.nLink > 1){
		fileInode.nLink -= 1;
		write_to_disk(&fileInode, sizeof(struct Inode), SBLK->logTail);
		update_sblk(fileInodeNum, SBLK->logTail, ts,
				  SBLK->imapTable.timeTable[fileInodeNum][1], SBLK->imapTable.timeTable[fileInodeNum][2]);
	}

	// Todo: Need to recycle unlinked entries
	// update dir's datablock and inode
	
	int successRm = 0, i;
	for (i = 0; i<dirInode.numBLK; i++){
		read_disk(&buf, BLKSIZE, dirInode.inodeAddrTable[i]);
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
	dirInode.inodeAddrTable[i] = SBLK->logTail;
	write_to_disk(&buf, BLKSIZE, SBLK->logTail);
	write_to_disk(&dirInode, sizeof(struct Inode), SBLK->logTail);

	// update super Block
	update_sblk(dirInodeNum, SBLK->logTail, ts,
				  ts, ts);
	// Finally, push suBLK to disk
	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return 0;
}

int dir_getattr(const char *path, struct stat *st, int baseInodeNum){
	//printf("*****%ld********\n", SBLK->logTail);
	int inodeNum = inodeNum = baseInodeNum;
	if (inodeNum == -ENOENT || inodeNum == -EACCES){
		st = NULL;
		return inodeNum;
	}
	if (inodeNum == -ENOMEM) return -ENOMEM;
	st->st_ino = inodeNum;
	st->st_ctim = SBLK->imapTable.timeTable[inodeNum][0];
	st->st_atim = SBLK->imapTable.timeTable[inodeNum][1];
	st->st_mtim = SBLK->imapTable.timeTable[inodeNum][2];
	struct Inode fileInode;
	read_disk(&fileInode, sizeof(struct Inode), SBLK->imapTable.addrTable[inodeNum]);
	st->st_uid = fileInode.uid;
	st->st_gid = fileInode.gid;
	st->st_mode = fileInode.mode;
	st->st_nlink = fileInode.nLink;
	st->st_size = fileInode.size;
	st->st_blksize = BLKSIZE;
	st->st_blocks = fileInode.numBLK;
	return 0;
}

static int lfs_getattr( const char *path, struct stat *st ){
	//printf("GETATTR, <%s%s>\n",mount_point, path);
	int inodeNum;
	inodeNum = get_inode_num(0, path);
	if (inodeNum == -ENOENT || inodeNum == -EACCES){
		st = NULL;
		return inodeNum;
	}
	if (inodeNum == -ENOMEM) return -ENOMEM;
	st->st_ino = inodeNum;
	st->st_ctim = SBLK->imapTable.timeTable[inodeNum][0];
	st->st_atim = SBLK->imapTable.timeTable[inodeNum][1];
	st->st_mtim = SBLK->imapTable.timeTable[inodeNum][2];
	struct Inode fileInode;
	read_disk(&fileInode, sizeof(struct Inode), SBLK->imapTable.addrTable[inodeNum]);
	st->st_uid = fileInode.uid;
	st->st_gid = fileInode.gid;
	st->st_mode = fileInode.mode;
	st->st_nlink = fileInode.nLink;
	st->st_size = fileInode.size;
	st->st_blksize = BLKSIZE;
	st->st_blocks = fileInode.numBLK;
	return 0;
}

static int lfs_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	//printf("READDIR, <%s%s>, <%x>,<%ld>\n",mount_point, path, buffer,offset);
	int dirInodeNum = get_inode_num(0, path);
	if (check_exist(dirInodeNum)!=1) return check_exist(dirInodeNum);
	struct Inode dirInode;
	read_disk(&dirInode, sizeof(struct Inode), SBLK->imapTable.addrTable[dirInodeNum]);
	if (!S_ISDIR(dirInode.mode)){
		return -ENOTDIR; // Not a dir.
	}

	int permission = get_mode(dirInode.uid, dirInode.gid,  dirInode.mode);
	if (permission < 04) return -EACCES;

	struct DirtDataBLK buf;
	struct stat statbuf;
	dir_getattr(NULL, &statbuf, dirInodeNum);
	filler( buffer, ".", &statbuf, 0 , 0); // Current Directory

	// Load files' attr
	for (int i=0; i<dirInode.numBLK; i++){
		read_disk(&buf, BLKSIZE, dirInode.inodeAddrTable[i]);
		for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			// 0 means after that no entry, -1 means the entry is removed 
			if (buf.fileNameTable[j][0] == 0 ) break;
			else if (buf.fileNameTable[j][0] != -1){
				dir_getattr(NULL, &statbuf, buf.inodeNumTable[j]);
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
		read_disk(buf, startpart, fileInode->inodeAddrTable[start]+BLKSIZE-startpart);
		int j=0;
		for (int i = start+1; i<end; i++, j++) read_disk(buf+j*BLKSIZE+startpart, BLKSIZE, fileInode->inodeAddrTable[i]);
		read_disk(buf+j*BLKSIZE+startpart, endpart, fileInode->inodeAddrTable[end]);
	}
	// [off,off+size] int the same data block
	else{
		read_disk(buf, size, fileInode->inodeAddrTable[start]+(offset-start*BLKSIZE));
	}
	memcpy(buffer, buf, size);
	free(buf);
	return size;
}

static int lfs_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi ){
	//printf("READ, <%s%s>, <%x>, <%ld>, <%ld>\n",mount_point, path, buffer, size, offset);
	int fileInodeNum = get_inode_num(0, path);
	if (check_exist(fileInodeNum)!=1) return check_exist(fileInodeNum);
	struct Inode fileInode;
	read_disk(&fileInode, sizeof(struct Inode), SBLK->imapTable.addrTable[fileInodeNum]);
	if (!S_ISREG(fileInode.mode)){
		return -EISDIR; // Not a file.
	}
	// update access time:
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	int permission = get_mode(fileInode.uid, fileInode.gid,  fileInode.mode);
	if (permission < 04) return -EACCES;
	update_time(fileInodeNum, SBLK->imapTable.timeTable[fileInodeNum][0], ts, SBLK->imapTable.timeTable[fileInodeNum][2]);
	return inodeBaseRead(&fileInode, buffer, size, offset);
}

static int lfs_write( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi ){
	//printf("WRITE, <%s%s>, <%x>, <%ld>, <%ld>\n", mount_point, path, buffer, size, offset);
	int fileInodeNum = get_inode_num(0, path);
	if (check_exist(fileInodeNum)!=1) return check_exist(fileInodeNum);

	struct Inode fileInode;
	read_disk(&fileInode, sizeof(struct Inode), SBLK->imapTable.addrTable[fileInodeNum]);
	if (!S_ISREG(fileInode.mode)){
		return -EISDIR; // Not a file.
	}
	if (offset>fileInode.size){
		return 0;	// Something wrong! No data written.
	}

	int permission = get_mode(fileInode.uid, fileInode.gid, fileInode.mode);
	if (permission != 07 && permission != 06 && permission !=02 && permission != 03) return -EACCES;

	int start = offset/BLKSIZE, end = (offset+size)/BLKSIZE, newBlkNum, noModified;
	if (end > MAX_FILE_SIZE_INBLK) return -EFBIG;
	
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
		fileInode.inodeAddrTable[i] = SBLK->logTail;
		write_to_disk(buf+(i-start)*BLKSIZE, BLKSIZE, SBLK->logTail);
	}
	write_to_disk(&fileInode, sizeof(struct Inode), SBLK->logTail);

	// update suBLK
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	update_sblk(fileInodeNum, SBLK->logTail, ts,
				  ts, ts);

	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return size;
}
static int lfs_mkdir(const char *path, mode_t mode){
	//printf("MKDIR, <%s%s>, <%d>\n",mount_point, path, mode);
	return lfs_create(path, mode|S_IFDIR, NULL); 
}

static int lfs_rmdir(const char *path){
	//printf("RMDIR, <%s%s>\n",mount_point, path);
	return lfs_remove(path);
}

static int lfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi){
	//printf("CHMOD, <%s%s>, <%d>\n",mount_point, path,mode);
	int fileInodeNum = get_inode_num(0, path);
	if (check_exist(fileInodeNum)!=1) return check_exist(fileInodeNum);
	struct Inode srcInode;
	read_disk(&srcInode, sizeof(struct Inode), SBLK->imapTable.addrTable[fileInodeNum]);
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
	write_to_disk(&srcInode, sizeof(struct Inode), SBLK->logTail);
	update_sblk(fileInodeNum, SBLK->logTail, ts,
				  SBLK->imapTable.timeTable[fileInodeNum][1], SBLK->imapTable.timeTable[fileInodeNum][2]);
	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return 0;
}
static int lfs_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	//printf("CHOWN, <%s%s>, <%d>, <%d>\n",mount_point, path, uid, gid);
	if (getuid() != 0)	return -EPERM; 	// Not root
	int inodeNum = get_inode_num(0, path);
	if (check_exist(inodeNum)!=1) return check_exist(inodeNum);
	struct Inode tmpInode;
	read_disk(&tmpInode, sizeof(struct Inode), SBLK->imapTable.addrTable[inodeNum]);
	if (uid != -1)	tmpInode.uid = uid;
	if (gid != -1)	tmpInode.gid = gid;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
	update_sblk(inodeNum, SBLK->logTail, ts,
				  SBLK->imapTable.timeTable[inodeNum][1], SBLK->imapTable.timeTable[inodeNum][2]);
	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return 0;
}

static int lfs_link(const char *srcPath, const char *destPath){
	//printf("LINK, <%s%s>, <%s%s>\n",mount_point, srcPath,mount_point, destPath);
	int srcInodeNum = get_inode_num(0, srcPath), destInodeNum = get_inode_num(0, destPath);
	if (srcInodeNum == -ENOENT)
		return -ENOENT;	// src file does not exist.
	if (destInodeNum != -ENOENT)
		return -EEXIST;	// dest file exists.
	if (srcInodeNum == -EACCES || destInodeNum == -EACCES)
		return -EACCES; // permission denied.
	if (srcInodeNum == -ENOMEM || destInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.

	struct Inode srcInode, tmpInode;
	read_disk(&srcInode, sizeof(struct Inode), SBLK->imapTable.addrTable[srcInodeNum]);

	// new src inode
	srcInode.nLink += 1;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	write_to_disk(&srcInode, sizeof(struct Inode), SBLK->logTail);
	update_sblk(srcInodeNum, SBLK->logTail, ts,
				  SBLK->imapTable.timeTable[srcInodeNum][1], SBLK->imapTable.timeTable[srcInodeNum][2]);

	char p[MAX_FILE_NAME*10];
	memcpy(p, destPath, strlen(destPath)+1);
	int dirInodeNum = 0, suffix = 0;
	for (suffix=strlen(destPath)-1; suffix>0; suffix--)
		if (*(p+suffix) == '/'){
			*(p+suffix) = '\0';
			dirInodeNum = get_inode_num(0, p);
			break;
		}
	read_disk(&tmpInode, sizeof(struct Inode), SBLK->imapTable.addrTable[dirInodeNum]);
	int successInsert = 0;
	struct DirtDataBLK buf;
	if (tmpInode.numBLK >= 1){
		read_disk(&buf, BLKSIZE, tmpInode.inodeAddrTable[tmpInode.numBLK-1]);
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
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = SBLK->logTail;
		write_to_disk(&buf, BLKSIZE, SBLK->logTail);
		write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
	}
	// New data block
	else{
		memset(&buf,0,BLKSIZE);
		buf.inodeNumTable[0] = srcInodeNum;
		memcpy(buf.fileNameTable[0], destPath+suffix+1, strlen(destPath+suffix+1)+1);
		tmpInode.numBLK += 1;
		tmpInode.size = tmpInode.numBLK*BLKSIZE; // dir's size update
		tmpInode.inodeAddrTable[tmpInode.numBLK-1] = SBLK->logTail;
		write_to_disk(&buf, BLKSIZE, SBLK->logTail);
		write_to_disk(&tmpInode, sizeof(struct Inode), SBLK->logTail);
	}
	update_sblk(dirInodeNum, SBLK->logTail, ts,
				  ts, ts);

	pwrite(SBLK->fd, SBLK, sizeof(struct LFS_superBlock), 0);
	return 0;
}

static int lfs_utimens (const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
	//printf("UTIMENS, <%s%s>\n",mount_point, path);
	int fileInodeNum = get_inode_num(0, path);
	if (check_exist(fileInodeNum)!=1) return check_exist(fileInodeNum); 

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	update_time(fileInodeNum, ts, tv[0], tv[1]);
	return 0;
}


static struct fuse_operations operations = {
	.create	= lfs_create,
	.unlink	= lfs_remove,
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.read		= lfs_read,
	.write		= lfs_write,
	.chmod		= lfs_chmod,
	.chown		= lfs_chown,
	.mkdir		= lfs_mkdir,
	.rmdir		= lfs_rmdir,
	.link		= lfs_link,
	.utimens	= lfs_utimens,
};

int main( int argc, char *argv[] )
{
	memcpy(mount_point, argv[argc-1], strlen(argv[argc-1])+1);
	lfs_init();
	return fuse_main(argc, argv, &operations, NULL);
}
