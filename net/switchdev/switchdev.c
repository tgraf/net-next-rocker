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
#include <net/ip_fib.h>
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

static struct net_device *swdev_dev_get_by_fib_dev(struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	struct net_device *lower_dev;
	struct net_device *port_dev;
	struct list_head *iter;

	/* Recusively search from fib_dev down until we find
	 * a sw port dev.  (A sw port dev supports
	 * ndo_sw_parent_id_get).
	 */

	if (ops->ndo_sw_parent_id_get)
		return dev;

	netdev_for_each_lower_dev(dev, lower_dev, iter) {
		port_dev = swdev_dev_get_by_fib_dev(lower_dev);
		if (port_dev)
			return port_dev;
	}

	return NULL;
}

int netdev_sw_fib_ipv4_add(u32 dst, int dst_len, struct fib_info *fi, u8 tos,
			   u8 type, u32 tb_id)
{
	struct net_device *dev;
	const struct net_device_ops *ops;
	int err = -EOPNOTSUPP;

	dev = swdev_dev_get_by_fib_dev(fi->fib_dev);
	if (!dev)
		return -EOPNOTSUPP;
	ops = dev->netdev_ops;

	if (ops->ndo_sw_parent_fib_ipv4_add)
		err = ops->ndo_sw_parent_fib_ipv4_add(dev, htonl(dst), dst_len,
						      fi, tos, type, tb_id);

	return err;
}
EXPORT_SYMBOL(netdev_sw_fib_ipv4_add);

int netdev_sw_fib_ipv4_del(u32 dst, int dst_len, struct fib_info *fi, u8 tos,
			   u8 type, u32 tb_id)
{
	struct net_device *dev;
	const struct net_device_ops *ops;
	int err = -EOPNOTSUPP;

	dev = swdev_dev_get_by_fib_dev(fi->fib_dev);
	if (!dev)
		return -EOPNOTSUPP;
	ops = dev->netdev_ops;

	if (ops->ndo_sw_parent_fib_ipv4_del)
		err = ops->ndo_sw_parent_fib_ipv4_del(dev, htonl(dst), dst_len,
						      fi, tos, type, tb_id);

	return err;
}
EXPORT_SYMBOL(netdev_sw_fib_ipv4_del);

/**
 *	netdev_sw_parent_features_get - Get list of features switch supports
 *	@dev: port device
 *
 *	Get list of features switch this port is part of supports.
 */
swdev_features_t netdev_sw_parent_features_get(struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_sw_parent_features_get)
		return 0;
	return ops->ndo_sw_parent_features_get(dev);
}
EXPORT_SYMBOL(netdev_sw_parent_features_get);

static void print_flow_key_phy(const char *prefix,
			       const struct swdev_flow_match_key *key)
{
	pr_debug("%s phy  { prio %08x, in_port_ifindex %08x }\n",
		 prefix,
		 key->phy.priority, key->phy.in_port_ifindex);
}

static void print_flow_key_eth(const char *prefix,
			       const struct swdev_flow_match_key *key)
{
	pr_debug("%s eth  { sm %pM, dm %pM, tci %04x, type %04x }\n",
		 prefix,
		 key->eth.src, key->eth.dst, ntohs(key->eth.tci),
		 ntohs(key->eth.type));
}

static void print_flow_key_ip(const char *prefix,
			      const struct swdev_flow_match_key *key)
{
	pr_debug("%s ip   { proto %02x, tos %02x, ttl %02x }\n",
		 prefix,
		 key->ip.proto, key->ip.tos, key->ip.ttl);
}

static void print_flow_key_ipv4(const char *prefix,
				const struct swdev_flow_match_key *key)
{
	pr_debug("%s ipv4 { si %pI4, di %pI4, sm %pM, dm %pM }\n",
		 prefix,
		 &key->ipv4.addr.src, &key->ipv4.addr.dst,
		 key->ipv4.arp.sha, key->ipv4.arp.tha);
}

static void print_flow_actions(const struct swdev_flow_action *action,
			       unsigned action_count)
{
	int i;

	pr_debug("  actions:\n");
	for (i = 0; i < action_count; i++) {
		switch (action->type) {
		case SW_FLOW_ACTION_TYPE_OUTPUT:
			pr_debug("    output    { ifindex %u }\n",
				 action->out_port_ifindex);
			break;
		case SW_FLOW_ACTION_TYPE_VLAN_PUSH:
			pr_debug("    vlan push { proto %04x, tci %04x }\n",
				 ntohs(action->vlan.proto),
				 ntohs(action->vlan.tci));
			break;
		case SW_FLOW_ACTION_TYPE_VLAN_POP:
			pr_debug("    vlan pop\n");
			break;
		}
		action++;
	}
}

#define PREFIX_NONE "      "
#define PREFIX_MASK "  mask"

static void print_flow_match(const struct swdev_flow_match *match)
{
	switch (match->type) {
	case SW_FLOW_MATCH_TYPE_KEY:
		print_flow_key_phy(PREFIX_NONE, &match->key);
		print_flow_key_phy(PREFIX_MASK, &match->key_mask);
		print_flow_key_eth(PREFIX_NONE, &match->key);
		print_flow_key_eth(PREFIX_MASK, &match->key_mask);
		print_flow_key_ip(PREFIX_NONE, &match->key);
		print_flow_key_ip(PREFIX_MASK, &match->key_mask);
		print_flow_key_ipv4(PREFIX_NONE, &match->key);
		print_flow_key_ipv4(PREFIX_MASK, &match->key_mask);
	}
}

static void print_flow(const struct swdev_flow *flow, struct net_device *dev,
		       const char *comment)
{
	pr_debug("%s flow %s:\n", dev->name, comment);
	print_flow_match(&flow->match);
	print_flow_actions(flow->action, flow->action_count);
}

static int check_match_type_features(struct net_device *dev,
				     const struct swdev_flow *flow)
{
	if (flow->match.type == SW_FLOW_MATCH_TYPE_KEY &&
	    !(netdev_sw_parent_features_get(dev) & SWDEV_F_FLOW_MATCH_KEY))
		return -EOPNOTSUPP;
	return 0;
}

/**
 *	netdev_sw_parent_flow_insert - Insert a flow into switch
 *	@dev: port device
 *	@flow: flow descriptor
 *
 *	Insert a flow into switch this port is part of.
 */
int netdev_sw_parent_flow_insert(struct net_device *dev,
				 const struct swdev_flow *flow)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	int err;

	print_flow(flow, dev, "insert");
	if (!ops->ndo_sw_parent_flow_insert)
		return -EOPNOTSUPP;
	err = check_match_type_features(dev, flow);
	if (err)
		return err;
	WARN_ON(!ops->ndo_sw_parent_id_get);
	return ops->ndo_sw_parent_flow_insert(dev, flow);
}
EXPORT_SYMBOL(netdev_sw_parent_flow_insert);

/**
 *	netdev_sw_parent_flow_remove - Remove a flow from switch
 *	@dev: port device
 *	@flow: flow descriptor
 *
 *	Remove a flow from switch this port is part of.
 */
int netdev_sw_parent_flow_remove(struct net_device *dev,
				 const struct swdev_flow *flow)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	int err;

	print_flow(flow, dev, "remove");
	if (!ops->ndo_sw_parent_flow_remove)
		return -EOPNOTSUPP;
	err = check_match_type_features(dev, flow);
	if (err)
		return err;
	WARN_ON(!ops->ndo_sw_parent_id_get);
	return ops->ndo_sw_parent_flow_remove(dev, flow);
}
EXPORT_SYMBOL(netdev_sw_parent_flow_remove);
