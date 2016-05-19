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

#include <linux/if_ether.h>
#include <stdlib.h>
#include <stdio.h>
#include "neighbor_set_ett.h"
#include <limits.h>

olsr_db_neighbors_ett_entry_t*		neighbor_set_ett = NULL;
int i;

/*
 * Calculates the difference between two timevals.
 */
void timeval_subtract(struct timeval* result, struct timeval* x, struct timeval* y) {
    /* Perform the carry for the later subtraction by updating y. */
    if(x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }

    if(x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
    tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    if(result->tv_sec < 0 && result->tv_usec > 0) {
        result->tv_usec = 1000000 - result->tv_usec;
        result->tv_sec = result->tv_sec * (-1) - 1;
    }
}

uint32_t get_min_time_from_neighbor(uint8_t neighbor_main_addr[ETH_ALEN]) {
    if(neighbor_set_ett == NULL) {
        return false;
    }

    // Searching for entry with neighbor_main_addr as key
    olsr_db_neighbors_ett_entry_t* entry;
    HASH_FIND(hh, neighbor_set_ett, neighbor_main_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        return false;
    }

    // searching for the minimum time_int_sw entry
    uint32_t min = INT_MAX;
    int i;

    for(i = 0; i < ETT_SW_SIZE; i++) {
        if(entry->time_int_sw[i] > 0) {
            if(entry->time_int_sw[i] < min) {
                min = entry->time_int_sw[i];
            }
        }
    }

    if(min == INT_MAX) {
        return false;
    }

    return min;
}

int process_ett_start_time(uint8_t neighbor_main_addr[ETH_ALEN], struct timeval* ett_start_time) {
    olsr_db_neighbors_ett_entry_t* entry;

    //if the set is empty a new entry is created
    if(neighbor_set_ett == NULL) {
        entry = malloc(sizeof(olsr_db_neighbors_ett_entry_t));
        memcpy(entry->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
        entry->timeval_recv.tv_sec = 0;
        entry->timeval_recv.tv_usec = 0;

        for(i = 0; i < ETT_SW_SIZE; i++) {
            entry->time_int_sw[i] = 0;
        }

        entry->time_sw_ptr = 0;
        HASH_ADD_KEYPTR(hh, neighbor_set_ett, entry->neighbor_main_addr, ETH_ALEN, entry);
    }
    else {
        HASH_FIND(hh, neighbor_set_ett, neighbor_main_addr, ETH_ALEN, entry);

        //if the entry did not exist a new entry is created
        if(entry == NULL) {
            entry = malloc(sizeof(olsr_db_neighbors_ett_entry_t));
            memcpy(entry->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
            entry->timeval_recv.tv_sec = 0;
            entry->timeval_recv.tv_usec = 0;

            for(i = 0; i < ETT_SW_SIZE; i++) {
                entry->time_int_sw[i] = 0;
            }

            entry->time_sw_ptr = 0;
            HASH_ADD_KEYPTR(hh, neighbor_set_ett, entry->neighbor_main_addr, ETH_ALEN, entry);
        }
    }

    // saves ett_start_time
    entry->timeval_recv = (*ett_start_time);
    return true;
}

uint32_t process_ett_stop_time(uint8_t neighbor_main_addr[ETH_ALEN], struct timeval* ett_stop_time) {
    olsr_db_neighbors_ett_entry_t* entry;

    if(neighbor_set_ett == NULL) {
        return false;
    }

    HASH_FIND(hh, neighbor_set_ett, neighbor_main_addr, ETH_ALEN, entry);

    if(entry == NULL) {
        return false;
    }

    // If no previous ETT-START package was received false is returned
    if(entry->timeval_recv.tv_sec == 0 && entry->timeval_recv.tv_usec == 0) {
        return false;
    }

    // calculates the difference between ett_stop_time and timeval_recv
    struct timeval res;
    timeval_subtract(&res, &(entry->timeval_recv), ett_stop_time);
    // calculates the difference time in usec
    uint32_t result = res.tv_sec * 1000000 + res.tv_usec;
    // deleting the old timeval_recv value
    entry->timeval_recv.tv_sec = 0;
    entry->timeval_recv.tv_usec = 0;
    return result;
}

int process_ett_msg(uint8_t neighbor_main_addr[ETH_ALEN], uint32_t recved_time) {
    if(recved_time < 100) {
        return false;
    }

    olsr_db_neighbors_ett_entry_t* entry;

    //if the set is empty a new entry is created
    if(neighbor_set_ett == NULL) {
        entry = malloc(sizeof(olsr_db_neighbors_ett_entry_t));
        memcpy(entry->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
        entry->timeval_recv.tv_sec = 0;
        entry->timeval_recv.tv_usec = 0;

        for(i = 0; i < ETT_SW_SIZE; i++) {
            entry->time_int_sw[i] = 0;
        }

        entry->time_sw_ptr = 0;
        HASH_ADD_KEYPTR(hh, neighbor_set_ett, entry->neighbor_main_addr, ETH_ALEN, entry);
    }
    else {
        HASH_FIND(hh, neighbor_set_ett, neighbor_main_addr, ETH_ALEN, entry);

        //if the entry did not exist a new entry is created
        if(entry == NULL) {
            entry = malloc(sizeof(olsr_db_neighbors_ett_entry_t));
            memcpy(entry->neighbor_main_addr, neighbor_main_addr, ETH_ALEN);
            entry->timeval_recv.tv_sec = 0;
            entry->timeval_recv.tv_usec = 0;

            for(i = 0; i < ETT_SW_SIZE; i++) {
                entry->time_int_sw[i] = 0;
            }

            entry->time_sw_ptr = 0;
            HASH_ADD_KEYPTR(hh, neighbor_set_ett, entry->neighbor_main_addr, ETH_ALEN, entry);
        }
    }

    //save the received time
    entry->time_int_sw[entry->time_sw_ptr] = recved_time;
    entry->time_sw_ptr = (entry->time_sw_ptr + 1) % ETT_SW_SIZE;
    return true;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_ett_report(char** str_out) {
    int report_ett_str_len = 98;
    olsr_db_neighbors_ett_entry_t* current_entry = neighbor_set_ett;
    char* output;
    char entry_str[report_ett_str_len + 1];

    output = malloc(sizeof(char) * report_ett_str_len * (4 + HASH_COUNT(current_entry)) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+----------------------+\n");
    strcat(output, "| neighbor address  | shortest time [usec] |\n");
    strcat(output, "+-------------------+----------------------+\n");

    uint32_t min_time;

    while(current_entry != NULL) {
        if((min_time = get_min_time_from_neighbor(current_entry->neighbor_main_addr)) != false) {
            snprintf(entry_str, report_ett_str_len + 1, "|" MAC "| %20i |\n",
                     EXPLODE_ARRAY6(current_entry->neighbor_main_addr),
                     min_time);
        }
        else {
            snprintf(entry_str, report_ett_str_len + 1, "|" MAC "| %20s |\n",
                     EXPLODE_ARRAY6(current_entry->neighbor_main_addr),
                     "none");
        }

        strcat(output, entry_str);
        current_entry = current_entry->hh.next;
    }

    strcat(output, "+-------------------+----------------------+\n");
    *str_out = output;
    return true;
}

