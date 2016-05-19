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

#ifndef BATMAN_RT_NHT
#define BATMAN_RT_NHT

#include <linux/if_ether.h>
#include "../batman_sw.h"
#include <uthash.h>

/** Entry of NextHop-List table */
typedef struct batman_brt_nht_entry {
    /** MAC address of next hop towards destination as string */
    uint8_t ether_nexthop_addr[ETH_ALEN]; // key value
    /**
     * Sliding Window of this connection towards destination.
     * Indicates quality of connection.
     */
    batman_sw_t* sw;
    UT_hash_handle hh;
} batman_rt_bnht_entry_t;

/** NextHop table */
typedef struct batman_brt_nht {
    batman_rt_bnht_entry_t* entrys;
    /** the best hop entry with most SeqNum count towards destination */
    batman_rt_bnht_entry_t* best_next_hop;
} batman_brt_nht_t;

// ------------------- NextHop-Table functions ------------------------------------------

/** Create a NextHop Table, a sub-table of Routing Table */
int batman_db_bnht_create(batman_brt_nht_t** rt_nh_out);

/** Destroy NextHop Table */
int batman_db_bnht_destroy(batman_brt_nht_t* rt_nh);

/** Adds a sequence number to one NextHop of NextHop Table */
int batman_db_bnht_addseq(batman_brt_nht_t* rt_next_hop_table,
                          uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num);

/** Shift all of NHT netrys to given seq_num. Pudge entrys with sw = 0 */
int batman_db_bnht_shiftuptoseq(batman_brt_nht_t* rt_next_hop_table, uint16_t seq_num);

/** Get total count of seq_nums in Next Hop Table */
int batman_db_bnht_get_totalsncount(batman_brt_nht_t* rt_next_hop_table);

// ------------------- reporting --------------------------------------------------------

#endif
