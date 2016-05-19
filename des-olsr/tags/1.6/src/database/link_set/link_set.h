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

#ifndef OLSR_LINK_SET
#define OLSR_LINK_SET
#include <uthash.h>
#include <dessert.h>
#include "../../android.h"
#include "sliding_window.h"
#include "../timeslot.h"


typedef struct olsr_db_linkset_nl_entry {
    uint8_t 			neighbor_iface_addr[ETH_ALEN];		// key
    uint8_t			neighbor_main_addr[ETH_ALEN];
    struct timeval		SYM_time;
    struct timeval		ASYM_time;
    olsr_sw_t*			sw;
    /** link quality towards neighbor */
    uint8_t			quality_to_neighbor;
    UT_hash_handle		hh;
} olsr_db_linkset_nl_entry_t;

typedef struct olsr_db_linkset_ltuple {
    const dessert_meshif_t* 	local_iface;			// key
    olsr_db_linkset_nl_entry_t*	neighbor_iface_list;
    timeslot_t*					ts;
    UT_hash_handle				hh;
} olsr_db_linkset_ltuple_t;

/**
 * @required: write lock
 */
uint8_t olsr_db_ls_getlinkquality_from_neighbor(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN]);

/**
 * @required: write lock
 */
uint8_t olsr_db_ls_get_linkmetrik_quality(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN]);

/**
 * @required: write lock
 */
int olsr_db_ls_updatelinkquality(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN],
                                 uint16_t hello_seq_num);

/**
 * @required: write lock
 */
int olsr_db_ls_gettuple(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN],
                        struct timeval* SYM_time_out, struct timeval* ASYM_time_out);

/**
 * @required: write lock
 */
olsr_db_linkset_nl_entry_t* olsr_db_ls_getlinkset(const dessert_meshif_t* local_iface);

/**
 * @required: read lock
 */
int olsr_db_ls_getmainaddr(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN],
                           uint8_t neighbor_main_addr_out[ETH_ALEN]);

/**
 * @required: write lock
 */
olsr_db_linkset_ltuple_t* olsr_db_ls_getif(const dessert_meshif_t* local_iface);

/**
 * @required: write lock
 */
olsr_db_linkset_nl_entry_t* olsr_db_ls_getneigh(olsr_db_linkset_ltuple_t* lf_tuple,
        uint8_t neighbor_iface_addr[ETH_ALEN], uint8_t neighbor_main_addr[ETH_ALEN]);

int olsr_db_ls_report(char** str_out);

#endif
