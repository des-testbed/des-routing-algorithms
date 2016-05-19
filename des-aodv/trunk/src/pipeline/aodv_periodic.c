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

#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include <string.h>
#include <pthread.h>
#include <utlist.h>

uint16_t seq_num_hello = 0;
pthread_rwlock_t hello_rwlock = PTHREAD_RWLOCK_INITIALIZER;

dessert_per_result_t aodv_periodic_send_hello(void* data, struct timeval* scheduled, struct timeval* interval) {

    dessert_msg_t* msg;
    dessert_msg_new(&msg);
    msg->ttl = 2;

    pthread_rwlock_wrlock(&hello_rwlock);
    msg->u16 = seq_num_hello++;
    pthread_rwlock_unlock(&hello_rwlock);

    dessert_ext_t* ext;
    dessert_msg_addext(msg, &ext, HELLO_EXT_TYPE, sizeof(struct aodv_msg_hello));

    struct aodv_msg_hello* hello_msg = (struct aodv_msg_hello*) ext->data;
    hello_msg->hello_rcvd_count = 0;
    hello_msg->hello_interval = hello_interval;

    dessert_msg_dummy_payload(msg, hello_size);

    dessert_meshsend(msg, NULL);
    dessert_msg_destroy(msg);
    return DESSERT_PER_KEEP;
}

dessert_per_result_t aodv_periodic_send_rreq(void* data, struct timeval* scheduled, struct timeval* interval) {
    dessert_trace("call periodic send rreq");

    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    aodv_link_break_element_t* head = NULL;

    if(!aodv_db_get_active_routes(&head)) {
        return DESSERT_PER_UNREGISTER;
    }

    aodv_link_break_element_t* dest, *tmp;
    DL_FOREACH_SAFE(head, dest, tmp) {
        dessert_debug("periodic send rreq to: " MAC " - interval=%" PRIu16 " ms", EXPLODE_ARRAY6(dest->host), rreq_interval);
        aodv_send_rreq(dest->host, &timestamp);
        free(dest);
    }
    return DESSERT_PER_KEEP;
}

dessert_per_result_t aodv_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval) {
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    if(aodv_db_cleanup(&timestamp)) {
        return DESSERT_PER_KEEP;
    }
    else {
        return DESSERT_PER_UNREGISTER;
    }
}

dessert_msg_t* aodv_create_rerr(aodv_link_break_element_t** destlist) {
    if(*destlist == NULL) {
        return NULL;
    }

    dessert_msg_t* msg;
    dessert_ext_t* ext;
    dessert_msg_new(&msg);

    // set ttl
    msg->ttl = TTL_MAX;

    // add l25h header
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
    mac_copy(rreq_l25h->ether_shost, dessert_l25_defsrc);
    mac_copy(rreq_l25h->ether_dhost, ether_broadcast);

    // add RERR ext
    dessert_msg_addext(msg, &ext, RERR_EXT_TYPE, sizeof(struct aodv_msg_rerr));
    struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) ext->data;
    rerr_msg->flags = AODV_FLAGS_RERR_N;

    // write addresses of all my mesh interfaces
    mac_addr *ifaceaddr_pointer = rerr_msg->ifaces;
    uint8_t ifaces_count = 0;
    dessert_meshif_t* iface;
    MESHIFLIST_ITERATOR_START(iface)
        assert(ifaces_count < MAX_MESH_IFACES_COUNT);

        mac_copy(*ifaceaddr_pointer, iface->hwaddr);
        ifaceaddr_pointer++;
        ifaces_count++;
    MESHIFLIST_ITERATOR_STOP;

    rerr_msg->iface_addr_count = ifaces_count;

    while(*destlist) {
        unsigned long dl_len = 0;

        //count the length of destlist up to MAX_MAC_SEQ_PER_EXT elements
        aodv_link_break_element_t* count_iter;

        for(count_iter = *destlist;
            (dl_len <= MAX_MAC_SEQ_PER_EXT) && count_iter;
            ++dl_len, count_iter = count_iter->next) {
        }

        if(dessert_msg_addext(msg, &ext, RERRDL_EXT_TYPE, dl_len * sizeof(struct aodv_mac_seq)) != DESSERT_OK) {
            break;
        }

        struct aodv_mac_seq* start = (struct aodv_mac_seq*) ext->data, *iter;

        for(iter = start; iter < start + dl_len; ++iter) {
            aodv_link_break_element_t* el = *destlist;
            mac_copy(iter->host, el->host);
            iter->sequence_number = el->sequence_number;
            dessert_debug("create rerr to: " MAC " seq=%" PRIu32 "", EXPLODE_ARRAY6(iter->host), iter->sequence_number);
            DL_DELETE(*destlist, el);
            free(el);
        }
    }

    return msg;
}

dessert_per_result_t aodv_periodic_scexecute(void* data, struct timeval* scheduled, struct timeval* interval) {
    uint8_t schedule_type;
    void* schedule_param = NULL;
    mac_addr ether_addr;
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    if(aodv_db_popschedule(&timestamp, ether_addr, &schedule_type, &schedule_param) == false) {
        //nothing to do come back later
        return DESSERT_PER_KEEP;
    }

    switch(schedule_type) {
        case AODV_SC_REPEAT_RREQ: {
            aodv_send_rreq_repeat(&timestamp, (aodv_rreq_series_t*)schedule_param);
            break;
        }
        case AODV_SC_SEND_OUT_RERR: {
            uint32_t rerr_count;
            aodv_db_getrerrcount(&timestamp, &rerr_count);

            if(rerr_count >= RERR_RATELIMIT) {
                return DESSERT_PER_KEEP;
            }

            if(!aodv_db_inv_over_nexthop(ether_addr)) {
                return 0; //nexthop not in nht
            }

            aodv_link_break_element_t* destlist = NULL;

            if(!aodv_db_get_destlist(ether_addr, &destlist)) {
                return 0; //nexthop not in nht
            }

            while(true) {
                dessert_msg_t* rerr_msg = aodv_create_rerr(&destlist);

                if(!rerr_msg) {
                    break;
                }

                dessert_meshsend(rerr_msg, NULL);
                dessert_msg_destroy(rerr_msg);
                aodv_db_putrerr(&timestamp);
            }

            break;
        }
        case AODV_SC_SEND_OUT_RWARN: {
            aodv_link_break_element_t* head = NULL;
            aodv_db_get_warn_endpoints_from_neighbor_and_set_warn(ether_addr, &head);

            aodv_link_break_element_t* dest, *tmp;
            DL_FOREACH_SAFE(head, dest, tmp) {
                dessert_debug("AODV_SC_SEND_OUT_RWARN: " MAC " -> " MAC,
                              EXPLODE_ARRAY6(ether_addr),
                              EXPLODE_ARRAY6(dest->host));
                aodv_send_rreq(dest->host, &timestamp);
            }
            break;
        }
#ifndef ANDROID
        case AODV_SC_UPDATE_RSSI: {
            dessert_meshif_t* iface = (dessert_meshif_t*)(schedule_param);
            int8_t diff = aodv_db_update_rssi(ether_addr, iface, &timestamp);

            if(diff > signal_strength_threshold) {
                //walking away -> we need to send a new warn
                dessert_debug("%s <= W => " MAC, iface->if_name, EXPLODE_ARRAY6(ether_addr));
                aodv_db_addschedule(&timestamp, ether_addr, AODV_SC_SEND_OUT_RWARN, 0);
            }

            break;
        }
#endif
        default: {
            dessert_crit("unknown schedule type=%" PRIu8 "", schedule_type);
        }
    }

    return DESSERT_PER_KEEP;
}
