/******************************************************************************
 Copyright 2009, 2010, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
 at Freie Universitaet Berlin (http://www.fu-berlin.de/),
 Computer Systems and Telematics / Distributed, Embedded Systems (DES) group
 (http://cst.mi.fu-berlin.de/, http://www.des-testbed.net/)
 ------------------------------------------------------------------------------
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program. If not, see http://www.gnu.org/licenses/ .
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
 http://www.des-testbed.net/
 ------------------------------------------------------------------------------

 ******************************************************************************/

#ifndef HELPER_H_
#define HELPER_H_

#include "dsr.h"

#define DSR_PATH_LINK_FOUND 0
#define DSR_PATH_NO_SUCH_LINK -1

typedef struct __attribute__((__packed__)) dsr_path {
    struct dsr_path* prev;
    struct dsr_path* next;
    int len;
    uint32_t weight;
    uint8_t address[DSR_SOURCE_MAX_ADDRESSES_IN_OPTION* ETHER_ADDR_LEN];
} dsr_path_t;

typedef uint16_t dsr_rreq_identification_t;
#define DSR_MAX_RREQ_IDENTIFICATION ((uint16_t) -1)
inline dsr_rreq_identification_t dsr_new_rreq_identification();
inline uint16_t dsr_new_ackreq_identification();

inline int dsr_msg_add_ackreq_ext(dessert_msg_t* msg, const uint16_t id);
inline int dsr_msg_add_rreq_ext(dessert_msg_t* msg, const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const uint8_t ttl, const dessert_meshif_t* meshif);
inline dsr_repl_ext_t* dsr_msg_add_repl_ext(dessert_msg_t* repl_msg, dsr_rreq_ext_t* rreq, const dessert_meshif_t* iface);
inline dsr_rerr_ext_t* dsr_msg_add_rerr_ext(dessert_msg_t* msg,
    const uint8_t salvage, const uint8_t error_source_address[ETHER_ADDR_LEN],
    const uint8_t error_destination_address[ETHER_ADDR_LEN],
    const uint8_t type_specific_information[6]);
inline dsr_source_ext_t* dsr_msg_add_source_ext(dessert_msg_t* msg, dsr_path_t* path, int segments_left);

#if (LINKCACHE == 0)
inline void dsr_msg_cache_repl_ext_to_routecache(dsr_repl_ext_t* repl);
#endif

#if (LINKCACHE == 1)
inline void dsr_msg_cache_source_ext_to_linkcache(dsr_source_ext_t* source, int repl_present);

#if (METRIC != HC)
inline void dsr_msg_cache_repl_ext_to_linkcache(dsr_repl_ext_t* repl);

#endif
#endif

inline int dsr_msg_send_with_route_maintenance(dessert_msg_t* msg, dessert_meshif_t* in_iface, dessert_meshif_t* out_iface, int nexthop_reachability);
inline int dsr_msg_send_with_route_maintenance_delay(dessert_msg_t* msg, const dessert_meshif_t* in_iface, const dessert_meshif_t* out_iface, __suseconds_t delay, int nexthop_reachability);

inline void dsr_msg_send_via_path(dessert_msg_t* msg, dsr_path_t* path);
inline void dsr_msg_send_via_path_delay(dessert_msg_t* msg, dsr_path_t* path, __suseconds_t delay);

inline int dsr_send_rerr_for_msg(dessert_msg_t* msg, uint8_t in_iface[ETHER_ADDR_LEN], uint8_t out_iface[ETHER_ADDR_LEN]);
inline void dsr_send_repl(dessert_meshif_t* iface, dsr_rreq_ext_t* rreq);
inline void dsr_propagate_rreq(dessert_meshif_t* iface, dessert_msg_t* msg, dsr_rreq_ext_t* rreq, dessert_ext_t* rreq_ext);

inline void dsr_do_route_discovery(const uint8_t dest[ETHER_ADDR_LEN]);
inline int dsr_send_rreq(const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const uint8_t ttl);
inline int dsr_send_rreq_piggyback(const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const dessert_msg_t* msg, const uint8_t ttl);

inline int dsr_patharray_get_index(uint8_t u[ETHER_ADDR_LEN], uint8_t* path, int path_len);
inline int dsr_patharray_contains_link(uint8_t* path, int path_len, const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
inline void dsr_patharray_reverse(uint8_t* to_revd, uint8_t* from_orig, int path_len);

inline int dsr_path_get_index(uint8_t u[ETHER_ADDR_LEN], dsr_path_t* path);

inline int dsr_path_contains_link(dsr_path_t* path, const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
inline int dsr_path_is_linkdisjoint(dsr_path_t* p1, dsr_path_t* p2);

inline void dsr_path_reverse(dsr_path_t* to_revd, dsr_path_t* from_orig);
inline void dsr_path_new_from_patharray(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight);
inline void dsr_path_new_from_reversed_patharray(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight);
inline void dsr_path_new_from_rreq(dsr_path_t** path, dsr_rreq_ext_t* rreq);

inline uint32_t dsr_rreq_get_weight_incl_hop_to_self(const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq);
inline uint32_t dsr_rreq_get_weight(dsr_rreq_ext_t* rreq);

inline void dsr_path_print_to_debug(dsr_path_t* path);

inline int dsr_path_cmp(dsr_path_t* p1, dsr_path_t* p2);
inline int dsr_path_hops_ident(dsr_path_t* p1, dsr_path_t* p2);

#endif /* HELPER_H_ */
