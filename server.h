#define MAX_DIRECT_PTRS 	14				
#define MAX_NUM_INODES 	4096				
#define MAX_BLK_SIZE	4096			
#define MAX_DIR_ENTRY_SIZE	32		
#define MAX_INODE	(MAX_BLK_SIZE/MAX_DIR_ENTRY_SIZE)	
#define MAX_NAME_LEN	28		


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
	char fileNames[MAX_INODE][MAX_NAME_LEN];
	int  inodeNums[MAX_INODE];
} dirDataBlk;


int Startup();
int Creat(int pinum, int type, char *name);
int Read(int inodeNum, char *buffer, int block);
int Stat(int inodeNum, MFS_Stat_t *m);
int Lookup(int pinum, char *name);
int Write(int inodeNum, char *buffer, int block);
int Unlink(int pinum, char *name);
int Shutdown();