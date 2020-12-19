CC     = gcc
OBJS = src/lfs.c
PWD = $(shell pwd)
INC =-I$(PWD)/include
all: 
	mkdir bin
	$(CC) $(INC) $(OBJS) -o bin/lfs.o `pkg-config fuse3 --cflags --libs`
	$(CC) $(INC) src/block_dump.c -o bin/block_dump.o `pkg-config fuse3 --cflags --libs`

clean:
	rm -r bin
	rm log

