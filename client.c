#include <stdio.h>
#include "udp.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    struct sockaddr_in addrSnd, addrRcv;

    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char fsop[BUFFER_SIZE];
    sprintf(fsop, "hello world");

    printf("client:: send fsop [%s]\n", fsop);
    rc = UDP_Write(sd, &addrSnd, fsop, BUFFER_SIZE);
    if (rc < 0) {
	printf("client:: failed to send\n");
	exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, fsop, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, fsop);
    return 0;
}
