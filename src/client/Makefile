CFLAGS+= -I../include -g

all: client

client: client.o
	gcc $^ -o $@ $(CFLAGS)
	
clean:
	rm -f *.o client