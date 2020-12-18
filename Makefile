CC     = gcc
OBJS = src/inode.c src/LFS.c
PWD = $(shell pwd)
INC =-I$(PWD)/include
all: 
	mkdir bin
	$(CC) $(INC) $(OBJS) -o bin/lfs.o `pkg-config fuse3 --cflags --libs`

clean:
	rm -r bin
	rm -r log

