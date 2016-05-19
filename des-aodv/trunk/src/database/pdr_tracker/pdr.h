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

#ifndef AODV_PDR
#define AODV_PDR

#include <dessert.h>
#include <utlist.h>
#include <uthash.h>
#include "../timeslot.h"
#include "../../helper.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

typedef struct pdr_neighbor_hello_msg {
    uint16_t			seq_num; //KEY
    UT_hash_handle		hh;
} pdr_neighbor_hello_msg_t;

typedef struct pdr_neighbor_entry {
    uint8_t						ether_neighbor[ETH_ALEN]; //KEY
    uint16_t					hello_interv;
    uint16_t					expected_hellos;
    uint8_t						rcvd_hello_count;
    uint8_t						nb_rcvd_hello_count;
    timeslot_t*					ts;
    pdr_neighbor_hello_msg_t* 	msg_list;
    struct timeval				purge_tv;
    UT_hash_handle		hh;
} pdr_neighbor_entry_t;

typedef struct pdr_neighbor_table {
    pdr_neighbor_entry_t*   entries;
    uint16_t				nb_expected_hellos;
    timeslot_t*				ts;
} pdr_neighbor_table_t;

pdr_neighbor_table_t pdr_nt;

/**Initialize PDR Tracker Structure*/
int aodv_db_pdr_nt_init();

/**Updates a neighbor entry, if neighbor changed hello interval*/
void pdr_neighbor_entry_update(pdr_neighbor_entry_t* update_entry, uint16_t new_interval);

/**Update nb_expected_hellos due to switch of own hello interval*/
int aodv_db_pdr_nt_upd_expected(uint16_t new_interval);

/**Purge Structure that is invoked every time an object is deleted from msg timeslot*/
void pdr_nt_purge_hello_msg(struct timeval* timestamp, void* src_object, void* object);

/**Purge Structure that is invoked every time an object is deleted from nb timeslot*/
void pdr_nt_purge_nb(struct timeval* timestamp, void* src_object, void* del_object);

/**Destroys a neighbor entry with all tracked hello msgs*/
int pdr_nt_neighbor_destroy(uint32_t* count_out);

/**Destroys all tracked hello msgs for the given neighbor entry*/
int pdr_nt_msg_destroy(pdr_neighbor_entry_t* curr_nb);

/**Resets the whole PDR Tracker structure*/
int aodv_db_pdr_nt_neighbor_reset(uint32_t* count_out);


/**Captures a hello req from neighbor*/
int aodv_db_pdr_nt_cap_hello(mac_addr ether_neighbor_addr, uint16_t hello_seq, uint16_t hello_interval, struct timeval* timestamp);

/**Captures a hello resp from neighbor*/
int aodv_db_pdr_nt_cap_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_interval, uint8_t hello_count, struct timeval* timestamp);

/**Purges all msg objects with expired lifetime*/
int pdr_nt_cleanup(pdr_neighbor_entry_t* given_entry, struct timeval* timestamp);

/**Cleanup function used for periodic cleanup*/
int aodv_db_pdr_nt_cleanup(struct timeval* timestamp);

/**Returns the pdr for the link encoded as uint16_t*/
int aodv_db_pdr_nt_get_pdr(mac_addr ether_neighbor_addr, uint16_t* pdr_out, struct timeval* timestamp);

/**Returns the etx value for the link encoded as uint16_t*/
int aodv_db_pdr_nt_get_etx_mul(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp);

/**Returns the etx value for the link encoded as uint16_t*/
int aodv_db_pdr_nt_get_etx_add(mac_addr ether_neighbor_addr, uint16_t* etx_out, struct timeval* timestamp);

/**Returns the number of rcvd hellos from the given adress*/
int aodv_db_pdr_nt_get_rcvdhellocount(mac_addr ether_neighbor_addr, uint8_t* count_out, struct timeval* timestamp);

/**Creates a visual representation of the pdr neighbor table*/
int aodv_db_pdr_nt_report(char** str_out);

#endif
