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

#ifndef BATMAN_DATABASE
#define BATMAN_DATABASE

#include <stdlib.h>
#include <linux/if_ether.h>
#include <dessert.h>
#include "../config.h"

/** Make read lock over database to avoid corrupt read/write */
void batman_db_rlock();

/** Make write lock over database to avoid currutp read/write */
void batman_db_wlock();

/** Unlock previos locks for this thread */
void batman_db_unlock();

/** initialize all tables of routing database */
int batman_db_init();

/** cleanup (pudge) old etnrys from all database tables */
int batman_db_cleanup();

/** change pudge timeout for all tables */
int batman_db_change_pt(time_t pudge_timeout);

/** Add routing entry into routing table */
int batman_db_captureroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* local_iface, time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num);

/** Delete route to destination. HINT: need when detected router restart */
int batman_db_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]);

/** Get best route (next hop) towards destination from routing table */
int batman_db_getbestroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** local_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN]);

/**
 * Get best route (next hop) towards destination from routing table
 * that avoids route looping
 */
int batman_db_getbestroute_arl(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN], uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN], uint8_t* presursosr_iface_count);


/** Get last know seq_num of destination */
int batman_db_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]);

/**
 * Take a record that the given neighbor seems to be
 * 1 hop bidirectional neighbor
 */
int batman_db_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* ether_iface);

/**
 * Check whether given neighbour is 1 hop bidirectional neighbor
 */
int batman_db_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* local_iface);

int batman_db_addbrcid(uint8_t source_addr[ETH_ALEN], uint16_t id);

int batman_db_addogmseq(uint8_t source_addr[ETH_ALEN], uint16_t seq);

/** Change window size of all sliding windows in entire routing table */
void batman_db_change_window_size(uint8_t window_size);

// ------------------- reporting -----------------------------------------------

/** get NextHop-List table for given destination */
int batman_db_view_nexthoptable(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* local_iface, char** str_out);

/** get routing table as string */
int batman_db_view_routingtable(char** str_out);

#endif
