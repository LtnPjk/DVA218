CC = gcc
CFLAGS = -Wall -pthread
PROGRAMS = client server

ALL: ${PROGRAMS}

client: client.c
	${CC} ${CFLAGS} -o client client.c

server: server.c
	${CC} ${CFLAGS} -o server server.c

clean:
	rm -f ${PROGRAMS}
