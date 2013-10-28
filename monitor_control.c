


/*******************************************************************************
  
  Copyright(c) 2008-2013 

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Version:  0.1

  Date: 2013-10-26 07:10:15 CST

  Contact Information:
  Tony <tingw.liu@gmail.com>
  Home, Hushan Road, Qingdao, China. 
*******************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

//FIXME: Put next definition before include
struct list_head {
	struct list_head *next, *prev;
};
struct hlist_node {
	struct hlist_node *next, **pprev;
};
#include "userspace_kernel.h"

char option;
const char *options = "hladtup:";
const struct option long_options[] = {
	{"help", 0, NULL, 'h'},
	{"add", 0, NULL, 'a'},
	{"del", 0, NULL, 'd'},
	{"port", 1, NULL, 'p'},
	{"tcp", 0, NULL, 't'},
	{"udp", 0, NULL, 'u'},
	{"list", 0, NULL, 'l'},
	{NULL, 0, NULL, 0}
};

void help()
{
	printf("Usage: monitor_control -[hadtu] -p port\n");
	printf("\t -l --list	List monitored port\n");
	printf("\t -a --add	Add a new port to monitor\n");
	printf("\t -d --del	Delete a new port to monitor\n");
	printf("\t -t --tcp	Monitor tcp protocol\n");
	printf("\t -u --udp	Monitor udp protocol\n");
	printf("\t -p --port	Monitor port number\n");
	printf("\t -h --help	Print this manual\n");
	printf("monitor_control -a -t -p 80\n");
	printf("monitor_control -d -u -p 8000\n");
	printf("Any questions and donate to Tony <tingw.liu@gmail.com>!\n");
	exit(-1);
}

int main(int argc, char **argv)
{
	int fd, ret = 0;
	struct delaydata data;
	char buf[MAX_BUF_SIZE] = {0};
	char *type = NULL, *protocoltype = NULL, *port = NULL;
	int list = 0, monitorcount = 0, i;
	struct monitor_node_ioctl *nctl;
	while ((option = getopt_long(argc, argv, options, long_options, NULL)) != -1)
	{
		switch (option) {
		case 'h':
			help();
			break;
		case 'a':
			type = "A";
			break;
		case 'd':
			type = "D";
			break;
		case 't':
			protocoltype = "TCP";
			break;
		case 'u':
			protocoltype = "UDP";
			break;
		case 'p':
			port = strdup(optarg);
			break;
		case 'l':
			list = 1;
			break;	
		default:
			help();
		}
	}
	if (list) {
		fd = open("/dev/delaymem",O_RDONLY);
		if (fd == -1) {
			printf("open /dev/delaymem error\n");
			exit(0);
		}
		ret = ioctl(fd, SIOGETMONITOR_COUNT, &monitorcount);
		if (ret == 0) {
			printf("monitor count = %d\n", monitorcount);
			nctl = malloc(sizeof(struct monitor_node_ioctl) +
					sizeof(struct monitor_node) * monitorcount);
			nctl->count = monitorcount;
			ret = ioctl(fd, SIOGETMONITOR_CONTENT, nctl);

			/*
			   Between this two ioctl call, monitored port maybe changed.
			   So use nctl->count not monitorcount!
			*/
			for (i = 0; i < nctl->count; i++) {
				printf("protocol=%d port=%d\n",
						nctl->nodes[i].protocol,
						ntohs(nctl->nodes[i].port));
			}
		}
		close(fd);
	}
	else if (type && protocoltype && port) {
		fd = open("/dev/delaymem",O_WRONLY);
		if (fd == -1) {
			printf("open /dev/delaymem error\n");
			exit(0);
		}
		snprintf(buf, MAX_BUF_SIZE, "%s:%s:%s", type, protocoltype, port);
		ret = write(fd, buf, strlen(buf));
		if (ret != strlen(buf)) {
			printf("Command run fail\n");
		} else {
			printf("Command run success\n");
		}
		close(fd);
	} else {
		help();
	}
}
