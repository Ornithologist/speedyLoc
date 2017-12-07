
obj-m+=speedyloc_driver.o

load: all
	sudo insmod speedyloc_driver.ko

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean

runtest: testfile.o testfile
	./testfile

testfile.o: testfile.c
	gcc -O0 -g -c -Wall -Werror -fpic  -o testfile.o testfile.c

testfile: testfile.o
	gcc -g -o testfile testfile.o
