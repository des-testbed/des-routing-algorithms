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

#include "batman_rt.h"
#include "../timeslot.h"
#include "../../config.h"
#include <dessert.h>
#include <time.h>
#include <utlist.h>
#include "../inv_routing_table/batman_invrt.h"
#include "../backup_routing_table/batman_brt.h"

typedef struct batman_rt {
    uint8_t				ether_dest_addr[ETH_ALEN]; // key
    const dessert_meshif_t* output_iface;
    uint8_t				ether_next_hop[ETH_ALEN];
    UT_hash_handle			hh;
} batman_rt_t;

batman_rt_t* rt_entrys = NULL;
timeslot_t* rt_ts;

void batman_db_rt_purgeentry(time_t timestamp, void* entry) {
    batman_rt_t* rt_entry = entry;
    HASH_DEL(rt_entrys, rt_entry);
    free(rt_entry);
}

int batman_db_rt_init() {
    return timeslot_create(&rt_ts, PUDGE_TIMEOUT, batman_db_rt_purgeentry);
}

int batman_db_rt_captureroute(uint8_t dest_addr[ETH_ALEN], const dessert_meshif_t* output_iface, uint8_t next_hop[ETH_ALEN]) {
    batman_rt_t* entry;
    HASH_FIND(hh, rt_entrys, dest_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        entry = malloc(sizeof(batman_rt_t));

        if(entry == NULL) {
            return false;
        }

        memcpy(entry->ether_dest_addr, dest_addr, ETH_ALEN);
        HASH_ADD_KEYPTR(hh, rt_entrys, entry->ether_dest_addr, ETH_ALEN, entry);
    }

    entry->output_iface = output_iface;
    memcpy(entry->ether_next_hop, next_hop, ETH_ALEN);
    timeslot_addobject(rt_ts, time(0), entry);
    return true;
}

int batman_db_rt_getroute(uint8_t dest_addr[ETH_ALEN], const dessert_meshif_t** iface_out, uint8_t next_hop_out[ETH_ALEN]) {
    batman_rt_t* entry;
    HASH_FIND(hh, rt_entrys, dest_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        return false;
    }

    *iface_out = entry->output_iface;
    memcpy(next_hop_out, entry->ether_next_hop, ETH_ALEN);
    return true;
}

int batman_db_rt_getroute_arl(uint8_t dest_addr[ETH_ALEN],
                              const dessert_meshif_t** iface_out, uint8_t next_hop_out[ETH_ALEN],
                              uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
                              uint8_t* precursors_iface_count) {
    batman_rt_t* entry;
    HASH_FIND(hh, rt_entrys, dest_addr, ETH_ALEN, entry);

    if(entry != NULL &&
       batman_db_brt_check_precursors_list(precursors_iface_list, precursors_iface_count,
                                           entry->ether_next_hop) == false) {
        *iface_out = entry->output_iface;
        memcpy(next_hop_out, entry->ether_next_hop, ETH_ALEN);
        batman_db_brt_add_myinterfaces_to_precursors(precursors_iface_list, precursors_iface_count);
        return true;
    }

    return batman_db_brt_getbestroute_arl(dest_addr, iface_out, next_hop_out,
                                          precursors_iface_list, precursors_iface_count);
}

int batman_db_rt_cleanup() {
    timeslot_purgeobjects(rt_ts, time(0));
    return true;
}


int batman_db_rt_change_pt(time_t pudge_timeout) {
    timeslot_change_pt(rt_ts, pudge_timeout);
    return true;
}

// ------------------- reporting -----------------------------------------------

int batman_db_rt_report(char** str_out) {
    int REPORT_RT_STR_LEN = 57;
    batman_rt_t* current_entry = rt_entrys;
    char* output;
    char entry_str[REPORT_RT_STR_LEN  + 1];

    output = malloc(sizeof(char) * REPORT_RT_STR_LEN * (4 + HASH_COUNT(rt_entrys)) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+--------------+-------------------+\n");
    strcat(output, "|  destination addr | output_iface |      next hop     |\n");
    strcat(output, "+-------------------+--------------+-------------------+\n");

    while(current_entry != NULL) {
        // first line for best output interface
        snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| " MAC " | %12s | " MAC " |\n",
                 current_entry->ether_dest_addr[0], current_entry->ether_dest_addr[1],
                 current_entry->ether_dest_addr[2], current_entry->ether_dest_addr[3],
                 current_entry->ether_dest_addr[4], current_entry->ether_dest_addr[5],
                 current_entry->output_iface->if_name,
                 current_entry->ether_next_hop[0], current_entry->ether_next_hop[1], current_entry->ether_next_hop[2],
                 current_entry->ether_next_hop[3], current_entry->ether_next_hop[4], current_entry->ether_next_hop[5]);
        strcat(output, entry_str);
        current_entry = current_entry->hh.next;
    }

    strcat(output, "+-------------------+--------------+-------------------+\n");
    *str_out = output;
    return true;
}
