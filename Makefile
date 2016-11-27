
obj-m += chat.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc test_module.c -o test_module
	gcc read_module.c -o readers


clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test_module
	rm readers
