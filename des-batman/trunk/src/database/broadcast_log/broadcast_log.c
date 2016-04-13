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

#include "broadcast_log.h"
#include "../../config.h"
#include "../timeslot.h"
#include "../../helper.h"
#include <time.h>


typedef struct batman_brclog_entry {
    uint8_t 		shost_ether[ETH_ALEN];
    uint32_t		brc_id;
    UT_hash_handle	hh;
} batman_brclog_entry_t;

batman_brclog_entry_t*	brclog_set = NULL;
timeslot_t*				brclog_ts;

batman_brclog_entry_t*	ogmlog_set = NULL;
timeslot_t*				ogmlog_ts;


void purge_brcid_entry(time_t timestamp, void* object) {
    batman_brclog_entry_t* entry = object;
    HASH_DEL(brclog_set, entry);
    free(entry);
}

void purge_ogmid_entry(time_t timestamp, void* object) {
    batman_brclog_entry_t* entry = object;
    HASH_DEL(ogmlog_set, entry);
    free(entry);
}

int batman_db_brct_init() {
    timeslot_create(&brclog_ts, BROADCAST_LOG_TIMEOUT, purge_brcid_entry);
    timeslot_create(&ogmlog_ts, RL_TIMEOUT, purge_ogmid_entry);
    return true;
}

int batman_db_brct_addid(uint8_t shost_ether[ETH_ALEN], uint16_t brc_id) {
    batman_brclog_entry_t* entry;
    timeslot_purgeobjects(brclog_ts, time(0));
    HASH_FIND(hh, brclog_set, shost_ether, ETH_ALEN, entry);

    if(entry == NULL) {
        entry = malloc(sizeof(batman_brclog_entry_t));

        if(entry == NULL) {
            return false;
        }

        memcpy(entry->shost_ether, shost_ether, ETH_ALEN);
        HASH_ADD_KEYPTR(hh, brclog_set, entry->shost_ether, ETH_ALEN, entry);
        entry->brc_id = brc_id;
        timeslot_addobject(brclog_ts, time(0), entry);
        return true;
    }

    if(hf_seq_comp_i_j(entry->brc_id, brc_id) < 0) {
        entry->brc_id = brc_id;
        timeslot_addobject(brclog_ts, time(0), entry);
        return true;
    }

    return false;
}

int batman_db_ogm_addid(uint8_t shost_ether[ETH_ALEN], uint16_t seq) {
    batman_brclog_entry_t* entry;
    timeslot_purgeobjects(ogmlog_ts, time(0));
    HASH_FIND(hh, ogmlog_set, shost_ether, ETH_ALEN, entry);

    if(entry == NULL) {
        entry = malloc(sizeof(batman_brclog_entry_t));

        if(entry == NULL) {
            return false;
        }

        memcpy(entry->shost_ether, shost_ether, ETH_ALEN);
        HASH_ADD_KEYPTR(hh, ogmlog_set, entry->shost_ether, ETH_ALEN, entry);
        entry->brc_id = seq;
        timeslot_addobject(ogmlog_ts, time(0), entry);
        return true;
    }

    if(hf_seq_comp_i_j(entry->brc_id, seq) < 0) {
        entry->brc_id = seq;
        timeslot_addobject(ogmlog_ts, time(0), entry);
        return true;
    }

    return false;
}


