typedef struct __inode {
	int inodeNum;
	int size;									int type;
	int used[MAX_BLOCKS];				int blocks[MAX_BLOCKS];		} inode;

typedef struct __dirDataBlk {
	char fileNames[MAX_INODE][MAX_LEN];
	int  inodeNums[MAX_INODE];
} dirDataBlk;

void update_CR(int dirty_inum);
int get_inode(int inodeNum, inode* n);
int build_dir_block(int firstBlock, int inodeNum, int pinum);

int Server_Startup();
int Server_Lookup(int pinum, char *name);
int Server_Stat(int inodeNum, MFS_Stat_t *m);
int Server_Write(int inodeNum, char *buffer, int block);
int Server_Read(int inodeNum, char *buffer, int block);
int Server_Creat(int pinum, int type, char *name);
int Server_Unlink(int pinum, char *name);
int Server_Shutdown();