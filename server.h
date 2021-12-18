typedef struct __inode {
	int inodeNum;
	int size;								// number of bytes in the file. a multiple of BLOCKSIZE
	int type;
	int used[MAX_BLOCKS];			// used[i] is true if blocks[i] is used
	int blocks[MAX_BLOCKS];		// address in memory of each block
} inode;

typedef struct __dirBlock {
	char names[MAX_INODE][MAX_LEN];
	int  inodeNums[MAX_INODE];
} dirBlock;

// Server functions
int get_inode(int inodeNum, inode* n);
int build_dir_block(int firstBlock, int inodeNum, int pinum);
void update_CR(int dirty_inum);
int Server_Startup();
int Server_Lookup(int pinum, char *name);
int Server_Stat(int inodeNum, MFS_Stat_t *m);
int Server_Write(int inodeNum, char *buffer, int block);
int Server_Read(int inodeNum, char *buffer, int block);
int Server_Creat(int pinum, int type, char *name);
int Server_Unlink(int pinum, char *name);
int Server_Shutdown();