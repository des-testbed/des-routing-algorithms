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
#include "../batman_sw.h"
#include "../../helper.h"
#include <dessert.h>
#include <time.h>


typedef struct aodv_brclog_entry {
    uint8_t 		shost_ether[ETH_ALEN];
    batman_sw_t*    sw;
    UT_hash_handle	hh;
} olsr_brclog_entry_t;

olsr_brclog_entry_t*	brclog_set = NULL;
timeslot_t*				brclog_ts;


void purge_brcid_entry(time_t timestamp, void* object) {
    olsr_brclog_entry_t* entry = object;
    batman_sw_destroy(entry->sw);
    HASH_DEL(brclog_set, entry);
    free(entry);
}

int batman_db_brct_init() {
    return timeslot_create(&brclog_ts, BRC_PURGE_TIME , purge_brcid_entry);
}


int batman_db_brct_addid(uint8_t shost_ether[ETH_ALEN], uint16_t brc_id) {
    olsr_brclog_entry_t* entry;
    time_t t = time(0);
    timeslot_purgeobjects(brclog_ts, t);
    HASH_FIND(hh, brclog_set, shost_ether, ETH_ALEN, entry);

    if(entry == NULL) {
        entry = malloc(sizeof(olsr_brclog_entry_t));

        if(entry == NULL) {
            return false;
        }

        if(batman_sw_create(&entry->sw, WINDOW_SIZE) != true) {
            free(entry);
            return true;
        }

        memcpy(entry->shost_ether, shost_ether, ETH_ALEN);
        HASH_ADD_KEYPTR(hh, brclog_set, entry->shost_ether, ETH_ALEN, entry);
        batman_sw_addsn(entry->sw, brc_id);
        timeslot_addobject(brclog_ts, t, entry);
        return true;
    }

    batman_sw_element_t* el = entry->sw->head;

    while(el != NULL) {
        if(el->seq_num == brc_id) {
            return false;
        }

        if(el == entry->sw->tail && hf_seq_comp_i_j(el->seq_num, brc_id) >= 0) {
            return false;
        }

        el = el->prev;
    }

    batman_sw_addsn(entry->sw, brc_id);
    timeslot_addobject(brclog_ts, t, entry);
    return true;
}
