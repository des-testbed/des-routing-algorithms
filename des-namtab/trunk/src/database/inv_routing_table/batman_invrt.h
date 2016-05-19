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

#ifndef INVROUTING_TABLE
#define INVROUTING_TABLE

#include <linux/if_ether.h>
#include "../batman_sw.h"
#include "batman_irt_nht.h"
#include <uthash.h>

/** Output interface entry.
 * To one entry of routing table belongs one or more if_entrys.
 */
typedef struct batman_irt_if_entry {
    /** number of source interface over that the OGM was sent */
    uint8_t				iface_num;	// key value
    /** NextNop-Table towards destination */
    batman_irt_nht_t* 		nht;
    UT_hash_handle 			hh;
} batman_irt_if_entry_t;

/** Row entry of B.A.T.M.A.N routing table */
typedef struct batman_irt_entry {
    /** MAC address of source */
    uint8_t 				ether_source_addr[ETH_ALEN];	// key value
    /** pointer to first HASH_MAP iface entry */
    batman_irt_if_entry_t*	if_entrys;
    /** Last aware time of destination */
    time_t 					last_aw_time;
    /** most actual sequence number for destination */
    uint16_t 				curr_seq_num;
    // /* HNA list
    // /* Gateway capabilities
    /** the best output interface for packets towards destination */
    batman_irt_if_entry_t*	best_output_iface;
    UT_hash_handle 			hh;
} batman_irt_entry_t;


int batman_db_irt_init();


/** Add routing entry into routing table */
int batman_db_irt_addroute(uint8_t ether_dest_addr[ETH_ALEN], uint8_t iface_num,
                           time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num);

/** delete route to destination */
int batman_db_irt_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]);

/** Get inverted routing table entrys */
batman_irt_entry_t* batman_db_irt_getinvrt();

/** Get last know seq_num of destination */
int batman_db_irt_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]);

/** Pudge old rows from routing table */
int batman_db_irt_cleanup();

/** Change pusge timeout for routing table */
int batman_db_irt_change_pt(time_t pudge_timeout);

// ------------------- reporting -----------------------------------------------

/** get routing table as string */
int batman_db_irt_report(char** str_out);

#endif
