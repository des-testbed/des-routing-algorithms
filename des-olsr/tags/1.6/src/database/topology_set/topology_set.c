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
#include "topology_set.h"
#include "../timeslot.h"
#include "../../config.h"
#include "../../helper.h"

typedef struct olsr_db_tc_tcs {
    uint8_t				tc_orig_addr[ETH_ALEN];
    olsr_db_tc_tcsentry_t*	orig_neighbors;
    uint16_t				seq_num;
    UT_hash_handle			hh;
} olsr_db_tc_tcs_t;

olsr_db_tc_tcs_t*			tc_set = NULL;
timeslot_t*					tc_ts = NULL;

void purge_tcs(struct timeval* curr_time, void* src_object, void* object) {
    olsr_db_tc_tcs_t* tcs = object;

    while(tcs->orig_neighbors != NULL) {
        olsr_db_tc_tcsentry_t* tcs_entry = tcs->orig_neighbors;
        HASH_DEL(tcs->orig_neighbors, tcs_entry);
        free(tcs_entry);
    }

    HASH_DEL(tc_set, tcs);
    free(tcs);
}

olsr_db_tc_tcs_t* create_tcs(uint8_t tc_orig_addr[ETH_ALEN]) {
    olsr_db_tc_tcs_t* entry = malloc(sizeof(olsr_db_tc_tcs_t));

    if(entry == NULL) {
        return NULL;
    }

    memcpy(entry->tc_orig_addr, tc_orig_addr, ETH_ALEN);
    entry->orig_neighbors = NULL;
    entry->seq_num = 0;
    return entry;
}

olsr_db_tc_tcsentry_t* create_tcs_entry(uint8_t tc_neigh_addr[ETH_ALEN], uint8_t link_quality) {
    olsr_db_tc_tcsentry_t* entry = malloc(sizeof(olsr_db_tc_tcsentry_t));

    if(entry == NULL) {
        return NULL;
    }

    entry->link_quality = link_quality;
    memcpy(entry->neighbor_main_addr, tc_neigh_addr, ETH_ALEN);
    return entry;
}

int olsr_db_tc_init() {
    return timeslot_create(&tc_ts, NULL, purge_tcs);
}

int olsr_db_tc_settuple(uint8_t tc_orig_addr[ETH_ALEN], uint8_t orig_neigh_addr[ETH_ALEN], uint8_t link_quality, struct timeval* purge_time) {
    olsr_db_tc_tcs_t* tcs = NULL;
    olsr_db_tc_tcsentry_t* tcs_entry = NULL;
    HASH_FIND(hh, tc_set, tc_orig_addr, ETH_ALEN, tcs);

    if(tcs == NULL) {
        tcs = create_tcs(tc_orig_addr);

        if(tcs == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, tc_set, tcs->tc_orig_addr, ETH_ALEN, tcs);
    }

    timeslot_addobject(tc_ts, purge_time, tcs);
    HASH_FIND(hh, tcs->orig_neighbors, orig_neigh_addr, ETH_ALEN, tcs_entry);

    if(tcs_entry == NULL) {
        tcs_entry = create_tcs_entry(orig_neigh_addr, link_quality);

        if(tcs_entry == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, tcs->orig_neighbors, tcs_entry->neighbor_main_addr, ETH_ALEN, tcs_entry);
    }

    tcs_entry->link_quality = link_quality;
    return true;
}

int olsr_db_tc_removeneighbors(uint8_t tc_orig_addr[ETH_ALEN]) {
    olsr_db_tc_tcs_t* tcs = NULL;
    olsr_db_tc_tcsentry_t* tcs_entry = NULL;
    HASH_FIND(hh, tc_set, tc_orig_addr, ETH_ALEN, tcs);

    if(tcs != NULL) {
        while(tcs->orig_neighbors != NULL) {
            tcs_entry = tcs->orig_neighbors;
            HASH_DEL(tcs->orig_neighbors, tcs_entry);
            free(tcs_entry);
        }
    }

    return true;
}

int olsr_db_tc_removetc(uint8_t tc_orig_addr[ETH_ALEN]) {
    olsr_db_tc_tcs_t* tcs = NULL;
    olsr_db_tc_tcsentry_t* tcs_entry = NULL;
    HASH_FIND(hh, tc_set, tc_orig_addr, ETH_ALEN, tcs);

    if(tcs != NULL) {
        while(tcs->orig_neighbors != NULL) {
            tcs_entry = tcs->orig_neighbors;
            HASH_DEL(tcs->orig_neighbors, tcs_entry);
            free(tcs_entry);
        }

        timeslot_deleteobject(tc_ts, tcs);
        HASH_DEL(tc_set, tcs);
        free(tcs);
    }

    return true;
}

int olsr_db_tc_updateseqnum(uint8_t tc_orig_addr[ETH_ALEN], uint16_t seq_num, struct timeval* purge_time) {
    olsr_db_tc_tcs_t* tcs = NULL;
    HASH_FIND(hh, tc_set, tc_orig_addr, ETH_ALEN, tcs);

    if(tcs == NULL) {
        tcs = create_tcs(tc_orig_addr);

        if(tcs == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, tc_set, tcs->tc_orig_addr, ETH_ALEN, tcs);
        tcs->seq_num = seq_num - 1;
    }

    timeslot_addobject(tc_ts, purge_time, tcs);

    if(hf_seq_comp_i_j(tcs->seq_num, seq_num) >= 0) {
        return false;
    }
    else {
        tcs->seq_num = seq_num;
        return true;
    }
}

olsr_db_tc_tcsentry_t* olsr_db_tc_getneighbors(uint8_t tc_orig_addr[ETH_ALEN]) {
    timeslot_purgeobjects(tc_ts);
    olsr_db_tc_tcs_t* tcs = NULL;
    HASH_FIND(hh, tc_set, tc_orig_addr, ETH_ALEN, tcs);

    if(tcs == NULL) {
        return NULL;
    }

    return tcs->orig_neighbors;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_tc_report(char** str_out) {
    timeslot_purgeobjects(tc_ts);
    int report_str_len = 57;
    olsr_db_tc_tcs_t* current_entry = tc_set;
    char* output;
    char entry_str[report_str_len  + 1];

    size_t str_count = 0;

    while(current_entry != NULL) {
        str_count += HASH_COUNT(current_entry->orig_neighbors) + 1 ;
        current_entry = current_entry->hh.next;
    }

    current_entry = tc_set;

    output = malloc(sizeof(char) * report_str_len * (3 + str_count) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+-------------------+--------------+\n");
    strcat(output, "|   TC orig. addr   | neigh. main addr  | link quality |\n");
    strcat(output, "+-------------------+-------------------+--------------+\n");

    while(current_entry != NULL) {
        int flag = false;
        olsr_db_tc_tcsentry_t* neigbors = current_entry->orig_neighbors;

        while(neigbors != NULL) {
            if(flag == false) {
                snprintf(entry_str, report_str_len + 1, "| " MAC " | " MAC " | %12i |\n",
                         EXPLODE_ARRAY6(current_entry->tc_orig_addr), EXPLODE_ARRAY6(neigbors->neighbor_main_addr), neigbors->link_quality);
                flag = true;
            }
            else {
                snprintf(entry_str, report_str_len + 1, "|                   | " MAC " | %12i |\n",
                         EXPLODE_ARRAY6(neigbors->neighbor_main_addr), neigbors->link_quality);
            }

            strcat(output, entry_str);
            neigbors = neigbors->hh.next;
        }

        current_entry = current_entry->hh.next;
        strcat(output, "+-------------------+-------------------+--------------+\n");
    }

    *str_out = output;
    return true;
}
