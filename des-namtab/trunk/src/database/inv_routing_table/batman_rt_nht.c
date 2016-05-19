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

#include "batman_rt_nht.h"
#include "../../helper.h"
#include "../../config.h"
#include <stdio.h>

#define REPORT_NH_STR_LEN 38

int batman_db_nht_entry_create(batman_rt_nht_entry_t** rt_nh_entry_out, uint8_t ether_addr[ETH_ALEN]) {
    batman_rt_nht_entry_t* new_entry;
    batman_sw_t* new_sw;
    new_entry = malloc(sizeof(batman_rt_nht_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    if(batman_sw_create(&new_sw, WINDOW_SIZE) == false) {
        free(new_entry);
        return false;
    };

    memcpy(new_entry->ether_nexthop_addr, ether_addr, ETH_ALEN);

    new_entry->sw = new_sw;

    *rt_nh_entry_out = new_entry;

    return true;
}

int batman_db_nht_entry_destroy(batman_rt_nht_entry_t* rt_nh_entry) {
    batman_sw_destroy(rt_nh_entry->sw);
    free(rt_nh_entry);
    return true;
}

int batman_db_nht_create(batman_rt_nht_t** rt_nh_out) {
    batman_rt_nht_t* new_nl;
    new_nl = malloc(sizeof(batman_rt_nht_t));

    if(new_nl == NULL) {
        return false;
    }

    new_nl->entrys = NULL;
    new_nl->best_next_hop = NULL;
    *rt_nh_out = new_nl;
    return true;
}

int batman_db_nht_destroy(batman_rt_nht_t* rt_nht) {
    // delete HASH map and all its entrys
    batman_rt_nht_entry_t* current_entry;

    while(rt_nht->entrys != NULL) {
        current_entry = rt_nht->entrys;
        HASH_DEL(rt_nht->entrys, current_entry);
        batman_db_nht_entry_destroy(current_entry);
    }

    // destroy NextHop-List table
    free(rt_nht);
    return true;
}

int batman_db_nht_addseq(batman_rt_nht_t* rt_next_hop_table,
                         uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num) {
    batman_rt_nht_entry_t* nh_entry;

    // Find entry with ether_nexthop_addr
    HASH_FIND(hh, rt_next_hop_table->entrys, ether_nexthop_addr, ETH_ALEN, nh_entry);

    // if no entry -> create!
    if(nh_entry == NULL) {
        if(batman_db_nht_entry_create(&nh_entry, ether_nexthop_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt_next_hop_table->entrys, nh_entry->ether_nexthop_addr,
                        ETH_ALEN, nh_entry);
    }

    // add sequence number to entry sliding window
    batman_sw_addsn(nh_entry->sw, seq_num);

    // drop all out-of-range seq_num from other entry
    // AND
    // set the BEST NEXT HOP to entry with most count of seq numbers
    batman_db_nht_shiftuptoseq(rt_next_hop_table, seq_num);
    return true;
}

int batman_db_nht_shiftuptoseq(batman_rt_nht_t* rt_next_hop_table, uint16_t seq_num) {
    batman_rt_nht_entry_t* nh_entry = rt_next_hop_table->entrys;
    rt_next_hop_table->best_next_hop = nh_entry; // reset best next hop

    while(nh_entry != NULL) {
        batman_sw_dropsn(nh_entry->sw, seq_num); // shift window by all entrys to seq_num

        if(rt_next_hop_table->best_next_hop->sw->size + WINDOW_SWITCH_DIFF <= nh_entry->sw->size) {
            rt_next_hop_table->best_next_hop = nh_entry;
        }

        batman_rt_nht_entry_t* prev_element = nh_entry;
        nh_entry = nh_entry->hh.next;

        if(prev_element->sw->size == 0) {
            // previous element is empty -> delete
            HASH_DEL(rt_next_hop_table->entrys, prev_element);

            if(rt_next_hop_table->best_next_hop == prev_element) {
                rt_next_hop_table->best_next_hop = rt_next_hop_table->entrys;
            }

            batman_db_nht_entry_destroy(prev_element);
        }
    }

    return true;
}

int batman_db_nht_get_totalsncount(batman_rt_nht_t* rt_next_hop_table) {
    int count = 0;
    batman_rt_nht_entry_t* curr_entry = rt_next_hop_table->entrys;

    while(curr_entry != NULL) {
        count += curr_entry->sw->size;
        curr_entry = curr_entry->hh.next;
    }

    return count;
}

// ------------------- reporting -----------------------------------------------

int batman_db_nht_report(batman_rt_nht_t* nh_table, char** str_out) {
    batman_rt_nht_entry_t* current_entry = nh_table->entrys;
    char* output;
    char entry_str[REPORT_NH_STR_LEN  + 1];

    output = malloc(sizeof(char) * REPORT_NH_STR_LEN * (4 + HASH_COUNT(current_entry)) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+---------------+\n");
    strcat(output, "| next hop address  | seq_num count |\n");
    strcat(output, "+-------------------+---------------+\n");

    while(current_entry != NULL) {
        if(nh_table->best_next_hop == current_entry) {
            snprintf(entry_str, REPORT_NH_STR_LEN + 1, "| " MAC " | * %12i|\n",
                     current_entry->ether_nexthop_addr[0], current_entry->ether_nexthop_addr[1],
                     current_entry->ether_nexthop_addr[2], current_entry->ether_nexthop_addr[3],
                     current_entry->ether_nexthop_addr[4], current_entry->ether_nexthop_addr[5],
                     current_entry->sw->size);
        }
        else {
            snprintf(entry_str, REPORT_NH_STR_LEN + 1, "| " MAC " |   %12i|\n",
                     current_entry->ether_nexthop_addr[0], current_entry->ether_nexthop_addr[1],
                     current_entry->ether_nexthop_addr[2], current_entry->ether_nexthop_addr[3],
                     current_entry->ether_nexthop_addr[4], current_entry->ether_nexthop_addr[5],
                     current_entry->sw->size);
        }

        strcat(output, entry_str);
        current_entry = current_entry->hh.next;
    }

    strcat(output, "+-------------------+---------------+\n");
    *str_out = output;
    return true;
}
