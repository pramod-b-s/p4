#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)
#define MFS_BLOCK_SIZE   (4096)
#define MAX_BLOCKS 	14				// max number of blocks per inode
#define NINODES 	4096			// max number of inodes in system
#define CRSIZE		6					// size (in blocks) of checkpoint region TODO
#define BLOCKSIZE	4096			// size (in bytes) of one block
#define DIRENTRYSIZE	32		// size (in bytes) of a directory entry
#define MAX_INODE	(BLOCKSIZE/DIRENTRYSIZE)	// number of entries per block in a directory
#define MAX_LEN	28			// length (in bytes) of a directory entry name
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
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inodeNum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

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

