CC=gcc
CFLAGS=-g -O0 -fPIC -fno-builtin
CFLAGS_AFT=-lm -lpthread

all: check

default: check

clean:
	rm -rf libmalloc.so *.o testfile

lib: libmalloc.so

# libmalloc.so: malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o
# 	$(CC) -g -o0 -shared -Wl,--unresolved-symbols=ignore-all malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o -o libmalloc.so $(CFLAGS_AFT)

libmalloc.so: malloc.o free.o
	$(CC) -g -o0 -shared -Wl,--unresolved-symbols=ignore-all malloc.o free.o -o libmalloc.so $(CFLAGS_AFT)

# libmalloc.so: malloc_test.o
# 		$(CC) -g -o0 -shared -Wl,--unresolved-symbols=ignore-all malloc_test.o  -o libmalloc.so $(CFLAGS_AFT)

testfile: testfile.o
	$(CC) $(CFLAGS) $< -o $@ $(CFLAGS_AFT)

gdb: libmalloc.so testfile
	gdb --args env LD_PRELOAD=./libmalloc.so ./testfile

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@ $(CFLAGS_AFT)

check:	clean libmalloc.so testfile
	sudo chmod 666 /dev/query
	LD_PRELOAD=`pwd`/libmalloc.so ./testfile

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
