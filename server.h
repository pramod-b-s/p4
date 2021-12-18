#define MAX_DIRECT_PTRS 	14				
#define MAX_NUM_INODES 	4096			
#define CRSIZE		6					
#define BLOCKSIZE	4096			
#define DIRENTRYSIZE	32		
#define MAX_INODE	(BLOCKSIZE/DIRENTRYSIZE)	
#define MAX_LEN	28			


int inodeMap[MAX_NUM_INODES];			
int nextBlock;					
int fd;

typedef struct __inode {
	int inodeNum;
	int size;									
	int type;
	int used[MAX_DIRECT_PTRS];				
	int blocks[MAX_DIRECT_PTRS];		
} inode;

typedef struct __dirDataBlk {
	char fileNames[MAX_INODE][MAX_LEN];
	int  inodeNums[MAX_INODE];
} dirDataBlk;


int Server_Startup();
int Server_Lookup(int pinum, char *name);
int Server_Stat(int inodeNum, MFS_Stat_t *m);
int Server_Write(int inodeNum, char *buffer, int block);
int Server_Read(int inodeNum, char *buffer, int block);
int Server_Creat(int pinum, int type, char *name);
int Server_Unlink(int pinum, char *name);
int Server_Shutdown();