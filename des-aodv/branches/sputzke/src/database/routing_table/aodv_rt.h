/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
       http://www.des-testbed.net
*******************************************************************************/

#ifndef AODV_RREQ_T
#define AODV_RREQ_T

#include <dessert.h>
#include <utlist.h>
#include <uthash.h>
#include "../../pipeline/aodv_pipeline.h"
#include "../timeslot.h"
#include "../aodv_database.h"
#include "../../config.h"
#include "../../helper.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

typedef struct aodv_rt_precursor_list_entry {
    mac_addr            addr; // ID
    dessert_meshif_t*   iface;
    UT_hash_handle      hh;
} aodv_rt_precursor_list_entry_t;

typedef struct aodv_rt_entry {
    mac_addr            addr; // ID
    mac_addr            next_hop;
    dessert_meshif_t*	output_iface;
    uint32_t            sequence_number;
    metric_t			metric;
    uint8_t				hop_count;
    /**
     * flags format: 0 0 0 0 0 0 U I
     * I - Invalid flag; route is invalid due of link breakage
     * U - next hop Unknown flag;
     */
    uint8_t				flags;
    aodv_rt_precursor_list_entry_t* precursor_list;
    UT_hash_handle		hh;
} aodv_rt_entry_t;


typedef struct aodv_rt {
    aodv_rt_entry_t*	entries;
    timeslot_t*			ts;
} aodv_rt_t;

/**
 * Mapping next_hop -> destination list
 */
typedef struct nht_destlist_entry {
    uint8_t				destination_host[ETH_ALEN];
    aodv_rt_entry_t*	rt_entry;
    UT_hash_handle		hh;
} nht_destlist_entry_t;

typedef struct nht_entry {
    uint8_t				destination_host_next_hop[ETH_ALEN];
    nht_destlist_entry_t*		dest_list;
    UT_hash_handle			hh;
} nht_entry_t;

int aodv_db_rt_init();

int aodv_db_rt_capt_rreq(mac_addr destination_host,
                         mac_addr originator_host,
                         mac_addr originator_host_prev_hop,
                         dessert_meshif_t* output_iface,
                         uint32_t originator_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval* timestamp,
                         aodv_capt_rreq_result_t* result_out);

int aodv_db_rt_capt_rrep(mac_addr destination_host,
                         mac_addr destination_host_next_hop,
                         dessert_meshif_t* output_iface,
                         uint32_t destination_sequence_number,
                         metric_t metric,
                         uint8_t hop_count,
                         struct timeval* timestamp);

int aodv_db_rt_getroute2dest(mac_addr destination_host, mac_addr destination_host_next_hop_out,
                             dessert_meshif_t** output_iface_out, struct timeval* timestamp, uint8_t flags);

int aodv_db_rt_getnexthop(mac_addr destination_host, mac_addr destination_host_next_hop_out);

int aodv_db_rt_getprevhop(mac_addr destination_host, mac_addr originator_host,
                          mac_addr originator_host_prev_hop_out, dessert_meshif_t** output_iface_out);

int aodv_db_rt_get_destination_sequence_number(mac_addr destination_host, uint32_t* destination_sequence_number_out);

int aodv_db_rt_get_quantity(mac_addr dhost_ether, mac_addr shost_ether, uint32_t* quantity_out);
int aodv_db_rt_get_hopcount(mac_addr destination_host, uint8_t* hop_count_out);
int aodv_db_rt_get_metric(mac_addr destination_host, metric_t* last_metric_out);

int aodv_db_rt_markrouteinv(mac_addr destination_host, uint32_t destination_sequence_number);
int aodv_db_rt_remove_nexthop(mac_addr next_hop);
int aodv_db_rt_inv_over_nexthop(mac_addr next_hop);
int aodv_db_rt_get_destlist(mac_addr dhost_next_hop, aodv_link_break_element_t** destlist);
int aodv_db_rt_add_precursor(mac_addr destination, mac_addr precursor, dessert_meshif_t *iface);

int aodv_db_rt_get_warn_endpoints_from_neighbor_and_set_warn(mac_addr neighbor, aodv_link_break_element_t** head);
int aodv_db_rt_get_warn_status(mac_addr dhost_ether);

int aodv_db_rt_get_active_routes(aodv_link_break_element_t** head);

int aodv_db_rt_cleanup(struct timeval* timestamp);
int aodv_db_rt_routing_reset(uint32_t* count_out);

int aodv_db_rt_report(char** str_out);

#endif
