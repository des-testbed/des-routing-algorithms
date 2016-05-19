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

#include <stdlib.h>
#include <stdio.h>
#include "../../config.h"
#include "link_set.h"
#include "../../helper.h"

olsr_db_linkset_ltuple_t*				link_set = NULL;

void purge_ltuple(struct timeval* timestamp, void* src_object, void* object) {
    olsr_db_linkset_ltuple_t* lf_entry = src_object;
    olsr_db_linkset_nl_entry_t* nl_entry = object;
    HASH_DEL(lf_entry->neighbor_iface_list, nl_entry);
    olsr_sw_destroy(nl_entry->sw);
    free(nl_entry);
}

int ltuple_create(olsr_db_linkset_ltuple_t** ltuple_out, const dessert_meshif_t* local_iface) {
    olsr_db_linkset_ltuple_t* tuple = malloc(sizeof(olsr_db_linkset_ltuple_t));

    if(tuple == NULL) {
        return false;
    }

    tuple->local_iface = local_iface;

    if(timeslot_create(&tuple->ts, tuple, purge_ltuple) == false) {
        free(tuple);
        return false;
    }

    tuple->neighbor_iface_list = NULL;
    *ltuple_out = tuple;
    return true;
}

int nltuple_create(olsr_db_linkset_nl_entry_t** nl_tuple_out, uint8_t* neighbor_iface_addr) {
    olsr_db_linkset_nl_entry_t* tuple = malloc(sizeof(olsr_db_linkset_nl_entry_t));

    if(tuple == NULL) {
        return false;
    }

    memcpy(tuple->neighbor_iface_addr, neighbor_iface_addr, ETH_ALEN);
    olsr_sw_create(&tuple->sw, window_size);
    tuple->quality_to_neighbor = 0;
    *nl_tuple_out = tuple;
    return true;
}

olsr_db_linkset_ltuple_t* olsr_db_ls_getif(const dessert_meshif_t* local_iface) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(int), lf_tuple);

    if(lf_tuple == NULL) {
        if(ltuple_create(&lf_tuple, local_iface) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, link_set, &lf_tuple->local_iface, sizeof(int), lf_tuple);
    }

    return lf_tuple;
}

olsr_db_linkset_nl_entry_t* olsr_db_ls_getneigh(olsr_db_linkset_ltuple_t* lf_tuple, uint8_t neighbor_iface_addr[ETH_ALEN], uint8_t neighbor_main_addr[ETH_ALEN]) {
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        if(nltuple_create(&nl_tuple, neighbor_iface_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, lf_tuple->neighbor_iface_list, nl_tuple->neighbor_iface_addr, ETH_ALEN, nl_tuple);
        memcpy(nl_tuple->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
    }

    return nl_tuple;
}



uint8_t olsr_db_ls_getlinkquality_from_neighbor(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN]) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(int), lf_tuple);

    if(lf_tuple == NULL) {
        return 0;
    }

    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        return 0;
    }

    return olsr_sw_getquality(nl_tuple->sw);
}

uint8_t olsr_db_ls_get_linkmetrik_quality(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN]) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(int), lf_tuple);

    if(lf_tuple == NULL) {
        return 0;
    }

    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        return 0;
    }

    uint8_t quality;

    if(rc_metric == RC_METRIC_ETX) {
        uint8_t quality_from_neighbor = olsr_sw_getquality(nl_tuple->sw);
        uint8_t quality_to_neighbor = nl_tuple->quality_to_neighbor;
        // (1 / ETX) * 100 %
        quality = (quality_from_neighbor * quality_to_neighbor) / 100;
    }
    else {
        quality = nl_tuple->quality_to_neighbor;
    }

    return quality;
}

int olsr_db_ls_updatelinkquality(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN], uint16_t hello_seq_num) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(int), lf_tuple);

    if(lf_tuple == NULL) {
        return false;
    }

    timeslot_purgeobjects(lf_tuple->ts);

    // search one more time since entry can be deleted
    HASH_FIND(hh, link_set, &local_iface, sizeof(int), lf_tuple);

    if(lf_tuple == NULL) {
        return false;
    }

    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        return false;
    }

    olsr_sw_addsn(nl_tuple->sw, hello_seq_num);
    return true;
}

int olsr_db_ls_gettuple(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN], struct timeval* SYM_time_out, struct timeval* ASYM_time_out) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(void*), lf_tuple);

    if(lf_tuple == NULL) {
        return false;
    }

    timeslot_purgeobjects(lf_tuple->ts);

    // search one more time since entry can be deleted
    HASH_FIND(hh, link_set, &local_iface, sizeof(void*), lf_tuple);

    if(lf_tuple == NULL) {
        return false;
    }

    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        return false;
    }

    SYM_time_out->tv_sec = nl_tuple->SYM_time.tv_sec;
    SYM_time_out->tv_usec = nl_tuple->SYM_time.tv_usec;
    ASYM_time_out->tv_sec = nl_tuple->ASYM_time.tv_sec;
    ASYM_time_out->tv_usec = nl_tuple->ASYM_time.tv_usec;

    return true;
}

olsr_db_linkset_nl_entry_t* olsr_db_ls_getlinkset(const dessert_meshif_t* local_iface) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(void*), lf_tuple);

    if(lf_tuple == NULL) {
        return NULL;
    }

    timeslot_purgeobjects(lf_tuple->ts);

    // search one more time since entry can be deleted
    HASH_FIND(hh, link_set, &local_iface, sizeof(void*), lf_tuple);

    if(lf_tuple == NULL) {
        return NULL;
    }

    return lf_tuple->neighbor_iface_list;
}

int olsr_db_ls_getmainaddr(const dessert_meshif_t* local_iface, uint8_t neighbor_iface_addr[ETH_ALEN], uint8_t neighbor_main_addr_out[ETH_ALEN]) {
    olsr_db_linkset_ltuple_t* lf_tuple;
    olsr_db_linkset_nl_entry_t* nl_tuple;
    HASH_FIND(hh, link_set, &local_iface, sizeof(void*), lf_tuple);

    if(lf_tuple == NULL) {
        return false;
    }

    HASH_FIND(hh, lf_tuple->neighbor_iface_list, neighbor_iface_addr, ETH_ALEN, nl_tuple);

    if(nl_tuple == NULL) {
        return false;
    }

    memcpy(neighbor_main_addr_out, nl_tuple->neighbor_main_addr, ETH_ALEN);
    return true;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_ls_report(char** str_out) {
    int report_str_len = 54;
    olsr_db_linkset_ltuple_t* current_entry = link_set;
    char* output;
    char entry_str[report_str_len  + 1];

    size_t str_count = 0;

    while(current_entry != NULL) {
        str_count += HASH_COUNT(current_entry->neighbor_iface_list);
        current_entry = current_entry->hh.next;
    }

    current_entry = link_set;

    output = malloc(sizeof(char) * report_str_len * (4 + str_count) + 1);

    if(output == NULL) {
        return false;
    }

    struct timeval curr_time;

    gettimeofday(&curr_time, NULL);

    // initialize first byte to \0 to mark output as empty
    *output = '\0';

    strcat(output, "+---------------+-------------------+-------+-------+\n");

    strcat(output, "|  local iface  | neigh. iface addr |  SYM  | ASYM  |\n");

    strcat(output, "+---------------+-------------------+-------+-------+\n");

    while(current_entry != NULL) {
        olsr_db_linkset_nl_entry_t* neigbors = current_entry->neighbor_iface_list;

        while(neigbors != NULL) {
            snprintf(entry_str, report_str_len + 1, "| %13s | " MAC " | %5s | %5s |\n",
                     current_entry->local_iface->if_name,
                     EXPLODE_ARRAY6(neigbors->neighbor_iface_addr),
                     (hf_compare_tv(&neigbors->SYM_time, &curr_time) >= 0) ? "true" : "false",
                     (hf_compare_tv(&neigbors->ASYM_time, &curr_time) >= 0) ? "true" : "false");
            strcat(output, entry_str);
            neigbors = neigbors->hh.next;
        }

        current_entry = current_entry->hh.next;
    }

    strcat(output, "+---------------+-------------------+-------+-------+\n");
    *str_out = output;
    return true;
}
