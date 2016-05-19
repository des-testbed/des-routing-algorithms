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

#include <linux/if_ether.h>
#include <dessert.h>


int batman_db_brt_init();


/** Add routing entry into routing table */
int batman_db_brt_addroute(uint8_t ether_dest_addr[ETH_ALEN], const dessert_meshif_t* ether_iface,
                           time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num);

/** delete route to destination */
int batman_db_brt_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]);

void batman_db_brt_add_myinterfaces_to_precursors(uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
        uint8_t* precursors_iface_count);

int batman_db_brt_check_precursors_list(uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
                                        uint8_t* precursors_iface_count, uint8_t iface_addr[ETH_ALEN]);

int batman_db_brt_getbestroute_arl(uint8_t ether_dest_addr[ETH_ALEN],
                                   const dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN],
                                   uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
                                   uint8_t* presursosr_iface_count);


/** Get last know seq_num of destination */
int batman_db_brt_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]);

/** Pudge old rows from routing table */
int batman_db_brt_cleanup();

/** Change pusge timeout for routing table */
int batman_db_brt_change_pt(time_t pudge_timeout);

// ------------------- reporting -----------------------------------------------

/** get routing table as string */
int batman_db_brt_report(char** str_out);

#endif
