CC=gcc
CFLAGS_debug=-ggdb
CFLAGS_release=-O3
CFLAGS=$(CFLAGS_debug)

all: utest

utest: utest.o cinq_cache.o rbtree.o
	$(CC) $(CFLAGS) $^ -o $@

utest.o: utest.c cinq_cache.h
	$(CC) $(CFLAGS) $< -c -o $@

cinq_cache.o: cinq_cache.c cinq_cache.h
	$(CC) $(CFLAGS) $< -c -o $@

rbtree.o: rbtree.c rbtree.h
	$(CC) $(CFLAGS) $< -c -o $@

runtest: utest
	@echo ========================
	@./utest

clean:
	rm -rf *.o *.ko utest

