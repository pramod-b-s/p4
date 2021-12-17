all: libmfs.so server

libmfs.so:
	gcc -Wall -g -shared -fPIC -ggdb -o libmfs.so mfs.c udp.c

client: libmfs.so
	gcc -Wall -g -ggdb -L. -lmfs -o client client.c

server:
	gcc -Wall -g -o server server.c sNetworkLib.c lfs.c udp.c

clean:
	rm -rf libmfs.so client server