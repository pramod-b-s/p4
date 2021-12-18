#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)
#define MFS_BLOCK_SIZE   (4096)
#define BUFFER_SIZE (4096)
#define MAX_NAME_SIZE (28)

char *serverHostname;
int serverPort;
int mfsInitDone = 0;

enum fsop {
	CREAT,
	RD,
	LOOKUP,
	STAT,
	RSP,
	WR,
	UNLINK,
	EXIT
};

typedef struct __MFS_Stat_t {
    int type;       
	int size;       
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];      
	int  inodeNum;      
} MFS_DirEnt_t;

typedef struct __dataPkt {
	enum fsop fsop;
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

