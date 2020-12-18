/*********************************************************************************************
 *   
 *                              LFS implemented using FUSE 3.1
 * 
 * @brief   Logic:       dir dataBLK entry             superBLK imap
 *              filename ------------------> inodeNum ------------------> inode ---> dataBLK
 * 
 * @author  Si Jiang
 *          Tsinghua University, IIIS
 *          2020-Fall, Operating system, proj3
 * 
 * @version 1.0 
 * 
 *********************************************************************************************/

/** @file   LSF.h
 *
 * This file defines the hyper parameters and superBlock data structure
 *
 */

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

/**
 * Max Log size
 */
#define LOG_SIZE            100*1024*1024

/**
 * Max length of filename
 */
#define MAX_FILE_NAME       124
#define MAX_PATH_LEN 512

/**
 * Max number of data blocks that a file can contain  
 */
#define MAX_FILE_SIZE_INBLK 1024*10     // 10 MB

/**
 * Data block size 
 */
#define BLKSIZE		1024

/**
 * Max number of inodes the file system can contain
 */
#define MAX_INODES	2048

/**
 * Number of entries in one dir data block
 */
#define DIR_ENTRYNUM_BLK    BLKSIZE/(MAX_FILE_NAME+sizeof(int))

#define MIN(a,b)	(a < b ? a : b)
#define MAX(a,b)	(a < b ? b : a)


/**
 * Record the disk addr according to inode number
 * Also record create, access, modify time, so we do not need to create new inode only when time part is changed 
 */
struct ImapTable {
    long addrTable[MAX_INODES];
    struct timespec timeTable[MAX_INODES][3];
};

/**
 * SuperBlock that record next inode number, log tail and imapTable
 * SuperBlock has pre-determined size, store in the head of the log
 * Each time the LFS is mounted, load the SuperBlock into Mem and modify it in the Mem
 * Only push the newest SuperBlock into the log after each operation is commited into the disk, this gurantees the transcation
 */
struct LFS_superBlock {
	int inodeNum;
	long logTail;
	struct ImapTable imapTable;
	/** 
     * the file descriptor of the file representing the disk log
     */
	int fd;	 
};

/**
 * The data structure used in dir's data block
 * record the name and corresponding inodeNum of files under the dir 
 */
struct DirtDataBLK {
    char fileNameTable[8][MAX_FILE_NAME];
    int inodeNumTable[8];
};

/**
 * Global controller: superBlock
 * Loaded from disk when initialization
 */
struct LFS_superBlock *suBlock;

// operations

/**
 * load data from the log
 * @param buf the buffer to store loaded data 
 * @param size bytes to be loaded 
 * @param offset begining byte in the log to be loaded 
 */
void readDisk(char *buf, size_t size, off_t offset);

/**
 * store data to the log
 * @param buf the buffer stored data
 * @param size bytes to be stored
 * @param offset begining byte in the log to be stored 
 */
void writeDisk(char *buf, size_t size, off_t offset);

/**
 * update inode time parts information in the superBlock imap
 * @param inodeNum idx of the inode to be updated
 * @param ctime new created time  
 * @param atime new accessed time  
 * @param mtime new modified time  
 */
void updateTime(int inodeNum, struct timespec ctime,struct timespec atime, struct timespec mtime);

/**
 * update inode in information the superBlock imap
 * @param inodeNum idx of the inode to be updated
 * @param inodeTail the END address of the inode in the disk
 * @param ctime new created time  
 * @param atime new accessed time  
 * @param mtime new modified time  
 */
void updateSuperBLK(int inodeNum, long inodeTail, struct timespec  ctime, struct timespec atime, struct timespec  mtime);

/**
 * find the inode number by given file(dir) name
 * @param baseInodeNum The dir inode where recursively search starts
 * @param suffix The file(dir) name 
 * 
 * @return inode number, or -ENOENT(-2) indicts file not found, or or -EACESS(-13) indicts no permission 
 */
int findInodeNum(int baseInodeNum, const char *suffix);

/**
 * find the permission bit of the current user
 * @param fileUid file's uid
 * @param fileGid file's gid
 * @param mode The file's mode
 * 
 * @return the permission bit (00-07)
 */
mode_t getMode(uid_t fileUid, uid_t fileGid,  mode_t mode);
/**
 * get attributes by given file(dir) name, using findInodeNum
 * @param path full path to the file(dir) 
 * @param st the buffer to store attributes
 * @param baseInodeNum The dir inode where recursively search starts in findInodeNum
 * @param isDirect if True, pass the search and use baseInodeNum as file's inode directly 
 * 
 * @return 0 if success, else return -ENOENT indicates file not found;
 */
int base_getattr(const char *path, struct stat *st, int baseInodeNum, int isDirect);

/**
 * Initialize the LFS
 * Load the log, if not exist, create a new log 
 */
void LFS_init();

// fuse-based operations
static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int do_remove(const char *path);
static int do_getattr( const char *path, struct stat *st );
static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi );
static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi );
static int do_write( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi );
static int do_mkdir(const char *path, mode_t mode);
static int do_rmdir(const char *path);
static int do_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
static int do_link(const char *srcPath, const char *destPath);
static int do_rename(const char *srcPath, const char *destPath, unsigned int flags);

#endif