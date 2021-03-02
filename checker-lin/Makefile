CC = gcc
CFLAGS = -Wall -fPIC

build: so_stdio.o
	$(CC) -shared so_stdio.o -o libso_stdio.so

so_stdio.o: so_stdio.c
	$(CC) $(CFLAGS) -c so_stdio.c

clean:
	rm so_stdio.o libso_stdio.so 