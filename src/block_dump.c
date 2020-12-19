#include "lfs.h"
#include "inode.h"

void readDisk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pread(SBLK -> fd, buf, size, offset);
}

int main(){
    struct  LFS_superBlock *SBLK = (struct  LFS_superBlock*)malloc(sizeof(struct  LFS_superBlock));
    int fd = open("log", O_RDWR);
    pread(fd, SBLK, sizeof(struct LFS_superBlock), 0);
	SBLK->fd = fd;
    printf("Total Inode Number: %d\n", SBLK -> inodeNum);
    printf("Log tail: %ld\n", SBLK->logTail);
    int inode_number, is_dir;
    struct Inode inode;
    struct DirtDataBLK bufdir;
    char buffile[BLKSIZE];
    for (inode_number = 0; inode_number < SBLK -> inodeNum; inode_number ++) {
        printf("\n");
        if (inode_number == -1) break;
        if (inode_number < 0 || inode_number >  SBLK->inodeNum-1)    continue;
        
        //printf("**************************************************************\n");
        printf("Address of Inode %d's log:\t%ld\n", inode_number, SBLK -> imapTable.addrTable[inode_number]);
        pread(fd, &inode, sizeof(struct Inode), SBLK -> imapTable.addrTable[inode_number]);
        is_dir = S_ISDIR(inode.mode);
        if (is_dir)
            printf("Directory/File: Directory\n");
        else
        printf("Directory/File: File\n");
        printf("gid:%d\n", inode.gid);
        printf("uid:%d\n", inode.uid);
        printf("mode:%o\n", inode.mode);
        printf("size:%ld\ndata blocks num:%d\nhard links:%ld\n", inode.size, inode.numBLK, inode.nLink);
        for (int i=0;i<inode.numBLK;i++){
            printf("Address of data block %d:%ld\n",i, inode.inodeAddrTable[i]);
            if (is_dir){
                pread(fd, &bufdir, sizeof(struct DirtDataBLK), inode.inodeAddrTable[i]);
                for (int j = 0; j < DIR_ENTRYNUM_BLK; j++){
			if (bufdir.fileNameTable[j][0] == 0 ) break;
			else if (bufdir.fileNameTable[j][0] != -1){
				printf("%s is under this directory. It's Inode number is: %d\n", bufdir.fileNameTable[j], bufdir.inodeNumTable[j]);
			}
		 }
            }
            else{
                pread(fd, &buffile, sizeof(struct DirtDataBLK), inode.inodeAddrTable[i]);
                printf("Content in Data block %d:\n%s", i, buffile);
            }
        }
        printf("\n");
    }
    return 0;
}
