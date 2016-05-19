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
#include <stdlib.h>
#include <time.h>
#include "rl_seq.h"
#include "../../config.h"
#include "sw.h"
#include "../timeslot.h"
#include "../../helper.h"

typedef struct rl_packet_id {
    uint8_t src_dest_addr[ETH_ALEN * 2]; // key
    sw_t* sw;
    UT_hash_handle hh;
} rl_packet_id_t;

rl_packet_id_t* rl_entrys = NULL;
timeslot_t* rl_ts = NULL;

void on_rl_timeout(struct timeval* purge_time, void* src_object, void* object) {
    rl_packet_id_t* rl_entry = object;
    HASH_DEL(rl_entrys, rl_entry);
    sw_destroy(rl_entry->sw);
    free(rl_entry);
}

int rl_table_init() {
    return timeslot_create(&rl_ts, NULL, on_rl_timeout);
}

rl_packet_id_t* create_entry(uint8_t key[2 * ETH_ALEN]) {
    rl_packet_id_t* entry = malloc(sizeof(rl_packet_id_t));

    if(entry == NULL) {
        return NULL;
    }

    sw_t* sw;

    if(sw_create(&sw, window_size) != true) {
        free(entry);
    }

    memcpy(entry->src_dest_addr, key, ETH_ALEN * 2);
    entry->sw = sw;
    return entry;
}

uint16_t rl_get_nextseq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN]) {
    uint8_t key[ETH_ALEN * 2];
    memcpy(key, src_addr, ETH_ALEN);
    memcpy(key + ETH_ALEN, dest_addr, ETH_ALEN);
    rl_packet_id_t* entry;
    HASH_FIND(hh, rl_entrys, key, ETH_ALEN * 2, entry);
    uint16_t packet_seq = 0;

    if(entry == NULL) {
        entry = create_entry(key);

        if(entry == NULL) {
            return 0;
        }

        sw_addsn(entry->sw, 0);
        HASH_ADD_KEYPTR(hh, rl_entrys, entry->src_dest_addr, ETH_ALEN * 2, entry);
    }
    else {
        sw_addsn(entry->sw, entry->sw->head->seq_num + 1);
        packet_seq = entry->sw->head->seq_num;
    }

    struct timeval purge_time;

    gettimeofday(&purge_time, NULL);

    struct timeval timeout;

    timeout.tv_sec = window_size;

    timeout.tv_usec = 0;

    hf_add_tv(&purge_time, &timeout, &purge_time);

    timeslot_addobject(rl_ts, &purge_time, entry);

    return packet_seq;
}

uint8_t rl_check_seq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN], uint16_t seq_num) {
    timeslot_purgeobjects(rl_ts);
    uint8_t key[ETH_ALEN * 2];
    memcpy(key, src_addr, ETH_ALEN);
    memcpy(key + ETH_ALEN, dest_addr, ETH_ALEN);
    rl_packet_id_t* entry;
    HASH_FIND(hh, rl_entrys, key, ETH_ALEN * 2, entry);

    if(entry == NULL) {
        return false;
    }

    sw_element_t* el = entry->sw->head;

    while(el != NULL) {
        if(el->seq_num == seq_num) {
            return true;
        }

        el = el->prev;
    }

    return false;
}

void rl_add_seq(uint8_t src_addr[ETH_ALEN], uint8_t dest_addr[ETH_ALEN], uint16_t seq_num) {
    uint8_t key[ETH_ALEN * 2];
    memcpy(key, src_addr, ETH_ALEN);
    memcpy(key + ETH_ALEN, dest_addr, ETH_ALEN);
    rl_packet_id_t* entry = NULL;
    HASH_FIND(hh, rl_entrys, key, ETH_ALEN * 2, entry);

    if(entry == NULL) {
        entry = create_entry(key);

        if(entry == NULL) {
            return;
        }

        HASH_ADD_KEYPTR(hh, rl_entrys, entry->src_dest_addr, ETH_ALEN * 2, entry);
    }

    sw_addsn(entry->sw, seq_num);
    struct timeval purge_time;
    gettimeofday(&purge_time, NULL);
    struct timeval timeout;
    timeout.tv_sec = window_size;
    timeout.tv_usec = 0;
    hf_add_tv(&purge_time, &timeout, &purge_time);
    timeslot_addobject(rl_ts, &purge_time, entry);
}
