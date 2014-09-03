/*
 * include/net/switchdev.h - Switch device API
 * Copyright (c) 2014 Jiri Pirko <jiri@resnulli.us>
 * Copyright (c) 2014 Scott Feldman <sfeldma@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _LINUX_SWITCHDEV_H_
#define _LINUX_SWITCHDEV_H_

#include <linux/netdevice.h>

#ifdef CONFIG_NET_SWITCHDEV

int netdev_sw_parent_id_get(struct net_device *dev,
			    struct netdev_phys_item_id *psid);
int netdev_sw_port_fdb_add(struct net_device *dev,
			   const unsigned char *addr, u16 vid);
int netdev_sw_port_fdb_del(struct net_device *dev,
			   const unsigned char *addr, u16 vid);

#else

static inline int netdev_sw_parent_id_get(struct net_device *dev,
					  struct netdev_phys_item_id *psid)
{
	return -EOPNOTSUPP;
}

static inline int netdev_sw_port_fdb_add(struct net_device *dev,
					 const unsigned char *addr, u16 vid)
{
	return -EOPNOTSUPP;
}

static inline int netdev_sw_port_fdb_del(struct net_device *dev,
					 const unsigned char *addr, u16 vid)
{
	return -EOPNOTSUPP;
}

#endif

#endif /* _LINUX_SWITCHDEV_H_ */
