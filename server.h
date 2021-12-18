typedef struct __inode {
	int inum;
	int size;								// number of bytes in the file. a multiple of BLOCKSIZE
	int type;
	int used[NBLOCKS];			// used[i] is true if blocks[i] is used
	int blocks[NBLOCKS];		// address in memory of each block
} inode;

typedef struct __dirBlock {
	char names[NENTRIES][NAMELENGTH];
	int  inums[NENTRIES];
} dirBlock;

// Server functions
int get_inode(int inum, inode* n);
int build_dir_block(int firstBlock, int inum, int pinum);
void update_CR(int dirty_inum);
int Server_Startup();
int Server_Lookup(int pinum, char *name);
int Server_Stat(int inum, MFS_Stat_t *m);
int Server_Write(int inum, char *buffer, int block);
int Server_Read(int inum, char *buffer, int block);
int Server_Creat(int pinum, int type, char *name);
int Server_Unlink(int pinum, char *name);
int Server_Shutdown();