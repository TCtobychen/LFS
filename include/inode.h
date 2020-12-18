/** @file   inode.h
 *
 * This file defines the inode data structure
 *
 */

#ifndef INODE_H
#define INODE_H

#include "LFS.h"

/**
 * record uid, gid, mode, size, number of data blocks, number of links, data blocks' address
 * time part information is saved in superBlock's imap to save operations making new inodes  
 */
struct Inode {
	uid_t uid;
	uid_t gid;
	mode_t mode;
	off_t size;
	int numBLK;
	nlink_t nLink;
	long direct[MAX_FILE_SIZE_INBLK];
};

/**
 * create set attributes of a new inode   
 */
void newInode(struct Inode *buf, uid_t uid, uid_t gid, mode_t mode,
			 size_t size, nlink_t nLink, int nBLK, long *addr);

#endif
