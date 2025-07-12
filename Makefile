CC=gcc
CFLAGS=-I.

exec: main.o
	$(CC) -o exec main.o -I.

clean:
	rm exec main.o
