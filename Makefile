CC     := gcc
CFLAGS := -Wall -Werror -g
LDFLAGS:= -L. -lmfs

LIB	   := mfs.c

DEPS   := udp.c

.PHONY: all
all: libmfs.so server

server: server.o ${DEPS}
	${CC} ${CFLAGS} -o server server.c ${DEPS}

client: client.o libmfs.so
	${CC} ${CFLAGS} -o client client.o ${LDFLAGS}

libmfs.so : mfs.o ${DEPS}
	${CC} ${CFLAGS} -shared -fPIC -Wl,-soname,libmfs.so -o libmfs.so mfs.o udp.c -lc

clean:
	rm -f ./client ./server *.o libmfs.so

mfs.o : ${LIB} Makefile
	${CC} ${CFLAGS} -c -fPIC ${LIB}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<