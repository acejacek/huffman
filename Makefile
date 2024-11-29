CC = gcc
CFLAGS = -Wall -Wextra -pedantic

all: exec

debug: CFLAGS += -ggdb
debug: exec

exec: main.o huff.o
	$(CC) $(CFLAGS) main.o huff.o -o compressor

main.o: main.c huff.h
	$(CC) $(CFLAGS) -c main.c

huff.o:	huff.c huff.h
	$(CC) $(CFLAGS) -c huff.c

run: exec test
	
test:
	./test.sh

clean:
	rm *.o compressor
