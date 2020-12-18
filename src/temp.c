static int do_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	printf("CHOWN, <%s%s>, <%d>, <%d>\n",mount_point, path, uid, gid);
	if (getuid() != 0)	return -EPERM; 	// Not root
	int fileInodeNum = findInodeNum(0, path);
	if (fileInodeNum == -ENOENT)
		return -ENOENT;	// src file does not exist.
	if (fileInodeNum == -ENOMEM)
		return -ENOMEM;	// too long file name.
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
