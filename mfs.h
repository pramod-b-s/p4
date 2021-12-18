#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)
#define MFS_BLOCK_SIZE   (4096)
#define MAX_BLOCKS 	14				
#define NINODES 	4096			
#define CRSIZE		6					
#define BLOCKSIZE	4096			
#define DIRENTRYSIZE	32		
#define MAX_INODE	(BLOCKSIZE/DIRENTRYSIZE)	
#define MAX_LEN	28			
#define BUFFER_SIZE (4096)
#define MAX_NAME_SIZE (28)

enum message {
	PAK_LOOKUP,
	PAK_STAT,
	PAK_WRITE,
	PAK_READ,
	PAK_CREAT,
	PAK_UNLINK,
	PAK_RESPONSE,
	PAK_SHUTDOWN
};

typedef struct __MFS_Stat_t {
    int type;       int size;       } MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];      int  inodeNum;      } MFS_DirEnt_t;

typedef struct __dataPkt {
	enum message message;
	int inodeNum;
	int block;
	int type;

	char name[MAX_NAME_SIZE];
	char buffer[BUFFER_SIZE];
	MFS_Stat_t stat;
} dataPkt;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inodeNum, MFS_Stat_t *m);
int MFS_Write(int inodeNum, char *buffer, int block);
int MFS_Read(int inodeNum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

