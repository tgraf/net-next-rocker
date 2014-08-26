/*
 * net/switchdev/switchdev_netlink.c - Netlink interface to Switch device
 * Copyright (c) 2014 Jiri Pirko <jiri@resnulli.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/switchdev.h>
#include <net/netlink.h>
#include <net/genetlink.h>
#include <net/netlink.h>
#include <uapi/linux/switchdev.h>

static struct genl_family swdev_nl_family = {
	.id		= GENL_ID_GENERATE,
	.name		= SWITCHDEV_GENL_NAME,
	.version	= SWITCHDEV_GENL_VERSION,
	.maxattr	= SWDEV_ATTR_MAX,
	.netnsok	= true,
};

static const struct nla_policy swdev_nl_flow_policy[SWDEV_ATTR_FLOW_MAX + 1] = {
	[SWDEV_ATTR_FLOW_UNSPEC]		= { .type = NLA_UNSPEC, },
	[SWDEV_ATTR_FLOW_MATCH_KEY]		= { .type = NLA_NESTED },
	[SWDEV_ATTR_FLOW_MATCH_KEY_MASK]	= { .type = NLA_NESTED },
	[SWDEV_ATTR_FLOW_LIST_ACTION]		= { .type = NLA_NESTED },
};

#define __IN6_ALEN sizeof(struct in6_addr)

static const struct nla_policy
swdev_nl_flow_match_key_policy[SWDEV_ATTR_FLOW_MATCH_KEY_MAX + 1] = {
	[SWDEV_ATTR_FLOW_MATCH_KEY_UNSPEC]		= { .type = NLA_UNSPEC, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_PRIORITY]	= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_IN_PORT]	= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_SRC]		= { .len  = ETH_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_DST]		= { .len  = ETH_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TCI]		= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TYPE]		= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IP_PROTO]		= { .type = NLA_U8, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TOS]		= { .type = NLA_U8, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TTL]		= { .type = NLA_U8, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IP_FRAG]		= { .type = NLA_U8, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_TP_SRC]		= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_TP_DST]		= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_TP_FLAGS]		= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_SRC]	= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_DST]	= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_SHA]	= { .len  = ETH_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_THA]	= { .len  = ETH_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_SRC]	= { .len  = __IN6_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_DST]	= { .len  = __IN6_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_LABEL]	= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TARGET]	= { .len  = __IN6_ALEN, },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_SLL]	= { .len  = ETH_ALEN },
	[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TLL]	= { .len  = ETH_ALEN },
};

static const struct nla_policy
swdev_nl_flow_action_policy[SWDEV_ATTR_FLOW_ACTION_MAX + 1] = {
	[SWDEV_ATTR_FLOW_ACTION_UNSPEC]		= { .type = NLA_UNSPEC, },
	[SWDEV_ATTR_FLOW_ACTION_TYPE]		= { .type = NLA_U32, },
	[SWDEV_ATTR_FLOW_ACTION_VLAN_PROTO]	= { .type = NLA_U16, },
	[SWDEV_ATTR_FLOW_ACTION_VLAN_TCI]	= { .type = NLA_U16, },
};

static int swdev_nl_cmd_noop(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	void *hdr;
	int err;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			  &swdev_nl_family, 0, SWDEV_CMD_NOOP);
	if (!hdr) {
		err = -EMSGSIZE;
		goto err_msg_put;
	}

	genlmsg_end(msg, hdr);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);

err_msg_put:
	nlmsg_free(msg);

	return err;
}

static int swdev_nl_parse_flow_match_key(struct nlattr *key_attr,
					 struct swdev_flow_match_key *key)
{
	struct nlattr *attrs[SWDEV_ATTR_FLOW_MATCH_KEY_MAX + 1];
	int err;

	err = nla_parse_nested(attrs, SWDEV_ATTR_FLOW_MATCH_KEY_MAX,
			       key_attr, swdev_nl_flow_match_key_policy);
	if (err)
		return err;

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_PRIORITY])
		key->phy.priority =
			nla_get_u32(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_PRIORITY]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_IN_PORT])
		key->phy.in_port_ifindex =
			nla_get_u32(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_PHY_IN_PORT]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_SRC])
		ether_addr_copy(key->eth.src,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_SRC]));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_DST])
		ether_addr_copy(key->eth.dst,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_DST]));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TCI])
		key->eth.tci =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TCI]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TYPE])
		key->eth.type =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_ETH_TYPE]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_PROTO])
		key->ip.proto =
			nla_get_u8(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_PROTO]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TOS])
		key->ip.tos =
			nla_get_u8(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TOS]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TTL])
		key->ip.ttl =
			nla_get_u8(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_TTL]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_FRAG])
		key->ip.frag =
			nla_get_u8(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IP_FRAG]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_SRC])
		key->tp.src =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_SRC]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_DST])
		key->tp.dst =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_DST]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_FLAGS])
		key->tp.flags =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_TP_FLAGS]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_SRC])
		key->ipv4.addr.src =
			nla_get_be32(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_SRC]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_DST])
		key->ipv4.addr.dst =
			nla_get_be32(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ADDR_DST]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_SHA])
		ether_addr_copy(key->ipv4.arp.sha,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_SHA]));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_THA])
		ether_addr_copy(key->ipv4.arp.tha,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV4_ARP_THA]));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_SRC])
		memcpy(&key->ipv6.addr.src,
		       nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_SRC]),
		       sizeof(key->ipv6.addr.src));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_DST])
		memcpy(&key->ipv6.addr.dst,
		       nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ADDR_DST]),
		       sizeof(key->ipv6.addr.dst));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_LABEL])
		key->ipv6.label =
			nla_get_be32(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_LABEL]);

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TARGET])
		memcpy(&key->ipv6.nd.target,
		       nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TARGET]),
		       sizeof(key->ipv6.nd.target));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_SLL])
		ether_addr_copy(key->ipv6.nd.sll,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_SLL]));

	if (attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TLL])
		ether_addr_copy(key->ipv6.nd.tll,
				nla_data(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_IPV6_ND_TLL]));

	return 0;
}

static int swdev_nl_parse_flow_action(struct nlattr *action_attr,
				      struct swdev_flow_action *flow_action)
{
	struct nlattr *attrs[SWDEV_ATTR_FLOW_ACTION_MAX + 1];
	int err;

	err = nla_parse_nested(attrs, SWDEV_ATTR_FLOW_ACTION_MAX,
			       action_attr, swdev_nl_flow_action_policy);
	if (err)
		return err;

	if (!attrs[SWDEV_ATTR_FLOW_ACTION_TYPE])
		return -EINVAL;

	switch (nla_get_u32(attrs[SWDEV_ATTR_FLOW_ACTION_TYPE])) {
	case SWDEV_FLOW_ACTION_TYPE_OUTPUT:
		if (!attrs[SWDEV_ATTR_FLOW_ACTION_OUT_PORT])
			return -EINVAL;
		flow_action->out_port_ifindex =
			nla_get_u32(attrs[SWDEV_ATTR_FLOW_ACTION_OUT_PORT]);
		flow_action->type = SW_FLOW_ACTION_TYPE_OUTPUT;
		break;
	case SWDEV_FLOW_ACTION_TYPE_VLAN_PUSH:
		if (!attrs[SWDEV_ATTR_FLOW_ACTION_VLAN_PROTO] ||
		    !attrs[SWDEV_ATTR_FLOW_ACTION_VLAN_TCI])
			return -EINVAL;
		flow_action->vlan.proto =
			nla_get_be16(attrs[SWDEV_ATTR_FLOW_ACTION_VLAN_PROTO]);
		flow_action->vlan.tci =
			nla_get_u16(attrs[SWDEV_ATTR_FLOW_ACTION_VLAN_TCI]);
		flow_action->type = SW_FLOW_ACTION_TYPE_VLAN_PUSH;
		break;
	case SWDEV_FLOW_ACTION_TYPE_VLAN_POP:
		flow_action->type = SW_FLOW_ACTION_TYPE_VLAN_POP;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int swdev_nl_parse_flow_actions(struct nlattr *actions_attr,
				       struct swdev_flow_action *action)
{
	struct swdev_flow_action *cur;
	struct nlattr *action_attr;
	int rem;
	int err;

	cur = action;
	nla_for_each_nested(action_attr, actions_attr, rem) {
		err = swdev_nl_parse_flow_action(action_attr, cur);
		if (err)
			return err;
		cur++;
	}
	return 0;
}

static int swdev_nl_parse_flow_action_count(struct nlattr *actions_attr,
					    unsigned *p_action_count)
{
	struct nlattr *action_attr;
	int rem;
	int count = 0;

	nla_for_each_nested(action_attr, actions_attr, rem) {
		if (nla_type(action_attr) != SWDEV_ATTR_FLOW_ITEM_ACTION)
			return -EINVAL;
		count++;
	}
	*p_action_count = count;
	return 0;
}

static void swdev_nl_free_flow(struct swdev_flow *flow)
{
	kfree(flow);
}

static int swdev_nl_parse_flow(struct nlattr *flow_attr, struct swdev_flow **p_flow)
{
	struct swdev_flow *flow;
	struct nlattr *attrs[SWDEV_ATTR_FLOW_MAX + 1];
	unsigned action_count;
	int err;

	err = nla_parse_nested(attrs, SWDEV_ATTR_FLOW_MAX,
			       flow_attr, swdev_nl_flow_policy);
	if (err)
		return err;

	if (!attrs[SWDEV_ATTR_FLOW_MATCH_KEY] ||
	    !attrs[SWDEV_ATTR_FLOW_MATCH_KEY_MASK] ||
	    !attrs[SWDEV_ATTR_FLOW_LIST_ACTION])
		return -EINVAL;

	err = swdev_nl_parse_flow_action_count(attrs[SWDEV_ATTR_FLOW_LIST_ACTION],
					       &action_count);
	if (err)
		return err;
	flow = swdev_flow_alloc(action_count, GFP_KERNEL);
	if (!flow)
		return -ENOMEM;

	err = swdev_nl_parse_flow_match_key(attrs[SWDEV_ATTR_FLOW_MATCH_KEY],
					    &flow->match.key);
	if (err)
		goto out;

	err = swdev_nl_parse_flow_match_key(attrs[SWDEV_ATTR_FLOW_MATCH_KEY_MASK],
					    &flow->match.key_mask);
	if (err)
		goto out;

	err = swdev_nl_parse_flow_actions(attrs[SWDEV_ATTR_FLOW_LIST_ACTION],
					  flow->action);
	if (err)
		goto out;

	*p_flow = flow;
	return 0;

out:
	kfree(flow);
	return err;
}

static struct net_device *swdev_nl_dev_get(struct genl_info *info)
{
	struct net *net = genl_info_net(info);
	int ifindex;

	if (!info->attrs[SWDEV_ATTR_IFINDEX])
		return NULL;

	ifindex = nla_get_u32(info->attrs[SWDEV_ATTR_IFINDEX]);
	return dev_get_by_index(net, ifindex);
}

static void swdev_nl_dev_put(struct net_device *dev)
{
	dev_put(dev);
}

static int swdev_nl_cmd_flow_insert(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev;
	struct swdev_flow *flow;
	int err;

	if (!info->attrs[SWDEV_ATTR_FLOW])
		return -EINVAL;

	dev = swdev_nl_dev_get(info);
	if (!dev)
		return -EINVAL;

	err = swdev_nl_parse_flow(info->attrs[SWDEV_ATTR_FLOW], &flow);
	if (err)
		goto dev_put;

	err = netdev_sw_parent_flow_insert(dev, flow);
	swdev_nl_free_flow(flow);
dev_put:
	swdev_nl_dev_put(dev);
	return err;
}

static int swdev_nl_cmd_flow_remove(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *dev;
	struct swdev_flow *flow;
	int err;

	if (!info->attrs[SWDEV_ATTR_FLOW])
		return -EINVAL;

	dev = swdev_nl_dev_get(info);
	if (!dev)
		return -EINVAL;

	err = swdev_nl_parse_flow(info->attrs[SWDEV_ATTR_FLOW], &flow);
	if (err)
		goto dev_put;

	err = netdev_sw_parent_flow_remove(dev, flow);
	swdev_nl_free_flow(flow);
dev_put:
	swdev_nl_dev_put(dev);
	return err;
}

static const struct genl_ops swdev_nl_ops[] = {
	{
		.cmd = SWDEV_CMD_NOOP,
		.doit = swdev_nl_cmd_noop,
	},
	{
		.cmd = SWDEV_CMD_FLOW_INSERT,
		.doit = swdev_nl_cmd_flow_insert,
		.policy = swdev_nl_flow_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SWDEV_CMD_FLOW_REMOVE,
		.doit = swdev_nl_cmd_flow_remove,
		.policy = swdev_nl_flow_policy,
		.flags = GENL_ADMIN_PERM,
	},
};

static int __init swdev_nl_module_init(void)
{
	return genl_register_family_with_ops(&swdev_nl_family, swdev_nl_ops);
}

static void swdev_nl_module_fini(void)
{
	genl_unregister_family(&swdev_nl_family);
}

module_init(swdev_nl_module_init);
module_exit(swdev_nl_module_fini);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jiri Pirko <jiri@resnulli.us>");
MODULE_DESCRIPTION("Netlink interface to Switch device");
MODULE_ALIAS_GENL_FAMILY(SWITCHDEV_GENL_NAME);
