

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

  Date: 2013-10-26 10:25:55 CST

  Contact Information:
  Tony <tingw.liu@gmail.com>
  Home, Hushan Road, Qingdao, China. 
*******************************************************************************/




/*======================================================================
	kernel write file for interruptable content
======================================================================*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include "memdevice.h"
#include "monitor.h"

#define MEM_CLEAR 0x1
#define GLOBALMEM_MAJOR 250
#define GLOBALMEM_MINOR 1

MODULE_AUTHOR("LTW");
MODULE_LICENSE("GPL");

extern struct kmem_cache *cachep;

static int kernelmem_major = GLOBALMEM_MAJOR;
module_param(kernelmem_major, int, S_IRUGO);


struct list_head bandlist;
DEFINE_SPINLOCK(delay_lock);
DECLARE_WAIT_QUEUE_HEAD(delaydata_queue);
static int name_flag=0;

extern atomic_t savednum;

static int myatoi(char *port)
{
	char *t = port;
	unsigned int p = 0;
	int i = 1;
	while (*(++t));
	--t;
	while (t != port - 1) {
		p += (*t - '0') * i;
		i *= 10;
		--t;
	}
	return p;
}

void add_data(struct delaydata *e)
{
#ifdef NETMONITOR_DEBUG
	printk(KERN_ALERT "Add one\n");
#endif
	spin_lock_bh(&delay_lock);
	list_add_tail(&(e->list), &bandlist);
	atomic_inc(&savednum);
	spin_unlock_bh(&delay_lock);
	name_flag=1;
	wake_up_interruptible(&delaydata_queue);
}
void freelist(void)
{
	struct list_head *i,*n;
	struct delaydata *ent;
	list_for_each_safe(i,n,&bandlist){
		ent=list_entry(i,struct delaydata,list);
		list_del(i);
		kmem_cache_free(cachep, ent);
	}
}
struct kernelmem_dev                                     
{                                                        
	struct cdev cdev; 
	loff_t size;
	struct semaphore sem;
};
struct kernelmem_dev *kernelmem_devp; 
int kernelmem_open(struct inode *inode, struct file *filp)
{
	filp->private_data = kernelmem_devp;
	return 0;
}

int kernelmem_release(struct inode *inode, struct file *filp)
{
	return 0;
}
/*
 ADD monitor port
	A:TCP:80
 Delete monitor port
	D:UDP:8000 
 */
static ssize_t kernelmem_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	char kbuf[MAX_BUF_SIZE + 1];
	char *proto_ptr, *port_ptr;
	unsigned char protocol;
	unsigned short port;
	struct hashnode *node;
	int addport; //1 add port; 0 del port
	if (count > MAX_BUF_SIZE) {
		count = MAX_BUF_SIZE;
	}
	if (copy_from_user(kbuf, buf, count)) {
		goto WRITE_ERROR;
	}
	kbuf[count] = 0;
	if (*kbuf == 'A')
		addport = 1;
	else if (*kbuf == 'D')
		addport = 0;
	else
		goto WRITE_ERROR;

	proto_ptr = strchr(kbuf, ':');
	if (proto_ptr == NULL)
		goto WRITE_ERROR;
	++proto_ptr;

	port_ptr = strchr(proto_ptr, ':');
	if (port_ptr == NULL)
		goto WRITE_ERROR;
	*port_ptr = 0;
	++port_ptr;
	if (strcasecmp(proto_ptr, "TCP") == 0) {
		protocol = IPPROTO_TCP;
	} else if (strcasecmp(proto_ptr, "UDP") == 0) {
		protocol = IPPROTO_UDP;
	} else {
		printk(KERN_WARNING "network monitor: Can't follow this protocol\n");
		goto WRITE_ERROR;
	}
	port = myatoi(port_ptr);
	if (port < 0 || port > 65535)
		goto WRITE_ERROR;
	if (addport) {
		node = addnode(protocol, htons(port));
		if (node == NULL)
			goto WRITE_ERROR;
	} else {
		delnode(protocol, htons(port));
	}
	return count;
WRITE_ERROR:
	return 0;
}

static ssize_t kernelmem_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
	unsigned int count = size;
	struct kernelmem_dev *dev = filp->private_data; 
	struct list_head *i,*n;
	struct delaydata *ent=NULL;
	if(list_empty(&bandlist)){
		if(wait_event_interruptible(delaydata_queue,name_flag!=0)){
			return -ERESTARTSYS;
		}
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	spin_lock_bh(&delay_lock);
	list_for_each_safe(i,n,&bandlist){
		ent = list_entry(i,struct delaydata,list);
		list_del(i);
		name_flag = 0;
		break;
	}
	spin_unlock_bh(&delay_lock);
	if (ent == NULL)
		goto GOT_ERROR;
	if (copy_to_user(buf,(char *)ent, sizeof(struct delaydata))) {
		printk(KERN_ALERT "network_monitor: %d copy_to_user error\n",
				__LINE__);
		goto COPY_ERROR;
	}
	atomic_dec(&savednum);
	kmem_cache_free(cachep, ent);
	up(&dev->sem);
	return count;

COPY_ERROR:
	atomic_dec(&savednum);
	kmem_cache_free(cachep, ent);
GOT_ERROR:
	up(&dev->sem);
	return -1;
}


static long
kernelmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct kernelmem_dev *dev = filp->private_data; 
	int err = -1;
	if (signal_pending(current))
		return -ERESTARTSYS;
	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	switch (cmd) {
	case SIOGETMONITOR_COUNT:
		printk(KERN_INFO "network monitor: Got SIOGETMONITOR_COUNT\n");
		err = getmonitor_count((void __user *)arg);
		break;
	case SIOGETMONITOR_CONTENT:
		printk(KERN_INFO "network monitor: Got SIOGETMONITOR_CONTENT\n");
		err = getmonitor_content((void __user *)arg);
		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}
	up(&dev->sem);
	if (err)
		printk(KERN_WARNING "network monitor: %s:%d err=%d\n",
				__FILE__, __LINE__, err);
	return err;
}

static const struct file_operations kernelmem_fops =
{
	.owner = THIS_MODULE,
	.llseek = NULL,
	.read = kernelmem_read,
	.write = kernelmem_write,
	.open = kernelmem_open,
	.unlocked_ioctl = kernelmem_ioctl,
	.release = kernelmem_release,
};
static void kernelmem_setup_cdev(struct kernelmem_dev *dev, int index)
{
	int err, devno = MKDEV(kernelmem_major, index);
	cdev_init(&dev->cdev, &kernelmem_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &kernelmem_fops;
	dev->size=0;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE "Error %d adding GLTW%d", err, index);
}
int kernelmem_init(void)
{
	int result;
	dev_t devno = MKDEV(kernelmem_major, GLOBALMEM_MINOR);
	if (kernelmem_major)
		result = register_chrdev_region(devno, 1, "delaymem");
	else{
		result = alloc_chrdev_region(&devno, 0, 1, "delaymem");
		kernelmem_major = MAJOR(devno);
	}  
	if (result < 0)
		return result;
	kernelmem_devp = kmalloc(sizeof(struct kernelmem_dev), GFP_KERNEL);
	if(!kernelmem_devp){
		result =  - ENOMEM;
		goto fail_malloc;
	}
	memset(kernelmem_devp, 0, sizeof(struct kernelmem_dev));
	kernelmem_setup_cdev(kernelmem_devp, GLOBALMEM_MINOR);
	sema_init(&kernelmem_devp->sem, 1);
	INIT_LIST_HEAD(&bandlist);
	return 0;
fail_malloc: 
	unregister_chrdev_region(devno, 1);
	return result;
}
void kernelmem_exit(void)
{
	cdev_del(&kernelmem_devp->cdev);
	kfree(kernelmem_devp);
	freelist();
	unregister_chrdev_region(MKDEV(kernelmem_major, GLOBALMEM_MINOR), 1); 
}
