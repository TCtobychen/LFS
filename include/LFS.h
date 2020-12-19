#ifndef LFS_H
#define LFS_H

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define LOG_SIZE            100*1024*1024
#define MAX_FILE_NAME       124
#define MAX_FILE_SIZE_INBLK 1024*10  
#define BLKSIZE		1024
#define MAX_INODES	2048
#define DIR_ENTRYNUM_BLK    BLKSIZE/(MAX_FILE_NAME+sizeof(int))

#define MIN(a,b)	(a < b ? a : b)
#define MAX(a,b)	(a < b ? b : a)

struct ImapTable {
    long addrTable[MAX_INODES];
    struct timespec timeTable[MAX_INODES][3];
};
struct LFS_superBlock {
	int inodeNum;
	long logTail;
	struct ImapTable imapTable;
	int fd;	 
};

struct DirtDataBLK {
    char fileNameTable[8][MAX_FILE_NAME];
    int inodeNumTable[8];
};
struct LFS_superBlock *SBLK;

void read_disk(char *buf, size_t size, off_t offset);
void write_to_disk(char *buf, size_t size, off_t offset);
void update_time(int inodeNum, struct timespec ctime,struct timespec atime, struct timespec mtime);
void update_sblk(int inodeNum, long inodeTail, struct timespec  ctime, struct timespec atime, struct timespec  mtime);
int get_inode_num(int baseInodeNum, const char *suffix);
mode_t get_mode(uid_t fileUid, uid_t fileGid,  mode_t mode);
int dir_getattr(const char *path, struct stat *st, int baseInodeNum);
void lfs_init();

static int lfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int lfs_remove(const char *path);
static int lfs_getattr( const char *path, struct stat *st );
static int lfs_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi );
static int lfs_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi );
static int lfs_write( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi );
static int lfs_mkdir(const char *path, mode_t mode);
static int lfs_rmdir(const char *path);
static int lfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
static int lfs_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
static int lfs_link(const char *srcPath, const char *destPath);

#endif
