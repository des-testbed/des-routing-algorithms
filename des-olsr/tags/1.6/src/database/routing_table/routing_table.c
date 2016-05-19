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

#include <uthash.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "../../config.h"
#include "routing_table.h"

typedef struct olsr_db_rt {
    uint8_t 		dest_addr[ETH_ALEN]; // key
    uint8_t		next_hop[ETH_ALEN];
    uint8_t		precursor_addr[ETH_ALEN];
    uint8_t		hop_count;
    float			link_quality;
    UT_hash_handle	hh;
} olsr_db_rt_t;

olsr_db_rt_t*		rt_set = NULL;

void olsr_db_rt_destroy() {
    while(rt_set != NULL) {
        olsr_db_rt_t* entry = rt_set;
        HASH_DEL(rt_set, entry);
        free(entry);
    }
}

olsr_db_rt_t* create_rtentry(uint8_t dest_addr[ETH_ALEN], uint8_t next_hop[ETH_ALEN], uint8_t precursor_addr[ETH_ALEN], uint8_t hop_count, float link_quality) {
    olsr_db_rt_t* entry = malloc(sizeof(olsr_db_rt_t));

    if(entry == NULL) {
        return NULL;
    }

    memcpy(entry->dest_addr, dest_addr, ETH_ALEN);
    memcpy(entry->next_hop, next_hop, ETH_ALEN);
    memcpy(entry->precursor_addr, precursor_addr, ETH_ALEN);
    entry->hop_count = hop_count;
    entry->link_quality = link_quality;
    return entry;
}

int olsr_db_rt_addroute(uint8_t dest_addr[ETH_ALEN], uint8_t next_hop[ETH_ALEN], uint8_t precursor_addr[ETH_ALEN], uint8_t hop_count, float link_quality) {
    olsr_db_rt_t* entry = NULL;
    HASH_FIND(hh, rt_set, dest_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        entry = create_rtentry(dest_addr, next_hop, precursor_addr, hop_count, link_quality);

        if(entry == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt_set, entry->dest_addr, ETH_ALEN, entry);
    }
    else {
        memcpy(entry->next_hop, next_hop, ETH_ALEN);
        memcpy(entry->precursor_addr, precursor_addr, ETH_ALEN);
        entry->hop_count = hop_count;
        entry->link_quality = link_quality;
    }

    return true;
}

int olsr_db_rt_getnexthop(uint8_t dest_addr[ETH_ALEN], uint8_t next_hop_out[ETH_ALEN]) {
    olsr_db_rt_t* entry = NULL;
    HASH_FIND(hh, rt_set, dest_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        return false;
    }

    memcpy(next_hop_out, entry->next_hop, ETH_ALEN);
    return true;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_rt_report(char** str_out) {
    int report_str_len = 150;
    olsr_db_rt_t* current_entry = rt_set;
    char* output;
    char entry_str[report_str_len  + 1];

    output = malloc(sizeof(char) * report_str_len * (4 + HASH_COUNT(rt_set)) + 1);

    if(output == NULL) {
        return false;
    }

    struct timeval curr_time;

    gettimeofday(&curr_time, NULL);

    // initialize first byte to \0 to mark output as empty
    *output = '\0';

    strcat(output, "+-------------------+-------------------+-------------------+-----------+--------------+\n");

    if(rc_metric != RC_METRIC_ETX_ADD) {
        strcat(output, "|    destination    |     next hop      |     precursor     | hop count | link quality |\n");
    }
    else {
        strcat(output, "|    destination    |     next hop      |     precursor     | hop count |   ETX-sum    |\n");
    }

    strcat(output, "+-------------------+-------------------+-------------------+-----------+--------------+\n");

    while(current_entry != NULL) {
        snprintf(entry_str, report_str_len + 1, "| " MAC " | " MAC " | "MAC" | %9i | %12.2f |\n",
                 EXPLODE_ARRAY6(current_entry->dest_addr), EXPLODE_ARRAY6(current_entry->next_hop), EXPLODE_ARRAY6(current_entry->precursor_addr), current_entry->hop_count, current_entry->link_quality);
        strcat(output, entry_str);
        current_entry = current_entry->hh.next;
    }

    strcat(output, "+-------------------+-------------------+-------------------+-----------+--------------+\n");
    *str_out = output;
    return true;
}

int olsr_db_rt_report_so(char** str_out) {
    int report_str_len = 70;
    olsr_db_rt_t* current_entry = rt_set;
    char* output;
    char entry_str[report_str_len  + 1];

    output = malloc(sizeof(char) * report_str_len * (HASH_COUNT(rt_set)) + 1);

    if(output == NULL) {
        return false;
    }

    struct timeval curr_time;

    gettimeofday(&curr_time, NULL);

    // initialize first byte to \0 to mark output as empty
    *output = '\0';

    while(current_entry != NULL) {
        snprintf(entry_str, report_str_len + 1, MAC "\t" MAC "\t" MAC "\t%i\t%5.2f\n",
                 EXPLODE_ARRAY6(current_entry->dest_addr), EXPLODE_ARRAY6(current_entry->next_hop), EXPLODE_ARRAY6(current_entry->precursor_addr), current_entry->hop_count, current_entry->link_quality);
        strcat(output, entry_str);
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return true;
}
