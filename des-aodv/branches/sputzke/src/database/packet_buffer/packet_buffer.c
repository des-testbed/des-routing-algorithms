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

#include "packet_buffer.h"
#include "../../config.h"
#include "../timeslot.h"


typedef struct fifo_list_el {
    dessert_msg_t*          msg;
    struct fifo_list_el*    next;
} fifo_list_el_t;

typedef struct fifo_list {
    fifo_list_el_t* head;
    fifo_list_el_t* tail;
    uint32_t        size;
} fifo_list_t;

/**
 * Packet buffer element
 */
typedef struct pb_el {
    uint8_t         dhost_ether[ETH_ALEN];
    fifo_list_t     fl;
    UT_hash_handle  hh;
} pb_el_t;

/**
 * Packet buffer
 */
typedef struct pb {
    pb_el_t*    entries;
    timeslot_t* ts;
} pb_t;

pb_t pbt;

void purge_packets(struct timeval* timestamp, void* src_object, void* object) {
    dessert_debug("purging packet buffer");
    pb_el_t* pb_el = object;
    fifo_list_el_t* fl_el = pb_el->fl.head;

    while(fl_el != NULL) {
        dessert_msg_destroy(pb_el->fl.head->msg);
        pb_el->fl.head = pb_el->fl.head->next;
        free(fl_el);
        fl_el = pb_el->fl.head;
    }

    HASH_DEL(pbt.entries, pb_el);
    free(pb_el);
}

int pb_init() {
    pbt.entries = NULL;
    struct timeval timeout;
    timeout.tv_sec = BLACKLIST_TIMEOUT / 1000;
    timeout.tv_usec = (BLACKLIST_TIMEOUT % 1000) * 1000;
    return timeslot_create(&pbt.ts, &timeout, NULL, purge_packets);
}

void fl_push_packet(fifo_list_t* fl, dessert_msg_t* msg) {
    fifo_list_el_t* new_el = malloc(sizeof(fifo_list_el_t));

    if(new_el == NULL) {
        return;
    }

    dessert_msg_t* msg_copy;
    dessert_msg_clone(&msg_copy, msg, false);
    new_el->msg = msg_copy;
    new_el->next = NULL;

    if(fl->head == NULL) {
        fl->head = new_el;
        fl->tail = new_el;
    }
    else {
        fl->tail->next = new_el;
        fl->tail = new_el;
    }

    fl->size++;

    if(fl->size > FIFO_BUFFER_MAX_ENTRY_SIZE) {
        dessert_debug("reached maximum number of packets in buffer for this destination host -> purge old packets");
        dessert_msg_destroy(fl->head->msg);
        fifo_list_el_t* curr_head = fl->head;
        fl->head = fl->head->next;
        free(curr_head);
        fl->size--;
    }
}

dessert_msg_t* fl_pop_packet(fifo_list_t* fl) {
    dessert_msg_t* msg;
    fifo_list_el_t* head_el;

    if(fl->head == NULL) {
        return NULL;
    }

    head_el = fl->head;
    msg = fl->head->msg;

    if(fl->head == fl->tail) {
        fl->head = fl->tail = NULL;
    }
    else {
        fl->head = fl->head->next;
    }

    free(head_el);
    fl->size--;
    return msg;
}

void pb_push_packet(mac_addr dhost_ether, dessert_msg_t* msg, struct timeval* timestamp) {
    pb_cleanup(timestamp);
    pb_el_t* pb_el;
    HASH_FIND(hh, pbt.entries, dhost_ether, ETH_ALEN, pb_el);

    if(pb_el == NULL) {
        pb_el = malloc(sizeof(pb_el_t));

        if(pb_el == NULL) {
            dessert_msg_destroy(msg);
            return;
        }

        mac_copy(pb_el->dhost_ether, dhost_ether);
        pb_el->fl.head = pb_el->fl.tail = NULL;
        pb_el->fl.size = 0;
        HASH_ADD_KEYPTR(hh, pbt.entries, pb_el->dhost_ether, ETH_ALEN, pb_el);
    }

    fl_push_packet(&pb_el->fl, msg);
    timeslot_addobject(pbt.ts, timestamp, pb_el);
}

dessert_msg_t* pb_pop_packet(mac_addr dhost_ether) {
    pb_el_t* pb_el;
    HASH_FIND(hh, pbt.entries, dhost_ether, ETH_ALEN, pb_el);

    if(pb_el == NULL) {
        return NULL;
    }

    dessert_msg_t* msg = fl_pop_packet(&pb_el->fl);

    if(pb_el->fl.head == NULL) {
        HASH_DEL(pbt.entries, pb_el);
        timeslot_deleteobject(pbt.ts, pb_el);
        free(pb_el);
    }

    return msg;
}

void pb_report(char** str_out) {
    timeslot_report(pbt.ts, str_out);
}

int pb_cleanup(struct timeval* timestamp) {
    return timeslot_purgeobjects(pbt.ts, timestamp);
}

