obj-m += kfetch_mod_109511028.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
load:
	sudo insmod kfetch_mod_109511028.ko
unload:
	sudo rmmod -f kfetch_mod_109511028

