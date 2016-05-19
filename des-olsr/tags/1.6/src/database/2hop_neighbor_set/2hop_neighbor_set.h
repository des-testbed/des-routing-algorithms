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

#ifndef OLSR_2HOP_NEIGHBOR_SET
#define OLSR_2HOP_NEIGHBOR_SET

#include <stdlib.h>
#include <uthash.h>
#include <linux/if_ether.h>

typedef struct olsr_2hns_neighbor {
    uint8_t		ether_addr[ETH_ALEN];
    uint8_t		link_quality;
    UT_hash_handle	hh;
} olsr_2hns_neighbor_t;

int olsr_db_2hns_add2hneighbor(uint8_t _1hop_neighbor_addr[ETH_ALEN],
                               uint8_t _2hop_neighbor_addr[ETH_ALEN], uint8_t link_quality, struct timeval* purge_time);

olsr_2hns_neighbor_t* olsr_db_2hns_get2hneighbors(uint8_t _1hop_neighbor_addr[ETH_ALEN]);

olsr_2hns_neighbor_t* olsr_db_2hns_get1hneighbors(uint8_t _2hop_neighbor_addr[ETH_ALEN]);

olsr_2hns_neighbor_t* olsr_db_2hns_get1hnset();

olsr_2hns_neighbor_t* olsr_db_2hns_get2hnset();


void olsr_db_2hns_del1hneighbor(uint8_t _1hop_neighbor_addr[ETH_ALEN]);

void olsr_db_2hns_del2hneighbor(uint8_t _2hop_neighbor_addr[ETH_ALEN]);

void olsr_db_2hns_clear1hn(uint8_t _1hop_neighbor_addr[ETH_ALEN]);

uint8_t olsr_db_2hns_getlinkquality(uint8_t _1hop_neghbor_addr[ETH_ALEN],
                                    uint8_t _2hop_neghbor_addr[ETH_ALEN]);

int olsr_db_2hns_report(char** str_out);

#endif
