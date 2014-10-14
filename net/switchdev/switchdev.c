/*
 * net/switchdev/switchdev.c - Switch device API
 * Copyright (c) 2014 Jiri Pirko <jiri@resnulli.us>
 * Copyright (c) 2014 Scott Feldman <sfeldma@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <net/switchdev.h>

/**
 *	netdev_sw_parent_id_get - Get ID of a switch
 *	@dev: port device
 *	@psid: switch ID
 *
 *	Get ID of a switch this port is part of.
 */
int netdev_sw_parent_id_get(struct net_device *dev,
			    struct netdev_phys_item_id *psid)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_sw_parent_id_get)
		return -EOPNOTSUPP;
	return ops->ndo_sw_parent_id_get(dev, psid);
}
EXPORT_SYMBOL(netdev_sw_parent_id_get);

/**
 *	netdev_sw_port_fdb_add - Add a fdb into switch port
 *	@dev: port device
 *	@addr: mac address
 *	@vid: vlan id
 *
 *	Add a fdb into switch port.
 */
int netdev_sw_port_fdb_add(struct net_device *dev,
			   const unsigned char *addr, u16 vid)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_sw_port_fdb_add)
		return -EOPNOTSUPP;
	WARN_ON(!ops->ndo_sw_parent_id_get);
	return ops->ndo_sw_port_fdb_add(dev, addr, vid);
}
EXPORT_SYMBOL(netdev_sw_port_fdb_add);

/**
 *	netdev_sw_port_fdb_del - Delete a fdb from switch port
 *	@dev: port device
 *	@addr: mac address
 *	@vid: vlan id
 *
 *	Delete a fdb from switch port.
 */
int netdev_sw_port_fdb_del(struct net_device *dev,
			   const unsigned char *addr, u16 vid)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_sw_port_fdb_del)
		return -EOPNOTSUPP;
	WARN_ON(!ops->ndo_sw_parent_id_get);
	return ops->ndo_sw_port_fdb_del(dev, addr, vid);
}
EXPORT_SYMBOL(netdev_sw_port_fdb_del);

/**
 *	netdev_sw_port_stp_update - Notify switch device port of STP
 *				    state change
 *	@dev: port device
 *	@state: port STP state
 *
 *	Notify switch device port of bridge port STP state change.
 */
int netdev_sw_port_stp_update(struct net_device *dev, u8 state)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_sw_port_stp_update)
		return -EOPNOTSUPP;
	WARN_ON(!ops->ndo_sw_parent_id_get);
	return ops->ndo_sw_port_stp_update(dev, state);
}
EXPORT_SYMBOL(netdev_sw_port_stp_update);
