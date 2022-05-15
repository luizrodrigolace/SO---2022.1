CC = gcc
CFLAGS = -Wall -Wextra

all: run

run: main
	./main

main: main.c queue.h premissas.h types.h
	${CC} ${CFLAGS} main.c -o $@

clean:
	rm -f *.out main
