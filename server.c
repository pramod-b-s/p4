#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "mfs.h"
#include "udp.h"
#include "server.h"


int get_inode(int inodeNum, inode* n) {
	
	if(inodeNum < 0 || inodeNum >= MAX_NUM_INODES)			{
		printf("get_inode: invalid inodeNum\n");
		return -1;
	}
	
	int iblock = inodeMap[inodeNum];						
	lseek(fd, iblock*BLOCKSIZE, SEEK_SET);
	read(fd, n, sizeof(inode));

	return 0;
}

static int build_dir_block(int firstBlock, int inodeNum, int pinum)
{
	dirDataBlk dirBlk;
	int i;
	for(i = 0; i < MAX_INODE; i++)
	{
		dirBlk.inodeNums[i] = -1;
		strcpy(dirBlk.fileNames[i], "INVALID\0");
	}

	if(firstBlock)
	{
		dirBlk.inodeNums[0] = inodeNum;
		strcpy(dirBlk.fileNames[0], ".\0");
		dirBlk.inodeNums[1] = pinum;
		strcpy(dirBlk.fileNames[1], "..\0");
	}
	
	lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, &dirBlk, BLOCKSIZE);
	nextBlock++;

	return nextBlock-1;
}

static void update_CR(int dirty_inum)
{
	if(dirty_inum != -1)
	{
		lseek(fd, dirty_inum*sizeof(int), SEEK_SET);				
		write(fd, &inodeMap[dirty_inum], sizeof(int));
	}

	lseek(fd, MAX_NUM_INODES*sizeof(int), SEEK_SET);		
	write(fd, &nextBlock, sizeof(int));
}

int Server_Startup(int port, char* path) {
	
	if((fd = open(path, O_RDWR)) == -1) {
		fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(fd == -1) {
			return -1;
		}
		nextBlock = CRSIZE;

		int i;
		for(i = 0; i < MAX_NUM_INODES; i++) {
			inodeMap[i] = -1;
		}

		lseek(fd, 0, SEEK_SET);
		write(fd, inodeMap, sizeof(int)*MAX_NUM_INODES);
		write(fd, &nextBlock, sizeof(int));

		inode n;
		n.inodeNum = 0;
		n.size = BLOCKSIZE;
		n.type = MFS_DIRECTORY;
		n.used[0] = 1;
		n.blocks[0] = nextBlock;
		for(i = 1; i < MAX_DIRECT_PTRS; i++) {
			n.used[i] = 0;
			n.blocks[i] = -1;
		}

		dirDataBlk baseBlock;
		baseBlock.inodeNums[0] = 0;
		baseBlock.inodeNums[1] = 0;
		strcpy(baseBlock.fileNames[0], ".\0");
		strcpy(baseBlock.fileNames[1], "..\0");

		for(i = 2; i < MAX_INODE; i++) {
			baseBlock.inodeNums[i] = -1;
			strcpy(baseBlock.fileNames[i], "INVALID\0");
		}

		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
		write(fd, &baseBlock, sizeof(dirDataBlk));
		nextBlock++;
		
		inodeMap[0] = nextBlock;

		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
		write(fd, &n, sizeof(inode));
		nextBlock++;

		update_CR(0);
	}
	else {
		lseek(fd, 0, SEEK_SET);
		read(fd, inodeMap, sizeof(int)*MAX_NUM_INODES);
		read(fd, &nextBlock, sizeof(int));
	}

	int sd = UDP_Open(port);
	if(sd < 0) {
		printf("Error opening socket on port %d\n", port);
		exit(1);
	}

    printf("Starting server...\n");
    while (1) {
		struct sockaddr_in s;
		dataPkt packet;

		int rc = UDP_Read(sd, &s, (char *)&packet, sizeof(dataPkt));
		if (rc > 0) {
		    dataPkt responsePacket;

		    switch(packet.fsop){
		    		
		    	case LOOKUP :
		    		responsePacket.inodeNum = Server_Lookup(packet.inodeNum, packet.name);
		    		break;

		    	case STAT :
		    		responsePacket.inodeNum = Server_Stat(packet.inodeNum, &(responsePacket.stat));
		    		break;

		    	case WR :
		    		responsePacket.inodeNum = Server_Write(packet.inodeNum, packet.buffer, packet.block);
		    		break;

		    	case RD:
		    		responsePacket.inodeNum = Server_Read(packet.inodeNum, responsePacket.buffer, packet.block);
		    		break;

		    	case CREAT:
		    		responsePacket.inodeNum = Server_Creat(packet.inodeNum, packet.type, packet.name);
		    		break;

		    	case UNLINK:
		    		responsePacket.inodeNum = Server_Unlink(packet.inodeNum, packet.name);
		    		break;

		    	case EXIT:
		  			break;
		    	
		    	case RSP:
		    		break;
		    }

		    responsePacket.fsop = RSP;
		    rc = UDP_Write(sd, &s, (char*)&responsePacket, sizeof(dataPkt));
		    if(packet.fsop == EXIT) {
		    	Server_Shutdown();
			}
		}
	}

	return 0;
}

int Server_Lookup(int pinum, char *name) {
	inode parent;
	if(get_inode(pinum, &parent) == -1) {
		return -1;
	}

	for(int b = 0; b < MAX_DIRECT_PTRS; b++) {
		if(parent.used[b]) {
			dirDataBlk block;
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			for(int e = 0; e < MAX_INODE; e++) {
				if(block.inodeNums[e] != -1) {
					if(strcmp(name, block.fileNames[e]) == 0) {
						return block.inodeNums[e];
					}
				}
			}
		}
	}

	return -1;
}

int Server_Stat(int inodeNum, MFS_Stat_t *m) {
	
	inode n;
	if(get_inode(inodeNum, &n) == -1) {
		return -1;
	}

	m->type = n.type;
	m->size = n.size;

	return 0;
}

int Server_Write(int inodeNum, char *buffer, int block) {
	inode n; 
	if(get_inode(inodeNum, &n) == -1) {
		return -1;
	}
	
	if(n.type != MFS_REGULAR_FILE) {								
		return -1;
	}

	if(block < 0 || block >= MAX_DIRECT_PTRS) {
		return -1;
	}
	
		n.size = (block+1)*BLOCKSIZE > n.size ? (block+1)*BLOCKSIZE : n.size;
	n.used[block] = 1;
	
		n.blocks[block] = nextBlock+1;

		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, &n, BLOCKSIZE);
	inodeMap[inodeNum] = nextBlock;
	nextBlock++;
	
		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, buffer, BLOCKSIZE);
	nextBlock++;

		update_CR(inodeNum);
	return 0;
}

int Server_Read(int inodeNum, char *buffer, int block){
	inode n;
	if(get_inode(inodeNum, &n) == -1)
	{
		printf("get_inode failed for inodeNum %d.\n", inodeNum);
		return -1;
	}

	if(block < 0 || block >= MAX_DIRECT_PTRS || !n.used[block])			{
		printf("invalid block.\n");
		return -1;
	}

		if(n.type == MFS_REGULAR_FILE)																			{
		if(lseek(fd, n.blocks[block]*BLOCKSIZE, SEEK_SET) == -1)
		{
			perror("Server_Read: lseek:");
			printf("Server_Read: lseek failed\n");
		}
		
		if(read(fd, buffer, BLOCKSIZE) == -1)
		{
			perror("Server_Read: read:");
			printf("Server_Read: read failed\n");
		}
	}
	else																										{
		dirDataBlk dirBlk;																						lseek(fd, n.blocks[block], SEEK_SET);
		read(fd, &dirBlk, BLOCKSIZE);

		MFS_DirEnt_t entries[MAX_INODE];													int i;
		for(i = 0; i < MAX_INODE; i++)
		{
			MFS_DirEnt_t entry ;
			strcpy(entry.name, dirBlk.fileNames[i]);
			entry.inodeNum = dirBlk.inodeNums[i];
			entries[i] = entry;
		}

		memcpy(buffer, entries, sizeof(MFS_DirEnt_t)*MAX_INODE);
	}
	return 0;
}

int Server_Creat(int pinum, int type, char *name){
	if(Server_Lookup(pinum, name) != -1)							return 0;

	inode parent;
	if(get_inode(pinum, &parent) == -1)
		return -1;

	if(parent.type != MFS_DIRECTORY) {
		return -1;
	}
	
	int inodeNum = -1;
	for(int i = 0; i < MAX_NUM_INODES; i++)
	{
		if(inodeMap[i] == -1)
		{
			inodeNum = i;
			break;
		}
	}

	if(inodeNum == -1) {
		return -1;
	}

	dirDataBlk block;
	for(int b = 0; b < MAX_DIRECT_PTRS; b++)
	{
		if(parent.used[b])
		{
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			for(int e = 0; e < MAX_INODE; e++)
			{
				if(block.inodeNums[e] == -1)
				{
					lseek(fd, inodeMap[pinum]*BLOCKSIZE, SEEK_SET);
					write(fd, &parent, BLOCKSIZE);

					block.inodeNums[e] = inodeNum;
					strcpy(block.fileNames[e], name);
					lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
					write(fd, &block, BLOCKSIZE);

					inode n;
					n.inodeNum = inodeNum;
					n.size = 0;
					for(int i = 0; i < MAX_DIRECT_PTRS; i++)
					{
						n.used[i] = 0;
						n.blocks[i] = -1;
					}
					n.type = type;	
					if(type == MFS_DIRECTORY)
					{
						n.used[0] = 1;
						n.blocks[0] = nextBlock;
						
						build_dir_block(1, inodeNum, pinum);

						n.size += BLOCKSIZE;
					}
					else if (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE)
					{
						return -1;
					}

					inodeMap[inodeNum] = nextBlock;

					lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
					write(fd, &n, sizeof(inode));
					nextBlock++;

					update_CR(inodeNum);

					return 0;
				}
			}
		}
		else
		{
						int bl = build_dir_block(0, inodeNum, -1);
			parent.size += BLOCKSIZE;

			parent.used[b] = 1;
			parent.blocks[b] = bl;
			b--;
		}
	}

	return -1;
}

int Server_Unlink(int pinum, char *name){
	
	inode toRemove;						inode parent;						
	if(get_inode(pinum, &parent) == -1)					return -1;

	int inodeNum = Server_Lookup(pinum, name);		if(get_inode(inodeNum, &toRemove) == -1)				return 0;

		if(toRemove.type == MFS_DIRECTORY)
	{
		int b;
		for(b = 0; b < MAX_DIRECT_PTRS; b++)
		{
			if(toRemove.used[b])
			{
				dirDataBlk block;
				lseek(fd, toRemove.blocks[b]*BLOCKSIZE, SEEK_SET);
				read(fd, &block, BLOCKSIZE);

				int e;
				for(e = 0; e < MAX_INODE; e++)
				{
					if(block.inodeNums[e] != -1 && strcmp(block.fileNames[e], ".") != 0 && strcmp(block.fileNames[e], "..") != 0)
					{
						return -1;						}
				}
			}
		}
	}
	
	int found = 0;
	int b;
	for(b = 0; b < MAX_DIRECT_PTRS && !found; b++)
	{
		if(parent.used[b])
		{
			dirDataBlk block;
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			int e;
			for(e = 0; e < MAX_INODE && !found; e++)
			{
				if(block.inodeNums[e] != -1)
				{
					if(strcmp(name, block.fileNames[e]) == 0)
					{
						block.inodeNums[e] = -1;
						strcpy(block.fileNames[e], "INVALID");
						found = 1;
					}
				}
			}

			if(found)
			{
				lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
				write(fd, &block, BLOCKSIZE);
				nextBlock++;

				parent.blocks[b] = nextBlock-1;
				lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
				write(fd, &parent, BLOCKSIZE);
				nextBlock++;

				inodeMap[pinum] = nextBlock-1;
				update_CR(pinum);
			}
		}
	}

	inodeMap[inodeNum] = -1;
	update_CR(inodeNum);

	return 0;
}

int Server_Shutdown()
{
	fsync(fd);				exit(0);
	return -1;	}

int main(int argc, char *argv[])
{
	if(argc != 3) {
		printf("Usage: server [portnum] [file-system image]\n");
		exit(1);
	}

	int portNumber = atoi(argv[1]);
	char *fileSysPath = argv[2];

	Server_Startup(portNumber, fileSysPath);

	return 0;
}
