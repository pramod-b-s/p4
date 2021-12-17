#include "mfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udp.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>


typedef struct __buf {
	char string [BLOCKSIZE/sizeof(char)];
} buf;
int newFS;

int imap[NINODES];			// block number of each inode
int nextBlock;					// next block in the address space to be written
int fd;										// the file descriptor of the LFS

int get_inode(int inum, inode* n) {
	
	if(inum < 0 || inum >= NINODES)		// check for invalid inum
	{
		printf("get_inode: invalid inum\n");
		return -1;
	}
	
	int iblock = imap[inum];					// block where desired inode is written
	
	lseek(fd, iblock*BLOCKSIZE, SEEK_SET);
	read(fd, n, sizeof(inode));

	return 0;
}

// Returns block number of new block
// pinum is unused if firstBlock == 0
int build_dir_block(int firstBlock, int inum, int pinum)
{
	dirBlock db;
	int i;
	for(i = 0; i < NENTRIES; i++)
	{
		db.inums[i] = -1;
		strcpy(db.names[i], "DNE\0");
	}

	if(firstBlock)
	{
		db.inums[0] = inum;
		strcpy(db.names[0], ".\0");
		db.inums[1] = pinum;
		strcpy(db.names[1], "..\0");
	}
	
	// write new block
	lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, &db, BLOCKSIZE);
	nextBlock++;

	return nextBlock-1;
}

void update_CR(int dirty_inum)
{
	if(dirty_inum != -1)
	{
		lseek(fd, dirty_inum*sizeof(int), SEEK_SET);		// update inode table
		write(fd, &imap[dirty_inum], sizeof(int));
	}

	lseek(fd, NINODES*sizeof(int), SEEK_SET);	// update nextBlock
	write(fd, &nextBlock, sizeof(int));
}

int Server_Startup(int port, char* path) {
	
	if((fd = open(path, O_RDWR)) == -1)
	{
		newFS = 1;

		// create new file system
		fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(fd == -1)
			return -1;
		nextBlock = CRSIZE;

		int i;
		for(i = 0; i < NINODES; i++)
		{
			imap[i] = -1;
		}

		lseek(fd, 0, SEEK_SET);
		write(fd, imap, sizeof(int)*NINODES);
		write(fd, &nextBlock, sizeof(int));

		inode n;
		n.inum = 0;
		n.size = BLOCKSIZE;
		n.type = MFS_DIRECTORY;
		n.used[0] = 1;
		n.blocks[0] = nextBlock;
		for(i = 1; i < NBLOCKS; i++)
		{
			n.used[i] = 0;
			n.blocks[i] = -1;
		}

		dirBlock baseBlock;
		baseBlock.inums[0] = 0;
		baseBlock.inums[1] = 0;
		strcpy(baseBlock.names[0], ".\0");
		strcpy(baseBlock.names[1], "..\0");

		for(i = 2; i < NENTRIES; i++)
		{
			baseBlock.inums[i] = -1;
			strcpy(baseBlock.names[i], "DNE\0");
		}

		// write baseBlock
		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
		write(fd, &baseBlock, sizeof(dirBlock));
		nextBlock++;
		
		// update imap
		imap[0] = nextBlock;

		// write inode
		lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
		write(fd, &n, sizeof(inode));
		nextBlock++;

		// write checkpoint region
		update_CR(0);
	}
	else
	{
		newFS = 0;

		lseek(fd, 0, SEEK_SET);
		read(fd, imap, sizeof(int)*NINODES);
		read(fd, &nextBlock, sizeof(int));
	}

	//	TODO: remove comment here
		int sd = UDP_Open(port);
	if(sd < 0)
	{
		printf("Error opening socket on port %d\n", port);
		exit(1);
	}

    printf("Starting server...\n");
    while (1) {
		struct sockaddr_in s;
		Net_Packet packet;
		int rc = UDP_Read(sd, &s, (char *)&packet, sizeof(Net_Packet));
		if (rc > 0) {
		    Net_Packet responsePacket;

		    switch(packet.message){
		    		
		    	case PAK_LOOKUP :
		    		responsePacket.inum = Server_Lookup(packet.inum, packet.name);
		    		break;

		    	case PAK_STAT :
		    		responsePacket.inum = Server_Stat(packet.inum, &(responsePacket.stat));
		    		break;

		    	case PAK_WRITE :
		    		responsePacket.inum = Server_Write(packet.inum, packet.buffer, packet.block);
		    		break;

		    	case PAK_READ:
		    		responsePacket.inum = Server_Read(packet.inum, responsePacket.buffer, packet.block);
		    		break;

		    	case PAK_CREAT:
		    		responsePacket.inum = Server_Creat(packet.inum, packet.type, packet.name);
		    		break;

		    	case PAK_UNLINK:
		    		responsePacket.inum = Server_Unlink(packet.inum, packet.name);
		    		break;

		    	case PAK_SHUTDOWN:
		  			break;
		    	
		    	case PAK_RESPONSE:
		    		break;
		    }

		    responsePacket.message = PAK_RESPONSE;
		    rc = UDP_Write(sd, &s, (char*)&responsePacket, sizeof(Net_Packet));
		    if(packet.message == PAK_SHUTDOWN)
		    	Server_Shutdown();
		}
	}
	return 0;
}

int Server_Lookup(int pinum, char *name) {
	
	inode parent;
	if(get_inode(pinum, &parent) == -1)
	{
		return -1;
	}

	int b;
	for(b = 0; b < NBLOCKS; b++)
	{
		if(parent.used[b])
		{
			dirBlock block;
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			int e;
			for(e = 0; e < NENTRIES; e++)
			{
				if(block.inums[e] != -1)
				{
					if(strcmp(name, block.names[e]) == 0)
					{
						return block.inums[e];
					}
				}
			}
		}
	}

	return -1;
}

int Server_Stat(int inum, MFS_Stat_t *m) {
	
	inode n;
	if(get_inode(inum, &n) == -1)
		return -1;

	m->type = n.type;
	m->size = n.size;

	return 0;
}

int Server_Write(int inum, char *buffer, int block) {
	inode n; 
	if(get_inode(inum, &n) == -1)
		return -1;
	
	if(n.type != MFS_REGULAR_FILE)								// can't write to directory
		return -1;

	if(block < 0 || block >= NBLOCKS)	// check for invalid block
		return -1;
	
	// update file size
	n.size = (block+1)*BLOCKSIZE > n.size ? (block+1)*BLOCKSIZE : n.size;
	n.used[block] = 1;
	
	// inform inode of location of new block
	n.blocks[block] = nextBlock+1;

	// write inode chunk
	lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, &n, BLOCKSIZE);
	imap[inum] = nextBlock;
	nextBlock++;
	
	// write buffer
	lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, buffer, BLOCKSIZE);
	nextBlock++;

	// write checkpoint region
	update_CR(inum);
	return 0;
}

int Server_Read(int inum, char *buffer, int block){
	inode n;
	if(get_inode(inum, &n) == -1)
	{
		printf("get_inode failed for inum %d.\n", inum);
		return -1;
	}

	if(block < 0 || block >= NBLOCKS || !n.used[block])		// check for invalid block
	{
		printf("invalid block.\n");
		return -1;
	}

	// read
	if(n.type == MFS_REGULAR_FILE)																		// read regular file
	{
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
	else																									// read directory
	{
		dirBlock db;																				// read dirBlock
		lseek(fd, n.blocks[block], SEEK_SET);
		read(fd, &db, BLOCKSIZE);

		MFS_DirEnt_t entries[NENTRIES];											// convert dirBlock to MRS_DirEnt_t
		int i;
		for(i = 0; i < NENTRIES; i++)
		{
			MFS_DirEnt_t entry ;
			strcpy(entry.name, db.names[i]);
			entry.inum = db.inums[i];
			entries[i] = entry;
		}

		memcpy(buffer, entries, sizeof(MFS_DirEnt_t)*NENTRIES);
	}
	return 0;
}

int Server_Creat(int pinum, int type, char *name){
	if(Server_Lookup(pinum, name) != -1)					// if the file already exists, return success
		return 0;

	inode parent;
	if(get_inode(pinum, &parent) == -1)
		return -1;

	if(parent.type != MFS_DIRECTORY)												// if parent directory is not a directory, return failure
		return -1;
	
	// find lowest available inum
	int inum = -1;
	int i;
	for(i = 0; i < NINODES; i++)
	{
		if(imap[i] == -1)
		{
			inum = i;
			break;
		}
	}

	if(inum == -1)			// if more than NINODES inodes exist, return failure
		return -1;

	// put inode into parent directory
	int b, e; dirBlock block;
	for(b = 0; b < NBLOCKS; b++)
	{
		if(parent.used[b])
		{
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			if(b>0)
			{
				//printf("About to examine the following block:\n");
				//print_dirBlock(parent.blocks[b]);
			}
			for(e = 0; e < NENTRIES; e++)
			{
				if(block.inums[e] == -1)
				{
					//printf("Chose block %d and entry %d\n", b, e);
					goto found_parent_slot;
				}
			}
		}
		else
		{
			// make new block, then repeat loop on this block
			int bl = build_dir_block(0, inum, -1);
			parent.size += BLOCKSIZE;

			parent.used[b] = 1;
			parent.blocks[b] = bl;
			b--;
		}
	}

	return -1;						// directory is full

	found_parent_slot:
	
	// write parent
	lseek(fd, imap[pinum]*BLOCKSIZE, SEEK_SET);
	write(fd, &parent, BLOCKSIZE);

	block.inums[e] = inum;
	strcpy(block.names[e], name);
	lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
	write(fd, &block, BLOCKSIZE);

	// create inode
	inode n;
	n.inum = inum;
	n.size = 0;
	for(i = 0; i < NBLOCKS; i++)
	{
		n.used[i] = 0;
		n.blocks[i] = -1;
	}
	n.type = type;	
	if(type == MFS_DIRECTORY)
	{
		n.used[0] = 1;
		n.blocks[0] = nextBlock;
		
		build_dir_block(1, inum, pinum);

		// update file size
		n.size += BLOCKSIZE;
	}
	else if (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE)
	{
		return -1;
	}

	// update imap
	imap[inum] = nextBlock;

	// write inode
	lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
	write(fd, &n, sizeof(inode));
	nextBlock++;

	// write checkpoint region
	update_CR(inum);

	return 0;
}

int Server_Unlink(int pinum, char *name){
	
	inode toRemove;					// to be removed
	inode parent;						// parent of toRemove

	if(get_inode(pinum, &parent) == -1)			// parent directory doesn't exist; return failure
		return -1;

	int inum = Server_Lookup(pinum, name);	// inum of toRemove
	if(get_inode(inum, &toRemove) == -1)		// toRemove doesn't exist; return success
		return 0;

	// if toRemove is a directory, make sure it's empty
	if(toRemove.type == MFS_DIRECTORY)
	{
		int b;
		for(b = 0; b < NBLOCKS; b++)
		{
			if(toRemove.used[b])
			{
				dirBlock block;
				lseek(fd, toRemove.blocks[b]*BLOCKSIZE, SEEK_SET);
				read(fd, &block, BLOCKSIZE);

				int e;
				for(e = 0; e < NENTRIES; e++)
				{
					if(block.inums[e] != -1 && strcmp(block.names[e], ".") != 0 && strcmp(block.names[e], "..") != 0)
					{
						return -1;	// found file in toRemove
					}
				}
			}
		}
	}
	
	// remove toRemove from parent
	int found = 0;
	int b;
	for(b = 0; b < NBLOCKS && !found; b++)
	{
		if(parent.used[b])
		{
			dirBlock block;
			lseek(fd, parent.blocks[b]*BLOCKSIZE, SEEK_SET);
			read(fd, &block, BLOCKSIZE);

			int e;
			for(e = 0; e < NENTRIES && !found; e++)
			{
				if(block.inums[e] != -1)
				{
					if(strcmp(name, block.names[e]) == 0)
					{
						block.inums[e] = -1;
						strcpy(block.names[e], "DNE");
						found = 1;
					}
				}
			}

			if(found)
			{
				// rewrite this block of parent
				lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
				write(fd, &block, BLOCKSIZE);
				nextBlock++;

				// inform parent inode of new block location
				parent.blocks[b] = nextBlock-1;
				lseek(fd, nextBlock*BLOCKSIZE, SEEK_SET);
				write(fd, &parent, BLOCKSIZE);
				nextBlock++;

				// update imap
				imap[pinum] = nextBlock-1;
				update_CR(pinum);
			}
		}
	}

	// remove toRemove from CR
	imap[inum] = -1;
	update_CR(inum);

	return 0;
}

int Server_Shutdown()
{
	fsync(fd);			// not sure if this is necessary
	exit(0);
	return -1;	// if we reach this line of code, there was an error
}

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
