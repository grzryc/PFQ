/***************************************************************
 *
 * (C) 2011-14 Nicola Bonelli <nicola.bonelli@cnit.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 ****************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>

#include <pf_q-module.h>

#include "inline.h"
#include "predicate.h"


static bool
pred_is_ip(arguments_t args, struct sk_buff const *skb)
{
        return  is_ip(skb);
}

static bool
pred_is_ip6(arguments_t args, struct sk_buff const *skb)
{
        return  is_ip6(skb);
}

static bool
pred_is_udp(arguments_t args, struct sk_buff const *skb)
{
        return  is_udp(skb);
}

static bool
pred_is_udp6(arguments_t args, struct sk_buff const *skb)
{
        return  is_udp6(skb);
}

static bool
pred_is_tcp(arguments_t args, struct sk_buff const *skb)
{
        return  is_tcp(skb);
}

static bool
pred_is_tcp6(arguments_t args, struct sk_buff const *skb)
{
        return  is_tcp6(skb);
}

static bool
pred_is_icmp(arguments_t args, struct sk_buff const *skb)
{
        return  is_icmp(skb);
}

static bool
pred_is_icmp6(arguments_t args, struct sk_buff const *skb)
{
        return  is_icmp6(skb);
}

static bool
pred_is_flow(arguments_t args, struct sk_buff const *skb)
{
        return  is_flow(skb);
}

static bool
pred_is_l3_proto(arguments_t args, struct sk_buff const *skb)
{
	const u16 type = get_data(u16, args);
	return is_l3_proto(skb, type);
}

static bool
pred_is_l4_proto(arguments_t args, struct sk_buff const *skb)
{
	const u8 protocol = get_data(u8, args);
	return is_l4_proto(skb, protocol);
}

static bool
pred_has_port(arguments_t args, struct sk_buff const *skb)
{
	const u16 port = get_data(u16, args);
	return has_port(skb, port);
}

static bool
pred_has_src_port(arguments_t args, struct sk_buff const *skb)
{
	const u16 port = get_data(u16, args);
	return has_src_port(skb, port);
}

static bool
pred_has_dst_port(arguments_t args, struct sk_buff const *skb)
{
	const u16 port = get_data(u16, args);
	return has_dst_port(skb, port);
}

static bool
pred_has_vlan(arguments_t args, struct sk_buff const *skb)
{
        return  has_vlan(skb);
}

static bool
pred_has_vid(arguments_t args, struct sk_buff const *skb)
{
	const int id = get_data(int, args);
        return  has_vid(skb, id);
}

static bool
pred_has_mark(arguments_t args, struct sk_buff const *skb)
{
	const unsigned long value = get_data(unsigned long, args);
	return get_state(skb) == value;
}


static int pred_addr_init(arguments_t args)
{
	struct network_addr {
	 	uint32_t addr;
	 	int 	 prefix;
	} data = get_data(struct network_addr, args);

	uint32_t ipv4 = data.addr;
	uint32_t mask = make_mask(data.prefix);

	set_data (args, ipv4);
	set_data2(args, mask);

	pr_devel("[PFQ|init] predicate: addr:%pI4 mask:%pI4\n", &ipv4, &mask);

	return 0;
}


static bool
pred_has_addr(arguments_t args, struct sk_buff const *skb)
{
	uint32_t addr = get_data(uint32_t, args);
	uint32_t mask = get_data2(uint32_t, args);

	return has_addr(skb, addr, mask);
}


static bool
pred_has_src_addr(arguments_t args, struct sk_buff const *skb)
{
	uint32_t addr = get_data(uint32_t, args);
	uint32_t mask = get_data2(uint32_t, args);

	return has_src_addr(skb, addr, mask);
}

static bool
pred_has_dst_addr(arguments_t args, struct sk_buff const *skb)
{
	uint32_t addr = get_data(uint32_t, args);
	uint32_t mask = get_data2(uint32_t, args);

	return has_dst_addr(skb, addr, mask);
}

static bool
pred_is_frag(arguments_t args, struct sk_buff const *skb)
{
        return  is_frag(skb);
}

static bool
pred_is_first_frag(arguments_t args, struct sk_buff const *skb)
{
        return  is_first_frag(skb);
}

static bool
pred_is_more_frag(arguments_t args, struct sk_buff const *skb)
{
        return  is_more_frag(skb);
}

struct pfq_predicate_fun_descr predicate_functions[] = {

        { "less", 	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , less	 },
        { "less_eq", 	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , less_eq	 },
        { "greater", 	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , greater 	 },
        { "greater_eq",  FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , greater_eq },
        { "equal",  	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , equal	 },
        { "not_equal",   FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , not_equal	 },
        { "any_bit", 	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , any_bit	 },
        { "all_bit", 	 FUN_PREDICATE | FUN_ARG_DATA | FUN_ARG_FUN , all_bit	 },

        { "is_ip", 	 FUN_PREDICATE, pred_is_ip    },
        { "is_tcp",      FUN_PREDICATE, pred_is_tcp   },
        { "is_udp",      FUN_PREDICATE, pred_is_udp   },
        { "is_icmp",     FUN_PREDICATE, pred_is_icmp  },
        { "is_ip6",	 FUN_PREDICATE, pred_is_ip6   },
        { "is_udp6",	 FUN_PREDICATE, pred_is_udp6  },
        { "is_tcp6",     FUN_PREDICATE, pred_is_tcp6  },
        { "is_icmp6",    FUN_PREDICATE, pred_is_icmp6 },
        { "is_flow",     FUN_PREDICATE, pred_is_flow  },
        { "has_vlan",    FUN_PREDICATE, pred_has_vlan },
        { "is_frag", 	 FUN_PREDICATE, pred_is_frag  },
        { "is_first_frag",FUN_PREDICATE, pred_is_first_frag },
        { "is_more_frag", FUN_PREDICATE, pred_is_more_frag },

        { "is_l3_proto", FUN_PREDICATE | FUN_ARG_DATA, pred_is_l3_proto  },
        { "is_l4_proto", FUN_PREDICATE | FUN_ARG_DATA, pred_is_l4_proto  },
        { "has_port",    FUN_PREDICATE | FUN_ARG_DATA, pred_has_port     },
        { "has_src_port",FUN_PREDICATE | FUN_ARG_DATA, pred_has_src_port },
        { "has_dst_port",FUN_PREDICATE | FUN_ARG_DATA, pred_has_dst_port },
        { "has_vid",     FUN_PREDICATE | FUN_ARG_DATA, pred_has_vid      },
        { "has_mark",    FUN_PREDICATE | FUN_ARG_DATA, pred_has_mark     },
        { "has_addr",    FUN_PREDICATE | FUN_ARG_DATA, pred_has_addr     , pred_addr_init },
        { "has_src_addr",FUN_PREDICATE | FUN_ARG_DATA, pred_has_src_addr , pred_addr_init },
        { "has_dst_addr",FUN_PREDICATE | FUN_ARG_DATA, pred_has_dst_addr , pred_addr_init },

        { NULL }};
