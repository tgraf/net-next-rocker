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

struct swdev_flow;
typedef u64 swdev_features_t;

#include <linux/netdevice.h>

struct fib_info;

enum {
	SWDEV_F_FLOW_MATCH_KEY_BIT,	/* Supports fixed key match */
	/**/SWDEV_FEATURE_COUNT
};

#define __SWDEV_F_BIT(bit)	((swdev_features_t)1 << (bit))
#define __SWDEV_F(name)		__SWDEV_F_BIT(SWDEV_F_##name##_BIT)

#define SWDEV_F_FLOW_MATCH_KEY	__SWDEV_F(FLOW_MATCH_KEY)

struct swdev_flow_match_key {
	struct {
		u32	priority;	/* Packet QoS priority. */
		u32	in_port_ifindex; /* Input switch port ifindex (or 0). */
	} phy;
	struct {
		u8     src[ETH_ALEN];	/* Ethernet source address. */
		u8     dst[ETH_ALEN];	/* Ethernet destination address. */
		__be16 tci;		/* 0 if no VLAN, VLAN_TAG_PRESENT set otherwise. */
		__be16 type;		/* Ethernet frame type. */
	} eth;
	struct {
		u8     proto;		/* IP protocol or lower 8 bits of ARP opcode. */
		u8     tos;		/* IP ToS. */
		u8     ttl;		/* IP TTL/hop limit. */
		u8     frag;		/* One of OVS_FRAG_TYPE_*. */
	} ip;
	struct {
		__be16 src;		/* TCP/UDP/SCTP source port. */
		__be16 dst;		/* TCP/UDP/SCTP destination port. */
		__be16 flags;		/* TCP flags. */
	} tp;
	union {
		struct {
			struct {
				__be32 src;	/* IP source address. */
				__be32 dst;	/* IP destination address. */
			} addr;
			struct {
				u8 sha[ETH_ALEN];	/* ARP source hardware address. */
				u8 tha[ETH_ALEN];	/* ARP target hardware address. */
			} arp;
		} ipv4;
		struct {
			struct {
				struct in6_addr src;	/* IPv6 source address. */
				struct in6_addr dst;	/* IPv6 destination address. */
			} addr;
			__be32 label;			/* IPv6 flow label. */
			struct {
				struct in6_addr target;	/* ND target address. */
				u8 sll[ETH_ALEN];	/* ND source link layer address. */
				u8 tll[ETH_ALEN];	/* ND target link layer address. */
			} nd;
		} ipv6;
	};
};

enum swdev_flow_match_type {
	SW_FLOW_MATCH_TYPE_KEY,
};

struct swdev_flow_match {
	enum swdev_flow_match_type			type;
	union {
		struct {
			struct swdev_flow_match_key	key;
			struct swdev_flow_match_key	key_mask;
		};
	};
};

enum swdev_flow_action_type {
	SW_FLOW_ACTION_TYPE_OUTPUT,
	SW_FLOW_ACTION_TYPE_VLAN_PUSH,
	SW_FLOW_ACTION_TYPE_VLAN_POP,
};

struct swdev_flow_action {
	enum swdev_flow_action_type	type;
	union {
		u32			out_port_ifindex;
		struct {
			__be16		proto;
			__be16		tci;
		} vlan;
	};
};

struct swdev_flow {
	struct swdev_flow_match		match;
	unsigned			action_count;
	struct swdev_flow_action	action[0];
};

static inline struct swdev_flow *swdev_flow_alloc(unsigned action_count,
						  gfp_t flags)
{
	struct swdev_flow *flow;

	flow = kzalloc(sizeof(struct swdev_flow) +
		       sizeof(struct swdev_flow_action) * action_count,
		       flags);
	if (!flow)
		return NULL;
	flow->action_count = action_count;
	return flow;
}

#ifdef CONFIG_NET_SWITCHDEV

int netdev_sw_parent_id_get(struct net_device *dev,
			    struct netdev_phys_item_id *psid);
int netdev_sw_port_fdb_add(struct net_device *dev,
			   const unsigned char *addr, u16 vid);
int netdev_sw_port_fdb_del(struct net_device *dev,
			   const unsigned char *addr, u16 vid);
int netdev_sw_port_stp_update(struct net_device *dev, u8 state);
int netdev_sw_fib_ipv4_add(u32 dst, int dst_len, struct fib_info *fi, u8 tos,
			   u8 type, u32 tb_id);
int netdev_sw_fib_ipv4_del(u32 dst, int dst_len, struct fib_info *fi, u8 tos,
			   u8 type, u32 tb_id);
swdev_features_t netdev_sw_parent_features_get(struct net_device *dev);
int netdev_sw_parent_flow_insert(struct net_device *dev,
				 const struct swdev_flow *flow);
int netdev_sw_parent_flow_remove(struct net_device *dev,
				 const struct swdev_flow *flow);

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

static inline int netdev_sw_port_stp_update(struct net_device *dev, u8 state)
{
	return -EOPNOTSUPP;
}

static inline int netdev_sw_fib_ipv4_add(u32 dst, int dst_len,
					 struct fib_info *fi,
					 u8 tos, u8 type, u32 tb_id)
{
	return -EOPNOTSUPP;
}

static inline int netdev_sw_fib_ipv4_del(u32 dst, int dst_len,
					 struct fib_info *fi,
					 u8 tos, u8 type, u32 tb_id)
{
	return -EOPNOTSUPP;
}

static inline swdev_features_t netdev_sw_parent_features_get(struct net_device *dev)
{
	return 0;
}

static inline int netdev_sw_parent_flow_insert(struct net_device *dev,
					       const struct swdev_flow *flow)
{
	return -EOPNOTSUPP;
}

static inline int netdev_sw_parent_flow_remove(struct net_device *dev,
					       const struct swdev_flow *flow)
{
	return -EOPNOTSUPP;
}

#endif

#endif /* _LINUX_SWITCHDEV_H_ */
