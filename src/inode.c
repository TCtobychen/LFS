#include "inode.h"

void newInode(struct Inode *buf, uid_t uid, uid_t gid, mode_t mode, 
			 size_t size, nlink_t nLink, int nBLK, long *addr){
	buf->uid = uid;
	buf->gid = gid;
	buf->mode = mode;
	buf->size = size;
	buf->nLink = nLink;
	buf->numBLK = nBLK;
	for (int i = 0; i<nBLK; i++)
		buf->direct[i] = *(addr+i);
}

