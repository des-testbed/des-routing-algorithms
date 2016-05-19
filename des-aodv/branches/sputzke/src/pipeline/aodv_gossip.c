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

#include "../config.h"
#include "../helper.h"
#include "aodv_pipeline.h"

#include <dessert.h>
#include <pthread.h>
#include <utlist.h>

typedef struct hold_queue {
    struct timeval timeout;
    dessert_msg_t *msg;
    struct hold_queue *prev, *next;
    int quantity;
} hold_queue_t;

typedef struct hold_queue hold_queue_elem_t;

static pthread_mutex_t hold_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static dessert_periodic_t *hold_queue_periodic = NULL;
static hold_queue_t *hold_queue = NULL;

int aodv_gossip_0(){
    return (random() < (((long double) gossip_p)*((long double) RAND_MAX)));
}

dessert_per_result_t aodv_gossip_3(void *data __attribute__((unused)),
                                   struct timeval *scheduled,
                                   struct timeval *interval __attribute__((unused))) {
     pthread_mutex_lock(&hold_queue_mutex);
     while(hold_queue) {
        hold_queue_elem_t *head = hold_queue;
        if(dessert_timevalcmp(&head->timeout, scheduled) > 0) {
            break;
        }

        DL_DELETE(hold_queue, head);
        if(head->quantity >= 2) {
            continue;
        }

        //temporarily unlock while sending packet
        pthread_mutex_unlock(&hold_queue_mutex);
        dessert_ext_t* rreq_ext;
        dessert_msg_getext(head->msg, &rreq_ext, RREQ_EXT_TYPE, 0);
        struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) rreq_ext->data;
        struct ether_header* l25h = dessert_msg_getl25ether(head->msg);

        dessert_debug("incoming RREQ from " MAC " over " MAC " to " MAC " seq=%ju ttl=%ju | %s", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(head->msg->l2h.ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), (uintmax_t)rreq_msg->originator_sequence_number, (uintmax_t)head->msg->ttl, "send finally (GOSSIP_3)");
        dessert_meshsend(head->msg, NULL);
        pthread_mutex_lock(&hold_queue_mutex);
    }

    if(!hold_queue) {
        //returning DESSERT_PER_UNREGISTER would delete the periodic _after_ unlocking the mutex
        dessert_periodic_del(hold_queue_periodic);
        hold_queue_periodic = NULL;
    }

    pthread_mutex_unlock(&hold_queue_mutex);
    return DESSERT_PER_KEEP;
}

//hold_queue_mutex must be locked
static hold_queue_elem_t *aodv_gossip_hold_queue_search(dessert_msg_t *msg) {
    struct ether_header* search_l25h = dessert_msg_getl25ether(msg);
    hold_queue_elem_t *elem;
    DL_FOREACH(hold_queue, elem) {
        struct ether_header* l25h = dessert_msg_getl25ether(elem->msg);
        bool same = mac_equal(search_l25h->ether_shost, l25h->ether_shost) && mac_equal(search_l25h->ether_dhost, l25h->ether_dhost);
        if(same) {
            return elem;
        }
    }
    return NULL;
}

//hold_queue_mutex must be locked
static void aodv_gossip_hold_queue_add(dessert_msg_t *msg) {
    hold_queue_elem_t *el = aodv_gossip_hold_queue_search(msg);

    if(!el) {
        hold_queue_elem_t *el = malloc(sizeof(*el));
        gettimeofday(&el->timeout, NULL);
        struct timeval hold_queue_duration;
        dessert_ms2timeval(3 * NODE_TRAVERSAL_TIME, &hold_queue_duration);
        dessert_timevaladd2(&el->timeout, &el->timeout, &hold_queue_duration);
        DL_APPEND(hold_queue, el);
        el->quantity = 1;
    }
    else {
        dessert_msg_destroy(el->msg);
    }
    dessert_msg_clone(&el->msg, msg, false);
    if(!hold_queue_periodic) {
        struct timeval hold_queue_poll_interval;
        dessert_ms2timeval(NODE_TRAVERSAL_TIME, &hold_queue_poll_interval);
        dessert_periodic_add(aodv_gossip_3, NULL, NULL, &hold_queue_poll_interval);
    }
}

//hold_queue_mutex must be locked
static void aodv_gossip_hold_queue_drop(dessert_msg_t *msg) {
    hold_queue_elem_t *el = aodv_gossip_hold_queue_search(msg);
    if(el) {
        DL_DELETE(hold_queue, el);
        dessert_msg_destroy(el->msg);
    }
}

void aodv_gossip_capt_rreq(dessert_msg_t *msg) {
    pthread_mutex_lock(&hold_queue_mutex);
    hold_queue_elem_t *el = aodv_gossip_hold_queue_search(msg);
    if(!el) {
        pthread_mutex_unlock(&hold_queue_mutex);
        return;
    }

    dessert_ext_t* rreq_ext;
    dessert_msg_getext(msg, &rreq_ext, RREQ_EXT_TYPE, 0);
    struct aodv_msg_rreq *capt_rreq = (struct aodv_msg_rreq*) rreq_ext->data;
    dessert_msg_getext(el->msg, &rreq_ext, RREQ_EXT_TYPE, 0);
    struct aodv_msg_rreq *held_rreq = (struct aodv_msg_rreq*) rreq_ext->data;

    int cmp_result = hf_comp_u32(held_rreq->originator_sequence_number, capt_rreq->originator_sequence_number);
    if(cmp_result > 0) {
        //captured request is newer, delete this held msg
        DL_DELETE(hold_queue, el);
        dessert_msg_destroy(el->msg);
    }
    else if(cmp_result == 0) {
        el->quantity += 1;
    }

    pthread_mutex_unlock(&hold_queue_mutex);
}

int aodv_gossip(dessert_msg_t* msg){
    switch(gossip_type) {
        case GOSSIP_NONE:
            return true;
        case GOSSIP_0:
            return aodv_gossip_0();
        case GOSSIP_1:
            /* u8 hop count */
            return (msg->u8 <= 1) || aodv_gossip_0();
        case GOSSIP_3: {
            if(aodv_gossip_0()) {
                pthread_mutex_lock(&hold_queue_mutex);
                aodv_gossip_hold_queue_drop(msg);
                pthread_mutex_unlock(&hold_queue_mutex);
                return true;
            }
            else {
                pthread_mutex_lock(&hold_queue_mutex);
                aodv_gossip_hold_queue_add(msg);
                pthread_mutex_unlock(&hold_queue_mutex);
                return false;
            }
        }
        default: {
            assert(false);
            return true;
        }
    }
}
