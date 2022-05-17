CC = gcc
CFLAGS = -Wall -Wextra

all: run_exe

run_exe: run
	./run

run: main.c queue.h premissas.h types.h
	${CC} ${CFLAGS} main.c -o $@

clean:
	rm -f *.out run
