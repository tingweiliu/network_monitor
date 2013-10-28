obj-m:=network_monitor.o
network_monitor-y:=monitor.o memdevice.o
KERNELBUILD:=/usr/src/linux/
default:
	make -C $(KERNELBUILD) M=$(shell pwd) modules
	gcc -g monitor_read.c -o monitor_read
	gcc -g monitor_control.c -o monitor_control
clean:
	rm -rf *.o *.ko *.mod.c .tmp_versions Module*  .*.cmd modules.order monitor_read monitor_control
