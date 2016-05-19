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
#include "timeslot.h"
#include "../config.h"

int create_new_ts_element(timeslot_element_t** ts_el_out, time_t timestamp, void* object) {
    timeslot_element_t* new_el;

    new_el = malloc(sizeof(timeslot_element_t));

    if(new_el == NULL) {
        return false;
    }

    new_el->next = NULL;
    new_el->prev = NULL;
    new_el->timestamp = timestamp;
    new_el->object = object;
    *ts_el_out = new_el;
    return true;
}

int timeslot_create(timeslot_t** ts_out, time_t pudge_timeout, purge_object_t* callback) {
    timeslot_t* ts;
    ts = malloc(sizeof(timeslot_t));

    if(ts == NULL) {
        return false;
    }

    ts->head = NULL;
    ts->tail = NULL;
    ts->size = 0;
    ts->purge_timeout = pudge_timeout;
    ts->object_purger = callback;
    ts->elements_hash = NULL;
    *ts_out = ts;
    return true;
}

int timeslot_destroy(timeslot_t* ts) {
    timeslot_element_t* temp_el;
    timeslot_element_t* search_el = ts->tail;

    while(search_el != NULL) {
        temp_el = search_el;
        search_el = search_el->next;
        HASH_DEL(ts->elements_hash, temp_el);

        if(ts->object_purger != NULL) {
            ts->object_purger(temp_el->timestamp, temp_el->object);
        }

        free(temp_el);
    }

    free(ts);
    return true;
};

int timeslot_change_pt(timeslot_t* ts, time_t pudge_timeout) {
    ts->purge_timeout = pudge_timeout;

    if(ts->head != NULL) {
        timeslot_purgeobjects(ts, ts->head->timestamp);
    }

    return true;
}

int timeslot_purgeobjects(timeslot_t* ts, time_t timestamp) {
    timeslot_element_t* search_el = ts->tail;

    while(search_el != NULL && search_el->timestamp + ts->purge_timeout <= timestamp) {
        search_el = search_el->next;
        HASH_DEL(ts->elements_hash, ts->tail);

        if(search_el != NULL) {
            search_el->prev = NULL;
        }

        if(ts->object_purger != NULL) {
            ts->object_purger(ts->tail->timestamp, ts->tail->object);
        }

        free(ts->tail);

        if(ts->tail == ts->head) {
            ts->tail = ts->head = search_el;
        }
        else {
            ts->tail = search_el;
        }

        ts->size--;
    }

    return true;
}

int timeslot_addobject(timeslot_t* ts, time_t timestamp, void* object) {
    timeslot_element_t* new_el;

    // first find element with *object pointer and delete this element
    timeslot_deleteobject(ts, object);

    // if new element out if date -> execute object_pudger and do nothing
    if((ts->head != NULL) && (timestamp + ts->purge_timeout <= ts->head->timestamp)) {
        ts->object_purger(timestamp, object);
        return true;
    }

    if(create_new_ts_element(&new_el, timestamp, object) == false) {
        return false;
    }

    HASH_ADD_KEYPTR(hh, ts->elements_hash, &new_el->object, sizeof(void*), new_el);

    // if this is a first element -> set tail and head
    if(ts->size == 0) {
        ts->head = ts->tail = new_el;
        ts->size = 1;
        return true;
    }

    // insert new element to apropriate place
    timeslot_element_t* search_el = ts->head;

    while(search_el->prev != NULL && timestamp < search_el->timestamp) {
        // we search for an smaller element
        search_el = search_el->prev;
    }

    if(timestamp >= search_el->timestamp) {
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
        // insert new element bevor search element
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

    // drop all elements out of WINDOW_SIZE range
    timeslot_purgeobjects(ts, ts->head->timestamp);
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

void timeslot_report(timeslot_t* ts) {
    printf("---------- Time Slot  -------------\n");
    printf("Timeslot size : %i\n", ts->size);
    printf("max timestamp : %i\n", (int)ts->head->timestamp);
    timeslot_element_t* search_el = ts->tail;
    printf("elements : (");

    while(search_el != NULL) {
        printf(" %i", (int)search_el->timestamp);
        search_el = search_el->next;
    }

    printf(")\n");
}

