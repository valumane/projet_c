CC = gcc
CFLAGS = -g -Wall -Wextra -pedantic -std=c99

all: worker master client

worker: worker.c
	$(CC) $(CFLAGS) worker.c -o worker.o

master: master.c
	$(CC) $(CFLAGS) master.c master_client.c -o master.o

client: client.c
	$(CC) $(CFLAGS) client.c master_client.c -o client.o

clean:
	rm -f *.o *.fifo
