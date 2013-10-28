#!/bin/sh

paramnum=$#

help()
{
	echo "Usage:"
	echo "	$0 start [devicenum] [max_node]"
	echo "		devicenum default 250, max_node default 10240"
	echo "	$0 stop"
	exit -1
}
start()
{
	mknod /dev/delaymem c $1 1 
	insmod network_monitor.ko max_save_num=$2
}
stop()
{
	fuser -k -f /dev/delaymem
	rm -f /dev/delaymem
	rmmod network_monitor
}

if [ $paramnum -lt 1 ];then
	help
fi

if [ "$1" = "start" ];then
	if [ $paramnum -ne 3 ];then
		start 250 10240
	else
		start $2 $3
	fi
elif [ "$1" = "stop" ];then
	stop
else
	help
fi
