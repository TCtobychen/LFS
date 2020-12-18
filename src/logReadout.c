#include "LFS.h"
#include "inode.h"

void readDisk(char *buf, size_t size, off_t offset){
	if (size == 0)	return;
	pread(suBlock->fd, buf, size, offset);
}

int main(){
    struct  LFS_superBlock *suBlock = (struct  LFS_superBlock*)malloc(sizeof(struct  LFS_superBlock));
    int fd = open("../log/LFS_Log", O_RDWR);
    pread(fd, suBlock, sizeof(struct LFS_superBlock), 0);
	suBlock->fd = fd;
    // print superBlock
    printf("Total inode Number:\t%d\n", suBlock->inodeNum);
    printf("Current log tail(in bytes):\t%ld\n", suBlock->logTail);
    int iNum, isDir;
    struct Inode inode;
    struct DirtDataBLK bufdir;
    char buffile[BLKSIZE];
    while(1) {
        printf("Input inode Number (0~%d, -1 to quit):", suBlock->inodeNum-1);
        scanf("%d", &iNum);
        printf("\n");
        if (iNum == -1) break;
        if (iNum < 0 || iNum >  suBlock->inodeNum-1)    continue;
        
        //printf("**************************************************************\n");
        printf("Inode %d's log addr(in bytes):\t%ld\n", iNum, suBlock->imapTable.addrTable[iNum]);
        pread(fd, &inode, sizeof(struct Inode), suBlock->imapTable.addrTable[iNum]);
        isDir = S_ISDIR(inode.mode);
        if (isDir)
            printf("\t@@@@@@@@  This is a dir @@@@@@@@\n");
        else
            printf("\t@@@@@@@@ This is a file @@@@@@@@\n");
        printf("uid:%d\t\tgid:%d\t\tmode:%o\n", inode.uid, inode.gid, inode.mode);
        printf("size:%ld\t\tdata blocks num:%d\thard links:%ld\n", inode.size, inode.numBLK, inode.nLink);
        for (int i=0;i<inode.numBLK;i++){
            printf("data block %d's addr:%ld\n",i, inode.inodeAddrTable[i]);
            if (isDir){
                pread(fd, &bufdir, sizeof(struct DirtDataBLK), inode.inodeAddrTable[i]);
                for (int j = 0;j<DIR_ENTRYNUM_BLK; j++){
			        // 0 means after that no entry, -1 means the entry is removed 
			        if (bufdir.fileNameTable[j][0] == 0 ) break;
			        else if (bufdir.fileNameTable[j][0] != -1){
				        printf("\t------%s is under this dir with inode num: %d\n", bufdir.fileNameTable[j], bufdir.inodeNumTable[j]);
			        }
		        }
            }
            else{
                pread(fd, &buffile, sizeof(struct DirtDataBLK), inode.inodeAddrTable[i]);
                printf("\t ---data block %d's content:\n %s\n", i, buffile);
            }
        }
        printf("**************************************************************\n\n");
    }
    return 0;
}