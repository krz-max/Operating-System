obj-m += kfetch_mod.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
load:
	sudo insmod kfetch_mod.ko
unload:
	sudo rmmod -f kfetch_mod

