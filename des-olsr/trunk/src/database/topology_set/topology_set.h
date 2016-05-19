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

#ifndef TOPOLOGY_SET
#define TOPOLOGY_SET

#include <stdlib.h>
#include <uthash.h>
#include <linux/if_ether.h>
#include "../../android.h"

typedef struct olsr_db_tc_tcsentry {
    uint8_t link_quality;
    uint8_t neighbor_main_addr[ETH_ALEN];
    UT_hash_handle hh;
} olsr_db_tc_tcsentry_t;

int olsr_db_tc_init();

int olsr_db_tc_settuple(uint8_t tc_orig_addr[ETH_ALEN], uint8_t orig_neigh_addr[ETH_ALEN], uint8_t link_quality, struct timeval* purge_time);

int olsr_db_tc_removeneighbors(uint8_t tc_orig_addr[ETH_ALEN]);

int olsr_db_tc_removetc(uint8_t tc_orig_addr[ETH_ALEN]);

int olsr_db_tc_updateseqnum(uint8_t tc_orig_addr[ETH_ALEN], uint16_t seq_num, struct timeval* purge_time);

olsr_db_tc_tcsentry_t* olsr_db_tc_getneighbors(uint8_t tc_orig_addr[ETH_ALEN]);

int olsr_db_tc_report(char** str_out);

#endif
