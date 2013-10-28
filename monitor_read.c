


/*******************************************************************************
	
  This is a demo read monitor information from kernel device!

  Version: 0.1

  Date: 2013-10-27 15:01:39 CST

  Contact Information:
  Tony <tingw.liu@gmail.com>
*******************************************************************************/




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


//FIXME: Put next definition before include
struct list_head {
	struct list_head *next, *prev;
};
struct hlist_node {
	struct hlist_node *next, **pprev;
};
#include "userspace_kernel.h"

int main()
{
	int fd, len;
	struct delaydata data; 
	fd = open("/dev/delaymem",O_RDONLY);
	if (fd == -1) {
		printf("open /dev/delaymem error\n");
		exit(0);
	}
	while(1){
		len = read(fd, &data, sizeof(struct delaydata));
		if(len == sizeof(struct delaydata)){
			printf("time=%u protocol=%d port=%d: pps=%lu "
				"bps=%lu cps=%lu\n", data.node.timestamp,
				data.node.protocol, ntohs(data.node.port),
				data.node.pps, data.node.bps, data.node.cps);
		}
	}
}
