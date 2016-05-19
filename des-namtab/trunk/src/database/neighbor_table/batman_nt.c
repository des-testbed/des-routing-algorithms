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

#include <stdio.h>
#include <time.h>
#include "batman_nt.h"
#include "../timeslot.h"
#include "../../config.h"

typedef struct batman_neighbor_entry {
    uint8_t 		ether_neighbor[ETH_ALEN];
    uint8_t		ether_iface[ETH_ALEN];
    UT_hash_handle	hh;
} __attribute__((__packed__)) batman_neighbor_entry_t;

typedef struct batman_neighbor_table {
    batman_neighbor_entry_t* 	entrys;
    timeslot_t*					ts;
} batman_neighbor_table_t;

batman_neighbor_table_t nt;

int batman_db_neighbor_entry_create(uint8_t ether_neighbor_addr[ETH_ALEN], const uint8_t ether_iface_addr[ETH_ALEN], batman_neighbor_entry_t** entry_out) {
    batman_neighbor_entry_t* new_entry;

    new_entry = malloc(sizeof(batman_neighbor_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    memcpy(new_entry->ether_neighbor, ether_neighbor_addr, ETH_ALEN);
    memcpy(new_entry->ether_iface, ether_iface_addr, ETH_ALEN);
    *entry_out = new_entry;
    return true;
}

int batman_db_neighbor_entry_destroy(batman_neighbor_entry_t* entry) {
    free(entry);
    return true;
}

void on_neigbor_timeout(time_t timestamp, void* object) {
    batman_neighbor_entry_t* curr_entry = object;
    HASH_DEL(nt.entrys, curr_entry);
    batman_db_neighbor_entry_destroy(curr_entry);
}

int batman_db_nt_init() {
    timeslot_t* new_ts;

    if(timeslot_create(&new_ts, NEIGHBOR_TIMEOUT, on_neigbor_timeout) == false) {
        return false;
    }

    nt.entrys = NULL;
    nt.ts = new_ts;
    return true;
}

int batman_db_nt_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* local_iface) {
    batman_neighbor_entry_t* curr_entry;
    uint8_t addr_sum[2*ETH_ALEN];
    memcpy(addr_sum, ether_neighbor_addr, ETH_ALEN);
    memcpy(addr_sum + ETH_ALEN, local_iface->hwaddr, ETH_ALEN);
    HASH_FIND(hh, nt.entrys, addr_sum, 2 * ETH_ALEN, curr_entry);

    if(curr_entry == NULL) {
        if(batman_db_neighbor_entry_create(ether_neighbor_addr, local_iface->hwaddr, &curr_entry) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, nt.entrys, curr_entry->ether_neighbor, 2 * ETH_ALEN, curr_entry);
        dessert_debug("%s <====> " MAC , local_iface->if_name, EXPLODE_ARRAY6(ether_neighbor_addr));
    }

    timeslot_addobject(nt.ts, time(0), curr_entry);
    return true;
}

int batman_db_nt_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* local_iface) {
    timeslot_purgeobjects(nt.ts, time(0));
    batman_neighbor_entry_t* curr_entry;
    uint8_t addr_sum[2*ETH_ALEN];
    memcpy(addr_sum, ether_neighbor_addr, ETH_ALEN);
    memcpy(addr_sum + ETH_ALEN, local_iface->hwaddr, ETH_ALEN);
    HASH_FIND(hh, nt.entrys, addr_sum, 2 * ETH_ALEN, curr_entry);

    if(curr_entry == NULL) {
        return false;
    }

    return true;
}

int batman_db_nt_change_pt(time_t pudge_timeout) {
    timeslot_change_pt(nt.ts, pudge_timeout);
    return true;
}
