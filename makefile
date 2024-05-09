CC = gcc
CFLAGS = -Wall -g -std=c11

A1: main.c
	$(CC) $(CFLAGS) main.c -o A1

clean:
	rm -f A1 *.o *.hist
