CC=gcc
CFLAGS_debug=-ggdb
CFLAGS_release=-O3
CFLAGS=$(CFLAGS_debug)

all: runtest

test: test.o cinq_cache.o
	$(CC) $(CFLAGS) $^ -o $@

test.o: test.c cinq_cache.h
	$(CC) $(CFLAGS) $< -c -o $@

cinq_cache.o: cinq_cache.c cinq_cache.h
	$(CC) $(CFLAGS) $< -c -o $@

runtest: test
	./test

clean:
	rm -rf *.o test

