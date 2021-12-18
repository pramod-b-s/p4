#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mfs.h"
#include "udp.h"
#include "server.h"

int get_inode(int inodeNum, inode *n)
{
	if (inodeNum < 0 || inodeNum >= MAX_NUM_INODES)
	{
		return -1;
	}

	int iblock = inodeMap[inodeNum];
	lseek(fd, iblock * MAX_BLK_SIZE, SEEK_SET);
	read(fd, n, sizeof(inode));

	return 0;
}

static int build_dir_block(int firstBlock, int inodeNum, int pinum)
{
	dirDataBlk dirBlk;
	int i;
	for (i = 0; i < MAX_INODE; i++)
	{
		dirBlk.inodeNums[i] = -1;
		strcpy(dirBlk.fileNames[i], "INVALID\0");
	}

	if (firstBlock)
	{
		dirBlk.inodeNums[0] = inodeNum;
		strcpy(dirBlk.fileNames[0], ".\0");
		dirBlk.inodeNums[1] = pinum;
		strcpy(dirBlk.fileNames[1], "..\0");
	}

	lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
	write(fd, &dirBlk, MAX_BLK_SIZE);
	nextBlock++;

	return nextBlock - 1;
}

static void update_checkpoint(int dirty_inum)
{
	if (dirty_inum != -1)
	{
		lseek(fd, dirty_inum * sizeof(int), SEEK_SET);
		write(fd, &inodeMap[dirty_inum], sizeof(int));
	}

	lseek(fd, MAX_NUM_INODES * sizeof(int), SEEK_SET);
	write(fd, &nextBlock, sizeof(int));
}

int Startup(int port, char *path)
{
	if ((fd = open(path, O_RDWR)) == -1)
	{
		fd = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
		if (fd == -1)
		{
			return -1;
		}
		nextBlock = 6;

		for (int i = 0; i < MAX_NUM_INODES; i++)
		{
			inodeMap[i] = -1;
		}

		lseek(fd, 0, SEEK_SET);
		write(fd, inodeMap, sizeof(int) * MAX_NUM_INODES);
		write(fd, &nextBlock, sizeof(int));

		inode ind;
		ind.inodeNum = 0;
		ind.size = MAX_BLK_SIZE;
		ind.type = MFS_DIRECTORY;
		ind.used[0] = 1;
		ind.blocks[0] = nextBlock;
		for (int i = 1; i < MAX_DIRECT_PTRS; i++)
		{
			ind.used[i] = 0;
			ind.blocks[i] = -1;
		}

		dirDataBlk baseBlock;
		baseBlock.inodeNums[0] = 0;
		baseBlock.inodeNums[1] = 0;
		strcpy(baseBlock.fileNames[0], ".\0");
		strcpy(baseBlock.fileNames[1], "..\0");

		for (int i = 2; i < MAX_INODE; i++)
		{
			baseBlock.inodeNums[i] = -1;
			strcpy(baseBlock.fileNames[i], "INVALID\0");
		}

		lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
		write(fd, &baseBlock, sizeof(dirDataBlk));
		nextBlock++;

		inodeMap[0] = nextBlock;

		lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
		write(fd, &ind, sizeof(inode));
		nextBlock++;

		update_checkpoint(0);
	}
	else
	{
		lseek(fd, 0, SEEK_SET);
		read(fd, inodeMap, sizeof(int) * MAX_NUM_INODES);
		read(fd, &nextBlock, sizeof(int));
	}

	int sd = UDP_Open(port);
	if (sd < 0) // Could not open port
	{
		exit(1);
	}

	while (1)
	{
		struct sockaddr_in s;
		dataPkt packet;

		int rc = UDP_Read(sd, &s, (char *)&packet, sizeof(dataPkt));
		if (rc > 0)
		{
			dataPkt responsePacket;

			switch (packet.fsop)
			{
			case LOOKUP:
				responsePacket.inodeNum = Lookup(packet.inodeNum, packet.name);
				break;

			case STAT:
				responsePacket.inodeNum = Stat(packet.inodeNum, &(responsePacket.stat));
				break;

			case WR:
				responsePacket.inodeNum = Write(packet.inodeNum, packet.buffer, packet.block);
				break;

			case RD:
				responsePacket.inodeNum = Read(packet.inodeNum, responsePacket.buffer, packet.block);
				break;

			case CREAT:
				responsePacket.inodeNum = Creat(packet.inodeNum, packet.type, packet.name);
				break;

			case UNLINK:
				responsePacket.inodeNum = Unlink(packet.inodeNum, packet.name);
				break;

			case EXIT:
				break;

			case RSP:
				break;
			}

			responsePacket.fsop = RSP;
			rc = UDP_Write(sd, &s, (char *)&responsePacket, sizeof(dataPkt));
			if (packet.fsop == EXIT)
			{
				Shutdown();
			}
		}
	}

	return 0;
}

int Lookup(int pinum, char *name)
{
	inode parent;
	if (get_inode(pinum, &parent) == -1)
	{
		return -1;
	}

	for (int b = 0; b < MAX_DIRECT_PTRS; b++)
	{
		if (parent.used[b])
		{
			dirDataBlk block;
			lseek(fd, parent.blocks[b] * MAX_BLK_SIZE, SEEK_SET);
			read(fd, &block, MAX_BLK_SIZE);

			for (int e = 0; e < MAX_INODE; e++)
			{
				if (block.inodeNums[e] != -1)
				{
					if (strcmp(name, block.fileNames[e]) == 0)
					{
						return block.inodeNums[e];
					}
				}
			}
		}
	}

	return -1;
}

int Stat(int inodeNum, MFS_Stat_t *m)
{
	inode ind;
	if (get_inode(inodeNum, &ind) == -1)
	{
		return -1;
	}

	m->type = ind.type;
	m->size = ind.size;

	return 0;
}

int Write(int inodeNum, char *buffer, int block)
{
	inode ind;
	if (get_inode(inodeNum, &ind) == -1)
	{
		return -1;
	}

	if (ind.type != MFS_REGULAR_FILE)
	{
		return -1;
	}

	if (block < 0 || block >= MAX_DIRECT_PTRS)
	{
		return -1;
	}

	ind.size = (block + 1) * MAX_BLK_SIZE > ind.size ? (block + 1) * MAX_BLK_SIZE : ind.size;
	ind.used[block] = 1;

	ind.blocks[block] = nextBlock + 1;

	lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
	write(fd, &ind, MAX_BLK_SIZE);
	inodeMap[inodeNum] = nextBlock;
	nextBlock++;

	lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
	write(fd, buffer, MAX_BLK_SIZE);
	nextBlock++;

	update_checkpoint(inodeNum);
	return 0;
}

int Read(int inodeNum, char *buffer, int block)
{
	inode ind;
	if (get_inode(inodeNum, &ind) == -1)
	{
		return -1;
	}

	if (block < 0 || block >= MAX_DIRECT_PTRS || !ind.used[block]) // invalid
	{
		return -1;
	}

	if (ind.type == MFS_REGULAR_FILE)
	{
		if (lseek(fd, ind.blocks[block] * MAX_BLK_SIZE, SEEK_SET) == -1)
		{
			perror("Failed on lseek");
		}

		if (read(fd, buffer, MAX_BLK_SIZE) == -1)
		{
			perror("Failed on read");
		}
	}
	else
	{
		dirDataBlk dirBlk;
		lseek(fd, ind.blocks[block], SEEK_SET);
		read(fd, &dirBlk, MAX_BLK_SIZE);

		MFS_DirEnt_t entries[MAX_INODE];
		int i;
		for (i = 0; i < MAX_INODE; i++)
		{
			MFS_DirEnt_t entry;
			strcpy(entry.name, dirBlk.fileNames[i]);
			entry.inodeNum = dirBlk.inodeNums[i];
			entries[i] = entry;
		}

		memcpy(buffer, entries, sizeof(MFS_DirEnt_t) * MAX_INODE);
	}
	return 0;
}

int Creat(int pinum, int type, char *name)
{
	if (Lookup(pinum, name) != -1)
	{
		return 0;
	}

	inode parent;
	if (get_inode(pinum, &parent) == -1)
	{
		return -1;
	}

	if (parent.type != MFS_DIRECTORY)
	{
		return -1;
	}

	int inodeNum = -1;
	for (int i = 0; i < MAX_NUM_INODES; i++)
	{
		if (inodeMap[i] == -1)
		{
			inodeNum = i;
			break;
		}
	}

	if (inodeNum == -1)
	{
		return -1;
	}

	dirDataBlk block;
	for (int b = 0; b < MAX_DIRECT_PTRS; b++)
	{
		if (parent.used[b])
		{
			lseek(fd, parent.blocks[b] * MAX_BLK_SIZE, SEEK_SET);
			read(fd, &block, MAX_BLK_SIZE);

			for (int e = 0; e < MAX_INODE; e++)
			{
				if (block.inodeNums[e] == -1)
				{
					lseek(fd, inodeMap[pinum] * MAX_BLK_SIZE, SEEK_SET);
					write(fd, &parent, MAX_BLK_SIZE);

					block.inodeNums[e] = inodeNum;
					strcpy(block.fileNames[e], name);
					lseek(fd, parent.blocks[b] * MAX_BLK_SIZE, SEEK_SET);
					write(fd, &block, MAX_BLK_SIZE);

					inode ind;
					ind.inodeNum = inodeNum;
					ind.size = 0;
					for (int i = 0; i < MAX_DIRECT_PTRS; i++)
					{
						ind.used[i] = 0;
						ind.blocks[i] = -1;
					}
					ind.type = type;
					if (type == MFS_DIRECTORY)
					{
						ind.used[0] = 1;
						ind.blocks[0] = nextBlock;

						build_dir_block(1, inodeNum, pinum);

						ind.size += MAX_BLK_SIZE;
					}
					else if (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE)
					{
						return -1;
					}

					inodeMap[inodeNum] = nextBlock;

					lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
					write(fd, &ind, sizeof(inode));
					nextBlock++;

					update_checkpoint(inodeNum);

					return 0;
				}
			}
		}
		else
		{
			int bl = build_dir_block(0, inodeNum, -1);
			parent.size += MAX_BLK_SIZE;

			parent.used[b] = 1;
			parent.blocks[b] = bl;
			b--;
		}
	}

	return -1;
}

int Unlink(int pinum, char *name)
{
	inode toRemove;
	inode parent;
	if (get_inode(pinum, &parent) == -1)
	{
		return -1;
	}

	int inodeNum = Lookup(pinum, name);
	if (get_inode(inodeNum, &toRemove) == -1)
	{
		return 0;
	}

	if (toRemove.type == MFS_DIRECTORY)
	{
		int b;
		for (b = 0; b < MAX_DIRECT_PTRS; b++)
		{
			if (toRemove.used[b])
			{
				dirDataBlk block;
				lseek(fd, toRemove.blocks[b] * MAX_BLK_SIZE, SEEK_SET);
				read(fd, &block, MAX_BLK_SIZE);

				int e;
				for (e = 0; e < MAX_INODE; e++)
				{
					if (block.inodeNums[e] != -1 && strcmp(block.fileNames[e], ".") != 0 && strcmp(block.fileNames[e], "..") != 0)
					{
						return -1;
					}
				}
			}
		}
	}

	int found = 0;
	int b;
	for (b = 0; b < MAX_DIRECT_PTRS && !found; b++)
	{
		if (parent.used[b])
		{
			dirDataBlk block;
			lseek(fd, parent.blocks[b] * MAX_BLK_SIZE, SEEK_SET);
			read(fd, &block, MAX_BLK_SIZE);

			int e;
			for (e = 0; e < MAX_INODE && !found; e++)
			{
				if (block.inodeNums[e] != -1)
				{
					if (strcmp(name, block.fileNames[e]) == 0)
					{
						block.inodeNums[e] = -1;
						strcpy(block.fileNames[e], "INVALID");
						found = 1;
					}
				}
			}

			if (found)
			{
				lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
				write(fd, &block, MAX_BLK_SIZE);
				nextBlock++;

				parent.blocks[b] = nextBlock - 1;
				lseek(fd, nextBlock * MAX_BLK_SIZE, SEEK_SET);
				write(fd, &parent, MAX_BLK_SIZE);
				nextBlock++;

				inodeMap[pinum] = nextBlock - 1;
				update_checkpoint(pinum);
			}
		}
	}

	inodeMap[inodeNum] = -1;
	update_checkpoint(inodeNum);

	return 0;
}

int Shutdown()
{
	fsync(fd);
	exit(0);
	return -1;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("server portnumber FSimage\n");
		exit(1);
	}
	int portNumber = atoi(argv[1]);
	char *fileSysPath = argv[2];
	Startup(portNumber, fileSysPath);
	return 0;
}