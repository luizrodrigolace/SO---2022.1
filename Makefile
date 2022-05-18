CC = gcc
CFLAGS = -Wall -Wextra

all: run_exe

run_exe: run
	./run

run: main.c queue.h premissas.h types.h
	${CC} ${CFLAGS} main.c -o $@

test_queue: test_queue.out
	./test_queue.out

test_queue.out: testes/queue_test.c queue.h
	${CC} ${CFLAGS} testes/queue_test.c -o $@

clean:
	rm -f *.out run
