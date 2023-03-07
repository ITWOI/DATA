obj-m += dataRace.o 
CURRENT_PATH := $(shell pwd)             #当前所在的路径
LINUX_KERNEL := $(shell uname -r)        #内核的版本号

LINUX_KERNEL_PATH := /usr/src/linux-headers-$(LINUX_KERNEL)

all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules   

clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
