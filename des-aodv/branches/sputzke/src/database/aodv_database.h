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

#ifndef AODV_DATABASE
#define AODV_DATABASE

#include <dessert.h>

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

#include "../config.h"

/** initialize all tables of routing database */
int aodv_db_init();

int aodv_db_neighbor_reset(uint32_t* count_out);

/**
 * Functions for pdr tracking
 */
int aodv_db_pdr_neighbor_reset(uint32_t* count_out);

int aodv_db_pdr_upd_expected(uint16_t new_interval);

int aodv_db_pdr_cap_hello(mac_addr ether_neighbor_addr, uint16_t hello_seq, uint16_t hello_interv, struct timeval* timestamp);

int aodv_db_pdr_cap_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_interv, uint8_t hello_count, struct timeval* timestamp);

int aodv_db_pdr_get_pdr(mac_addr ether_neighbor_addr, uint16_t* pdr_out, struct timeval* timestamp);

int aodv_db_pdr_get_etx_mul(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp);

int aodv_db_pdr_get_etx_add(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp);

int aodv_db_pdr_get_rcvdhellocount(mac_addr ether_neighbor_addr, uint8_t* count_out, struct timeval* timestamp);

/**END: Functions for pdr tracking*/

/** cleanup (purge) old entries from all database tables except from pdr_tracker */
int aodv_db_cleanup(struct timeval* timestamp);

void aodv_db_push_packet(mac_addr dhost_ether, dessert_msg_t* msg, struct timeval* timestamp);

dessert_msg_t* aodv_db_pop_packet(mac_addr dhost_ether);

typedef enum aodv_capt_rreq_result {
    AODV_CAPT_RREQ_OLD,
    AODV_CAPT_RREQ_NEW,
    AODV_CAPT_RREQ_METRIC_HIT
} aodv_capt_rreq_result_t;

/**
 * Captures seq_num of the source. Also add prev_hop to precursor list of
 * this destination.
 */
int aodv_db_capt_rreq(mac_addr destination_host,
                      mac_addr originator_host,
                      mac_addr prev_hop,
                      dessert_meshif_t* iface,
                      uint32_t originator_sequence_number,
                      metric_t metric,
                      uint8_t hop_count,
                      struct timeval* timestamp,
                      aodv_capt_rreq_result_t* result_out);

int aodv_db_capt_rrep(mac_addr destination_host,
                      mac_addr destination_host_next_hop,
                      dessert_meshif_t* output_iface,
                      uint32_t destination_sequence_number,
                      metric_t metric,
                      uint8_t hop_count,
                      struct timeval* timestamp);

int aodv_db_getroute2dest(mac_addr dhost_ether, mac_addr dhost_next_hop_out,
                          dessert_meshif_t** output_iface_out, struct timeval* timestamp, uint8_t flags);

int aodv_db_getnexthop(mac_addr dhost_ether, mac_addr dhost_next_hop_out);

int aodv_db_get_destination_sequence_number(mac_addr dhost_ether, uint32_t* destination_sequence_number_out);

int aodv_db_get_hopcount(mac_addr dhost_ether, uint8_t* hop_count_out);
int aodv_db_get_metric(mac_addr dhost_ether, metric_t* last_metric_out);

int aodv_db_markrouteinv(mac_addr dhost_ether, uint32_t destination_sequence_number);
int aodv_db_remove_nexthop(mac_addr next_hop);
int aodv_db_inv_over_nexthop(mac_addr next_hop);
int aodv_db_get_destlist(mac_addr dhost_next_hop, aodv_link_break_element_t** destlist);
int aodv_db_add_precursor(mac_addr destination, mac_addr precursor, dessert_meshif_t *iface);

int aodv_db_get_warn_endpoints_from_neighbor_and_set_warn(mac_addr neighbor, aodv_link_break_element_t** head);
int aodv_db_get_warn_status(mac_addr dhost_ether);

int aodv_db_get_active_routes(aodv_link_break_element_t** head);

int aodv_db_routing_reset(uint32_t* count_out);

/**
 * Take a record that the given neighbor seems to be
 * the 1 hop bidirectional neighbor
 */
int aodv_db_cap2Dneigh(mac_addr ether_neighbor_addr, uint16_t hello_seq, dessert_meshif_t* iface, struct timeval* timestamp);

/**
 * Check whether given neighbor is 1 hop bidirectional neighbor
 */
int aodv_db_check2Dneigh(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp);

int aodv_db_reset_rssi(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp);

int8_t aodv_db_update_rssi(mac_addr ether_neighbor, dessert_meshif_t* iface, struct timeval* timestamp);

int aodv_db_addschedule(struct timeval* execute_ts, mac_addr ether_addr, uint8_t type, void* param);

int aodv_db_popschedule(struct timeval* timestamp, mac_addr ether_addr_out, uint8_t* type, void* param);

int aodv_db_schedule_exists(mac_addr ether_addr, uint8_t type);

int aodv_db_dropschedule(mac_addr ether_addr, uint8_t type);

void aodv_db_putrreq(struct timeval* timestamp);

void aodv_db_getrreqcount(struct timeval* timestamp, uint32_t* count_out);

void aodv_db_putrerr(struct timeval* timestamp);

void aodv_db_getrerrcount(struct timeval* timestamp, uint32_t* count_out);

int aodv_db_capt_data_seq(mac_addr src_addr, uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp);

// ----------------------------------- reporting -------------------------------------------------------------------------

int aodv_db_view_routing_table(char** str_out);
int aodv_db_view_pdr_nt(char** str_out);
void aodv_db_neighbor_timeslot_report(char** str_out);
void aodv_db_packet_buffer_timeslot_report(char** str_out);
void aodv_db_data_seq_timeslot_report(char** str_out);

#endif
