CFLAGS=-g
OBJS=queue.o main.o
LIBS=-pthread
CC=gcc

all: main

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean: 
	rm -f *.o main
