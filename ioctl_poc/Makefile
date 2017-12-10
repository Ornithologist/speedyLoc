ifneq ($(KERNELPATH),)
	KDIR	:= $(KERNELPATH)
else
	# Default kernel path
	KDIR	:= /lib/modules/$(shell uname -r)/build
endif

PWD	:= $(shell pwd)

obj-m+=query_ioctl.o

load: all unload
	sudo insmod query_ioctl.ko
	sudo chmod 666 /dev/query

unload:
	sudo rmmod query_ioctl.ko

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
