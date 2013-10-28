

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

  Date: 2013-10-27 10:08:50 CST

  Contact Information:
  Tony <tingw.liu@gmail.com>
  Home, Qingdao, China. 
*******************************************************************************/




#ifndef __LINUX_MONITOR_H__
#define __LINUX_MONITOR_H__
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define MAX_LIST 1024
#define MAX_SAVE_NODE 10240

struct hashnode *addnode(unsigned char protocol, unsigned short netport);
int delnode(unsigned char protocol, unsigned short netport);
int getmonitor_count(void __user *arg);
int getmonitor_content(void __user *arg);
#endif
