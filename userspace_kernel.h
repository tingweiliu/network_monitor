
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

  Date: 2013-10-26 15:15:21 CST

  Interface between userspace and kernelspace

  Contact Information:
  Tony <tingw.liu@gmail.com>
  Home, Hushan Road, Qingdao, China. 
*******************************************************************************/




#ifndef __USERSPACE_KERNEL_H
#define __USERSPACE_KERNEL_H
struct hashnode{
	struct hlist_node list;
	time_t timestamp;
	unsigned char protocol;
	unsigned short port;
	unsigned long pps;
	unsigned long bps;
	unsigned long cps;
};

struct delaydata{
	struct list_head list;
	struct hashnode node;
};

//Just for user space got monitored port list
struct monitor_node{
	unsigned char protocol;
	unsigned short port;
};
struct monitor_node_ioctl{
	int count;
	struct monitor_node nodes[0];
};

/*
   Used to add monitored port to kernel module!
   Max length "A:TCP:65535", so 31 is enough.
*/
#define MAX_BUF_SIZE 31


/*
FIXME: we should use _IOW command
For ioctl to control monitor module!
*/
#define SIOGETMONITOR_COUNT 30
#define SIOGETMONITOR_CONTENT 10

#endif
