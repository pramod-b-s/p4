#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "udp.h"
#include "mfs.h"


int sendToHost(char *hostname, int port, dataPkt *sentPacket, dataPkt *responsePacket, int maxTries)
{
	int sd = UDP_Open(0);
	if (sd < -1)
	{
		perror("Error opening connectioind.\n");
		return -1;
	}

	struct sockaddr_in addr, addr2;
	int rc = UDP_FillSockAddr(&addr, hostname, port);
	if (rc < 0)
	{
		perror("Error looking up host.\n");
		return -1;
	}

	fd_set rfds;
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (1)
	{
		FD_ZERO(&rfds);
		FD_SET(sd, &rfds);
		UDP_Write(sd, &addr, (char *)sentPacket, sizeof(dataPkt));
		if (select(sd + 1, &rfds, NULL, NULL, &tv))
		{
			rc = UDP_Read(sd, &addr2, (char *)responsePacket, sizeof(dataPkt));
			if (rc > 0)
			{
				UDP_Close(sd);
				return 0;
			}
		}
		else
		{
			maxTries -= 1;
		}
	};
}

int MFS_Init(char *hostname, int port)
{
	if (port < 0 || strlen(hostname) < 1)
		return -1;
	serverHostname = malloc(strlen(hostname) + 1);
	strcpy(serverHostname, hostname);
	serverPort = port;
	mfsInitDone = 1;
	return 0;
}

int MFS_Lookup(int pinum, char *name)
{
	if (!mfsInitDone)
		return -1;

	if (strlen(name) > 27)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = pinum;
	sentPacket.fsop = LOOKUP;
	strcpy((char *)&(sentPacket.name), name);
	int rc = sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3);
	if (rc < 0)
		return -1;

	rc = responsePacket.inodeNum;
	return rc;
}

int MFS_Stat(int inodeNum, MFS_Stat_t *m)
{
	if (!mfsInitDone)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = inodeNum;
	sentPacket.fsop = STAT;

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	memcpy(m, &(responsePacket.stat), sizeof(MFS_Stat_t));
	return 0;
}

int MFS_Write(int inodeNum, char *buffer, int block)
{
	if (!mfsInitDone)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = inodeNum;
	memcpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	sentPacket.block = block;
	sentPacket.fsop = WR;

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inodeNum;
}

int MFS_Read(int inodeNum, char *buffer, int block)
{
	if (!mfsInitDone)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = inodeNum;
	sentPacket.block = block;
	sentPacket.fsop = RD;

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	if (responsePacket.inodeNum > -1)
		memcpy(buffer, responsePacket.buffer, BUFFER_SIZE);

	return responsePacket.inodeNum;
}

int MFS_Creat(int pinum, int type, char *name)
{
	if (!mfsInitDone)
		return -1;

	if (strlen(name) > 27)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = pinum;
	sentPacket.type = type;
	sentPacket.fsop = CREAT;

	strcpy(sentPacket.name, name);

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inodeNum;
}

int MFS_Unlink(int pinum, char *name)
{
	if (!mfsInitDone)
		return -1;

	if (strlen(name) > 27)
		return -1;

	dataPkt sentPacket;
	dataPkt responsePacket;

	sentPacket.inodeNum = pinum;
	sentPacket.fsop = UNLINK;
	strcpy(sentPacket.name, name);

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inodeNum;
}

int MFS_Shutdown()
{
	dataPkt sentPacket, responsePacket;
	sentPacket.fsop = EXIT;

	if (sendToHost(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return 0;
}