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

#ifndef BATMAN_RT
#define BATMAN_RT

#include <dessert.h>
#include "../../config.h"

int batman_db_rt_init();

/** Add routing entry into routing table */
int batman_db_rt_addroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* ether_iface, time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num);

/** delete route to destination */
int batman_db_rt_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]);

/** Get best route (next hop) towards destination from routing table */
int batman_db_rt_getbestroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN]);

/**
 * Get best route (next hop) towards destination from routing table
 * that avoids routing loops
 */
int batman_db_rt_getbestroute_arl(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN], uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN], uint8_t* presursosr_iface_count);

/** Get last know seq_num of destination */
int batman_db_rt_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]);

/** Pudge old rows from routing table */
int batman_db_rt_cleanup();

/** Change pusge timeout for routing table */
int batman_db_rt_change_pt(time_t pudge_timeout);

/** Change window size in all sliding windows in entire routing table */
void batman_db_rt_change_window_size(uint8_t window_size);

// ------------------- reporting -----------------------------------------------

/** get NextHop-List table for given destination */
int batman_db_rtnht_report(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* ether_iface, char** str_out);

/** get routing table as string */
int batman_db_rt_report(char** str_out);

#endif
