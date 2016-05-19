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

#include <string.h>
#include <uthash.h>
#include "../../config.h"
#include "../../helper.h"
#include "aodv_st.h"

typedef struct schedule {
    struct timeval      execute_ts;
    struct __attribute__((__packed__)) {
        uint8_t         ether_addr[ETH_ALEN];
        uint8_t         schedule_id;
    };
    void*               schedule_param;
    struct schedule*    next;
    struct schedule*    prev;
    UT_hash_handle      hh;
} schedule_t;

schedule_t* first_schedule = NULL;

schedule_t* hash_table = NULL;

schedule_t* create_schedule(struct timeval* execute_ts, mac_addr ether_addr, uint8_t type, void* param) {
    schedule_t* s = malloc(sizeof(schedule_t));

    if(s == NULL) {
        return NULL;
    }

    s->execute_ts = *execute_ts;
    mac_copy(s->ether_addr, ether_addr);
    s->schedule_id = type;
    s->schedule_param = param;
    s->next = s->prev = NULL;
    return s;
}

int aodv_db_sc_addschedule(struct timeval* execute_ts, mac_addr ether_addr, uint8_t type, void* param) {
    aodv_db_sc_dropschedule(ether_addr, type);

    schedule_t* next_el = first_schedule;
    schedule_t* el = create_schedule(execute_ts, ether_addr, type, param);

    if(el == NULL) {
        return false;
    }

    HASH_ADD_KEYPTR(hh, hash_table, el->ether_addr, ETH_ALEN + sizeof(uint8_t), el);

    // search for appropriate place to insert new element
    while(next_el != NULL && next_el->next != NULL && dessert_timevalcmp(execute_ts, &next_el->execute_ts) > 0) {
        if(next_el->next != NULL) {
            next_el = next_el->next;
        }
    }

    if(next_el == NULL) {
        first_schedule = el;
    }
    else {
        if(dessert_timevalcmp(&next_el->execute_ts, execute_ts) > 0) {
            if(next_el->prev != NULL) {
                next_el->prev->next = el;
                el->prev = next_el->prev;
            }
            else {
                first_schedule = el;
            }

            next_el->prev = el;
            el->next = next_el;
        }
        else {
            if(next_el->next != NULL) {
                next_el->next->prev = el;
                el->next = next_el->next;
            }

            next_el->next = el;
            el->prev = next_el;
        }
    }

    return true;
}

int aodv_db_sc_popschedule(struct timeval* timestamp, mac_addr ether_addr_out, uint8_t* type, void** param) {
    if(first_schedule != NULL && dessert_timevalcmp(&first_schedule->execute_ts, timestamp) <= 0) {
        schedule_t* sc = first_schedule;
        first_schedule = first_schedule->next;

        if(first_schedule != NULL) {
            first_schedule->prev = NULL;
        }

        mac_copy(ether_addr_out, sc->ether_addr);
        *type = sc->schedule_id;
        *param = sc->schedule_param;
        HASH_DEL(hash_table, sc);
        free(sc);
        return true;
    }

    return false;
}

int aodv_db_sc_schedule_exists(mac_addr ether_addr, uint8_t type) {
    schedule_t* schedule;
    uint8_t key[ETH_ALEN + sizeof(uint8_t)];
    mac_copy(key, ether_addr);
    memcpy(key + ETH_ALEN, &type, sizeof(uint8_t));
    HASH_FIND(hh, hash_table, key, ETH_ALEN + sizeof(uint8_t), schedule);

    if(schedule == NULL) {
        return false;
    }
    else {
        return true;
    }
}

int aodv_db_sc_dropschedule(mac_addr ether_addr, uint8_t type) {
    schedule_t* schedule;
    uint8_t key[ETH_ALEN + sizeof(uint8_t)];
    mac_copy(key, ether_addr);
    memcpy(key + ETH_ALEN, &type, sizeof(uint8_t));
    HASH_FIND(hh, hash_table, key, ETH_ALEN + sizeof(uint8_t), schedule);

    if(schedule == NULL) {
        return false;
    }

    if(schedule->prev != NULL) {
        schedule->prev->next = schedule->next;
    }
    else {
        first_schedule = schedule->next;
    }

    if(schedule->next != NULL) {
        schedule->next->prev = schedule->prev;
    }

    HASH_DEL(hash_table, schedule);
    free(schedule);
    return true;
}
