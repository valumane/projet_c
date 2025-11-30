CC = gcc
CFLAGS = -g -Wall -Wextra -pedantic -std=c99

all: master client worker

master: master.c
	$(CC) $(CFLAGS) master.c master_client.c -o master

client: client.c
	$(CC) $(CFLAGS) client.c master_client.c -o client

worker: worker.c
	$(CC) $(CFLAGS) worker.c master_worker.c -o worker

clean:
	rm client master worker
