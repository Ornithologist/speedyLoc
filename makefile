CC=gcc
CFLAGS=-g -O0 -fPIC -fno-builtin
CFLAGS_AFT=-lm -lpthread

all: check

default: check

clean:
	rm -rf libmalloc.so *.o test

lib: libmalloc.so

libmalloc.so: malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o
	$(CC) -g -o0 -shared -Wl,--unresolved-symbols=ignore-all malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o -o libmalloc.so $(CFLAGS_AFT)

test: test.o
	$(CC) $(CFLAGS) $< -o $@ $(CFLAGS_AFT)

gdb: libmalloc.so test
	gdb --args env LD_PRELOAD=./libmalloc.so ./test

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@ $(CFLAGS_AFT)

check:	clean libmalloc.so test
	LD_PRELOAD=`pwd`/libmalloc.so ./test

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar 
	