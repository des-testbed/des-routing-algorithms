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

#ifndef OLSR_NEIGHBOR_SET
#define OLSR_NEIGHBOR_SET

#include <dessert.h>
#include <uthash.h>
#include "../../android.h"


typedef struct olsr_db_tuple {
    uint8_t neighbor_main_addr[ETH_ALEN]; // key
    uint8_t mpr;
    uint8_t mpr_selector;
    uint8_t willingness;
    struct {
        dessert_meshif_t* local_iface;
        uint8_t neighbor_iface_addr[ETH_ALEN];
        // ETX or PLR
        uint8_t quality; // (1 - PLR) * 100 % OR (1 / ETX) * 100 %
    } best_link;
    UT_hash_handle hh;
} olsr_db_ns_tuple_t;

int olsr_db_ns_init();

/**
 * get or create neighbor
 */
olsr_db_ns_tuple_t* olsr_db_ns_gcneigh(uint8_t neighbor_main_addr[ETH_ALEN]);

void olsr_db_ns_updatetimeslot(olsr_db_ns_tuple_t* tuple, struct timeval* purgetime);

int olsr_db_ns_getneigh(uint8_t neighbor_main_addr[ETH_ALEN], uint8_t* mpr_out, uint8_t* mpr_selector_out, uint8_t* willingness_out);

int olsr_db_ns_isneigh(uint8_t neighbor_main_addr[ETH_ALEN]);

int olsr_db_ns_setneigh_mprstatus(uint8_t neighbor_main_addr[ETH_ALEN], uint8_t is_mpr);

int olsr_db_ns_ismprselector(uint8_t neighbor_main_addr[ETH_ALEN]);

olsr_db_ns_tuple_t* olsr_db_ns_getneighset();

void olsr_db_ns_removeallmprs();

uint8_t olsr_db_ns_getlinkquality(uint8_t neighbor_main_addr[ETH_ALEN]);

int olsr_db_ns_getbestlink(uint8_t neighbor_main_addr[ETH_ALEN], dessert_meshif_t** output_iface_out, uint8_t neighbor_iface[ETH_ALEN]);

int olsr_db_ns_report(char** str_out);

int olsr_db_ns_report_so(char** str_out);

#endif
