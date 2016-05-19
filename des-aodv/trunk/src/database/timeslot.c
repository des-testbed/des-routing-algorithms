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
#include <time.h>
#include "timeslot.h"
#include "../config.h"
#include "../helper.h"

int create_new_ts_element(timeslot_element_t** ts_el_out, struct timeval* timestamp, void* object) {
    timeslot_element_t* new_el;

    new_el = malloc(sizeof(timeslot_element_t));

    if(new_el == NULL) {
        return false;
    }

    new_el->next = NULL;
    new_el->prev = NULL;

    struct timeval* purge_time = malloc(sizeof(struct timeval));
    purge_time->tv_sec = timestamp->tv_sec;
    purge_time->tv_usec = timestamp->tv_usec;
    new_el->purge_time = purge_time;

    new_el->object = object;
    *ts_el_out = new_el;
    return true;
}

int timeslot_create(timeslot_t** ts_out, struct timeval* purge_timeout, void* src_object, object_purger_t* object_purger) {
    timeslot_t* ts;
    ts = malloc(sizeof(timeslot_t));

    if(ts == NULL) {
        return false;
    }

    ts->head = NULL;
    ts->tail = NULL;
    ts->size = 0;
    ts->object_purger = object_purger;
    ts->purge_timeout = malloc(sizeof(struct timeval));
    ts->purge_timeout->tv_sec = purge_timeout->tv_sec;
    ts->purge_timeout->tv_usec = purge_timeout->tv_usec;
    ts->src_object = src_object;
    ts->elements_hash = NULL;
    *ts_out = ts;
    return true;
}

int timeslot_destroy(timeslot_t* ts) {
    timeslot_element_t* search_el = ts->elements_hash;

    while(search_el != NULL) {
        HASH_DEL(ts->elements_hash, search_el);
        free(search_el);
        search_el = ts->elements_hash;
    }

    free(ts);
    return true;
}

int timeslot_purgeobjects(timeslot_t* ts, struct timeval* curr_time) {
    timeslot_element_t* search_el = ts->tail;

    while(search_el != NULL && dessert_timevalcmp(search_el->purge_time, curr_time) <= 0) {
        HASH_DEL(ts->elements_hash, search_el);

        if(search_el == ts->head) {
            ts->tail = ts->head = NULL;
        }
        else {
            ts->tail = search_el->next;

            if(ts->tail != NULL) {
                ts->tail->prev = NULL;
            }
        }

        ts->size--;
        timeslot_element_t* new_tail = ts->tail;

        if(ts->object_purger != NULL) {
            ts->object_purger(search_el->purge_time, ts->src_object, search_el->object);
        }

        free(search_el);
        search_el = new_tail;
    }

    return true;
}

int timeslot_addobject(timeslot_t* ts, struct timeval* timestamp, void* object) {
    timeslot_element_t* new_el;
    struct timeval purge_time;
    dessert_timevaladd2(&purge_time, ts->purge_timeout, timestamp);

    if(create_new_ts_element(&new_el, &purge_time, object) == false) {
        return false;
    }

    // first find element with *object pointer and delete this element
    timeslot_deleteobject(ts, object);

    HASH_ADD_KEYPTR(hh, ts->elements_hash, &new_el->object, sizeof(void*), new_el);

    // if this is a first element -> set tail and head
    if(ts->size == 0) {
        ts->head = ts->tail = new_el;
        ts->size = 1;
        return true;
    }

    // insert new element into appropriate place
    timeslot_element_t* search_el = ts->head;

    while(search_el->prev != NULL && (dessert_timevalcmp(&purge_time, search_el->purge_time) < 0)) {
        // we search for an smaller element
        search_el = search_el->prev;
    }

    if(dessert_timevalcmp(&purge_time, search_el->purge_time) >= 0) {
        // insert new element after search element
        new_el->prev = search_el;
        new_el->next = search_el->next;
        search_el->next = new_el;

        if(new_el->next != NULL) {
            new_el->next->prev = new_el;
        }

        if(ts->head == search_el) {
            ts->head = new_el;
        }
    }
    else {
        // insert new element before search element
        new_el->prev = search_el->prev;
        new_el->next = search_el;
        search_el->prev = new_el;

        if(new_el->prev != NULL) {
            new_el->prev->next = new_el;
        }

        if(ts->tail == search_el) {
            ts->tail = new_el;
        }
    }

    ts->size++;

    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    timeslot_purgeobjects(ts, &curr_time);
    return true;
}

int timeslot_addobject_varpurge(timeslot_t* ts, struct timeval* timestamp, void* object, struct timeval* not_def_lifetime) {
    timeslot_element_t* new_el;
    struct timeval purge_time;
    dessert_timevaladd2(&purge_time, not_def_lifetime, timestamp);

    if(create_new_ts_element(&new_el, &purge_time, object) == false) {
        return false;
    }

    // first find element with *object pointer and delete this element
    timeslot_deleteobject(ts, object);

    HASH_ADD_KEYPTR(hh, ts->elements_hash, &new_el->object, sizeof(void*), new_el);

    // if this is a first element -> set tail and head
    if(ts->size == 0) {
        ts->head = ts->tail = new_el;
        ts->size = 1;
        return true;
    }

    // insert new element into appropriate place
    timeslot_element_t* search_el = ts->head;

    while(search_el->prev != NULL && (dessert_timevalcmp(&purge_time, search_el->purge_time) < 0)) {
        // we search for an smaller element
        search_el = search_el->prev;
    }

    if(dessert_timevalcmp(&purge_time, search_el->purge_time) >= 0) {
        // insert new element after search element
        new_el->prev = search_el;
        new_el->next = search_el->next;
        search_el->next = new_el;

        if(new_el->next != NULL) {
            new_el->next->prev = new_el;
        }

        if(ts->head == search_el) {
            ts->head = new_el;
        }
    }
    else {
        // insert new element before search element
        new_el->prev = search_el->prev;
        new_el->next = search_el;
        search_el->prev = new_el;

        if(new_el->prev != NULL) {
            new_el->prev->next = new_el;
        }

        if(ts->tail == search_el) {
            ts->tail = new_el;
        }
    }

    ts->size++;

    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    timeslot_purgeobjects(ts, &curr_time);
    return true;
}

int timeslot_deleteobject(timeslot_t* ts, void* object) {
    // first find element with *object pointer
    timeslot_element_t* old_el;
    HASH_FIND(hh, ts->elements_hash, &object, sizeof(void*), old_el);

    // then delete if found
    if(old_el != NULL) {
        if(old_el->prev != NULL) {
            old_el->prev->next = old_el->next;
        }

        if(old_el->next != NULL) {
            old_el->next->prev = old_el->prev;
        }

        if(ts->tail == old_el) {
            ts->tail = ts->tail->next;
        }

        if(ts->head == old_el) {
            ts->head = ts->head->prev;
        }

        HASH_DEL(ts->elements_hash, old_el);
        free(old_el);
        ts->size--;
        return true;
    }

    return false;
}

void timeslot_report(timeslot_t* ts, char** str_out) {

    char* output = calloc(1, 1024);
    char entry[128];

    if(ts == NULL) {
        snprintf(entry, 128, "Time Slot: NULL\n");
        strcat(output, entry);
        *str_out = output;
        return;
    }

    if(ts->head == NULL) {
        snprintf(entry, 128, "Time Slot: EMPTY\n");
        strcat(output, entry);
        *str_out = output;
        return;
    }

    snprintf(entry, 128, "---------- Time Slot  -------------\n");
    strcat(output, entry);

    snprintf(entry, 128, "Timeslot size : %" PRIu32 "\n", ts->size);
    strcat(output, entry);

    snprintf(entry, 128, "max timestamp : %ld.%.6ld\n", ts->head->purge_time->tv_sec, ts->head->purge_time->tv_usec);
    strcat(output, entry);

    timeslot_element_t* search_el = ts->tail;

    while(search_el != NULL && search_el->purge_time != NULL) {
        snprintf(entry, 128, "element       : ");
        strcat(output, entry);
        snprintf(entry, 128, "%ld.%.6ld\n", search_el->purge_time->tv_sec, search_el->purge_time->tv_usec);
        strcat(output, entry);
        search_el = search_el->next;
    }

    *str_out = output;
}

