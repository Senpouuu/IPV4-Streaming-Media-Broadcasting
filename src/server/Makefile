CC = gcc
CFLAGS=-pthread -g
LDFLAGS=


all:server

server:server.o thread_channel.o thread_list.o mytbf.o medialib.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o server
	