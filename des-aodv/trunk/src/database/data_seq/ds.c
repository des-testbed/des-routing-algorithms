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

#include "ds.h"

typedef struct data_packet_id {
    uint8_t         src_addr[ETH_ALEN]; // key
    uint16_t        seq_num;
    UT_hash_handle  hh;
} data_packet_id_t;

typedef struct aodv_ds {
    data_packet_id_t*	entries;
    timeslot_t*			ts;
} data_seq_t;

data_seq_t ds;

data_packet_id_t* ds_entry_create(mac_addr src_addr, uint16_t seq_num) {
    data_packet_id_t* new_entry;
    new_entry = malloc(sizeof(data_packet_id_t));

    if(new_entry == NULL) {
        dessert_warn("malloc returned NULL");
        return NULL;
    }

    mac_copy(new_entry->src_addr, src_addr);
    new_entry->seq_num = seq_num;

    return new_entry;
}

void db_nt_on_ds_timeout(struct timeval* timestamp, void* src_object, void* object) {
    data_packet_id_t* curr_entry = object;
    dessert_debug("data seq timeout:" MAC " last_seq_num=% " PRIu16 "", EXPLODE_ARRAY6(curr_entry->src_addr), curr_entry->seq_num);
    HASH_DEL(ds.entries, curr_entry);

    free(curr_entry);
}

int db_ds_init() {
    timeslot_t* new_ts;
    struct timeval timeout;
    uint32_t ds_int_msek = AODV_DATA_SEQ_TIMEOUT;
    timeout.tv_sec = ds_int_msek / 1000;
    timeout.tv_usec = (ds_int_msek % 1000) * 1000;

    if(timeslot_create(&new_ts, &timeout, NULL, db_nt_on_ds_timeout) != true) {
        return false;
    }

    ds.entries = NULL;
    ds.ts = new_ts;
    return true;
}

int aodv_db_ds_capt_data_seq(mac_addr src_addr, uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp) {

    data_packet_id_t* curr_entry = NULL;
    HASH_FIND(hh, ds.entries, src_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL) {
        //never got data from this host

        curr_entry = ds_entry_create(src_addr, data_seq_num);

        if(curr_entry == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, ds.entries, curr_entry->src_addr, ETH_ALEN, curr_entry);
        dessert_debug("data seq - new source: " MAC " data_seq=% " PRIu16 "", EXPLODE_ARRAY6(src_addr), data_seq_num);
        timeslot_addobject(ds.ts, timestamp, curr_entry);
        return true;
    }

    //data source is known
    if((curr_entry->seq_num - data_seq_num > (1 << 15)) || (curr_entry->seq_num < data_seq_num)) {
        //data packet is newer
        curr_entry->seq_num = data_seq_num;
        timeslot_addobject(ds.ts, timestamp, curr_entry);
        return true;
    }

    //data packet is old
    return false;
}

void ds_report(char** str_out) {
    timeslot_report(ds.ts, str_out);
}

int db_ds_cleanup(struct timeval* timestamp) {
    return timeslot_purgeobjects(ds.ts, timestamp);
}

