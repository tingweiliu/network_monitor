

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

  Date: 2013-10-26 14:59:24 CST

  Contact Information:
  Tony <tingw.liu@gmail.com>
  Home, Hushan Road, Qingdao, China. 
*******************************************************************************/




#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <net/icmp.h>
#include <linux/skbuff.h>

#include "monitor.h"
#include "userspace_kernel.h"
#include "memdevice.h"

DEFINE_SPINLOCK(hashlist_lock);
struct timer_list timer;

static struct nf_hook_ops iphook_in;
static struct nf_hook_ops iphook_out;

struct hlist_head hashnode_list[MAX_LIST];

struct kmem_cache *cachep = NULL;

int max_save_num = MAX_SAVE_NODE;
module_param(max_save_num, int, 0644);

atomic_t savednum;
void hashnode_head_init(void)
{
	int i=0;
	for(; i < MAX_LIST; i++)
		INIT_HLIST_HEAD(&hashnode_list[i]);
}


static int gethash(unsigned short netport)
{
	return ntohs(netport)&(MAX_LIST - 1);
}


struct hashnode *__gethashnode(unsigned char protocol, unsigned short netport)
{
	struct hlist_node *pos;
	struct hashnode *tpos = NULL; 
	int index = gethash(netport);
	hlist_for_each_entry(tpos, pos, &hashnode_list[index], list) {
		if(tpos->port == netport && tpos->protocol == protocol)
			return tpos;
	}
	return NULL;
}

int delnode(unsigned char protocol, unsigned short netport)
{
	struct hlist_node *pos, *n;
	struct hashnode *tpos = NULL; 
	int index = gethash(netport);
	spin_lock_bh(&hashlist_lock);
	hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[index],list){
		if(tpos->port == netport && tpos->protocol == protocol) {
			hlist_del(&tpos->list);
			kfree(tpos);
			printk(KERN_INFO "network monitor: Remove monitor "
					"%d:%u\n", protocol, ntohs(netport));
		}
	}
	spin_unlock_bh(&hashlist_lock);
	return 0;
}
struct hashnode *addnode(unsigned char protocol, unsigned short netport)
{
	struct hlist_node *pos;
	struct hashnode *tpos = NULL; 
	int index = gethash(netport);
	spin_lock_bh(&hashlist_lock);
	hlist_for_each_entry(tpos,pos,&hashnode_list[index],list){
		if(tpos->port == netport && tpos->protocol == protocol) {
			printk(KERN_INFO "network monitor: Already in monitor "
					"%d:%u\n", protocol, ntohs(netport));
			goto ADDOK;
		}
	}
	tpos = (struct hashnode*)kzalloc(sizeof(struct hashnode), GFP_ATOMIC);
	if (likely(tpos)) {
		tpos->protocol = protocol;
		tpos->port = netport;
		printk(KERN_INFO "network monitor: Add new monitor %d:%u\n",
				protocol, ntohs(netport));
		hlist_add_head(&(tpos->list),&hashnode_list[index]);
	}
ADDOK:
	spin_unlock_bh(&hashlist_lock);
	return tpos;
}

int getmonitor_count(void __user *arg)
{
	struct hashnode *tpos;
	struct hlist_node *pos,*n;
	int i, ret, number = 0;
	if (copy_from_user(&number, arg, sizeof(int))) {
		printk(KERN_WARNING "network monitor: %s:%d copy_from_user err!",
				__FILE__, __LINE__);
		return -EFAULT;
	}
	spin_lock_bh(&hashlist_lock);
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			++number;
		}
	}
	spin_unlock_bh(&hashlist_lock);
	ret = copy_to_user(arg, &number, sizeof(int)) ? -EFAULT : 0;
	return ret;
}


int getmonitor_content(void __user *arg)
{
	struct hashnode *tpos;
	struct hlist_node *pos,*n;
	int i, ret, number = 0;
	int copylen = 0;
	struct monitor_node_ioctl *nctl, _nctl;
	if (copy_from_user(&_nctl, arg, sizeof(struct monitor_node_ioctl))) {
		printk(KERN_WARNING "network monitor: %s:%d copy_from_user err!",
				__FILE__, __LINE__);
		return -EFAULT;
	}
	if (_nctl.count < 0) {
		printk(KERN_WARNING "network monitor: %s:%d user param err!",
				__FILE__, __LINE__);
		return -EFAULT;
	}
	copylen = sizeof(struct monitor_node_ioctl) +
			sizeof(struct monitor_node) * _nctl.count;
	nctl = kmalloc(copylen, GFP_KERNEL);
	if (nctl == NULL) {
		printk(KERN_WARNING "network monitor: %s:%d kmalloc faild!",
				__FILE__, __LINE__);
		return -EFAULT;
	}

	spin_lock_bh(&hashlist_lock);
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			nctl->nodes[number].port = tpos->port;
			nctl->nodes[number].protocol = tpos->protocol;
			++number;
			if (number >= _nctl.count)
				break;
		}
	}
	spin_unlock_bh(&hashlist_lock);
	nctl->count = number;
	printk(KERN_INFO "network monitor monitor number=%d\n", number);
	ret = copy_to_user(arg, nctl, copylen) ? -EFAULT : 0;
	return ret;
}

void hashnode_free(void)
{
	struct hashnode *tpos;
	struct hlist_node *pos,*n;
	int i;
	spin_lock_bh(&hashlist_lock);
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			hlist_del(&tpos->list);
			kfree(tpos);
		}
	}
	spin_unlock_bh(&hashlist_lock);
}
void __clearnode(void)
{
	struct hashnode *tpos;
	struct hlist_node *pos,*n;
	int i;
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			tpos->pps = 0;
			tpos->bps = 0;
			tpos->cps = 0;
		}
	}
}
void __shownode(void)
{
	struct hashnode *tpos;
	struct hlist_node *pos,*n;
	int i;
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			printk(KERN_ALERT "protocol=%d, port=%d, pps=%lu, "
				"bps=%lu, cps=%lu\n", tpos->protocol,
				ntohs(tpos->port), tpos->pps, tpos->bps,
				tpos->cps);
		}
	}
}
void __savenode(void)
{
	struct hashnode *tpos;
	struct hlist_node *pos, *n;
	struct delaydata *elem;
	int i;
	time_t now = get_seconds();
	for (i = 0; i < MAX_LIST; i++) {
		hlist_for_each_entry_safe(tpos, pos, n, &hashnode_list[i], list) {
			if (atomic_read(&savednum) > max_save_num) {
				printk(KERN_WARNING "network monitor:Too "
						"much node in memory not read!\n");
				return;
			}
			elem = kmem_cache_alloc(cachep, GFP_ATOMIC);
			if (elem) {
				memcpy(&(elem->node), tpos, sizeof(struct hashnode));
				elem->node.timestamp = now;
				add_data(elem);
			}
		}
	}
}

static void timerfun(unsigned long arg)
{
	spin_lock_bh(&hashlist_lock);
	//__shownode();
	__savenode();
	__clearnode();
	mod_timer(&timer, jiffies + HZ);
	spin_unlock_bh(&hashlist_lock);
}


static void timer_init(void)
{
	init_timer(&timer);
	timer.function = &timerfun;
	timer.expires = jiffies + HZ;
	timer.data = 0;
	add_timer(&timer);
}

static void timer_exit(void)
{
	del_timer_sync(&timer);
}


struct kmem_cache * kmemcache_init(void)
{
	cachep = kmem_cache_create("network_monitor", sizeof(struct delaydata),
			SLAB_HWCACHE_ALIGN, 0, NULL);
	return cachep;
}
void kmemcache_free(void)
{
	kmem_cache_destroy(cachep);
	return;
}

static unsigned int ippacket_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int(*okfn)(struct sk_buff*))
{
	struct iphdr *iph;
	struct tcphdr *th = NULL, _tcph;
	struct udphdr *uh = NULL, _udph;
	unsigned int protoff;
	struct hashnode *node;
	spin_lock_bh(&hashlist_lock);
	iph = (struct iphdr*)ip_hdr(skb);
	protoff = iph->ihl << 2;
	if (iph->protocol == IPPROTO_TCP) {
		th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
		if (th) {
			node = __gethashnode(IPPROTO_TCP, th->dest);
			if (node) {
				node->pps ++;
				node->bps += ntohs(iph->tot_len);
				if (th->syn) {
					node->cps ++;
				}
			}
		}
	} else if (iph->protocol == IPPROTO_UDP) {
		uh = skb_header_pointer(skb, protoff, sizeof(_udph), &_udph);
		if (uh) {
			node = __gethashnode(IPPROTO_UDP, uh->dest);
			if (node) {
				node->pps ++;
				node->bps += ntohs(iph->tot_len);
			}
		}
	} else {
		;
	}
	spin_unlock_bh(&hashlist_lock);
	return NF_ACCEPT;
}
static unsigned int ippacket_out(unsigned int hooknum,struct sk_buff *skb,
		const struct net_device *in,const struct net_device *out,
		int(*okfn)(struct sk_buff*))
{
	struct iphdr *iph;
	struct tcphdr *th = NULL, _tcph;
	struct udphdr *uh = NULL, _udph;
	unsigned int protoff;
	struct hashnode *node;
	spin_lock_bh(&hashlist_lock);
	iph = (struct iphdr*)ip_hdr(skb);
	protoff = iph->ihl << 2;
	if (iph->protocol == IPPROTO_TCP) {
		th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
		if (th) {
			node = __gethashnode(IPPROTO_TCP, th->dest);
			if (node) {
				node->pps ++;
				node->bps += ntohs(iph->tot_len);
				if (th->syn) {
					node->cps ++;
				}
			}
		}
	} else if (iph->protocol == IPPROTO_UDP) {
		uh = skb_header_pointer(skb, protoff, sizeof(_udph), &_udph);
		if (uh) {
			node = __gethashnode(IPPROTO_UDP, uh->dest);
			if (node) {
				node->pps ++;
				node->bps += ntohs(iph->tot_len);
			}
		}
	} else {
		;
	}
	spin_unlock_bh(&hashlist_lock);
	return NF_ACCEPT;
}
static int iphook_init(void)
{
	memset(&iphook_in,0,sizeof(struct nf_hook_ops));
	iphook_in.hook=ippacket_in;
	iphook_in.owner=THIS_MODULE;
	iphook_in.pf=PF_INET;
	iphook_in.hooknum=NF_INET_LOCAL_IN;

	memset(&iphook_out,0,sizeof(struct nf_hook_ops));
	iphook_out.hook=ippacket_out;
	iphook_out.owner=THIS_MODULE;
	iphook_out.pf=PF_INET;
	iphook_out.hooknum=NF_INET_LOCAL_OUT;

	nf_register_hook(&iphook_in);
	nf_register_hook(&iphook_out);
	return 0;
}

static void iphook_exit(void)
{
	nf_unregister_hook(&iphook_in);
	nf_unregister_hook(&iphook_out);
}

static int __init network_monitor_init(void)
{
	atomic_set(&savednum, 1);
	if (kmemcache_init() == NULL) {
		printk(KERN_ALERT "kmemcache_init err!\n");
		goto CACHE_ERROR;
	}
	if (kernelmem_init() != 0) {
		printk(KERN_ALERT "memdevice init err!\n");
		goto MEMDEVICE_ERROR;
	}
	hashnode_head_init();
	iphook_init();
	timer_init();
	return 0;

MEMDEVICE_ERROR:
	kmemcache_free();
CACHE_ERROR:
	return -1;
}

static void __exit network_monitor_exit(void)
{
	timer_exit();
	iphook_exit();
	hashnode_free();
	kernelmem_exit();
	kmemcache_free();
}

module_init(network_monitor_init);
module_exit(network_monitor_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony <tingw.liu@gmail.com>");
