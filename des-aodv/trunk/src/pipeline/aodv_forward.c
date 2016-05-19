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

#include <pthread.h>
#include <string.h>
#include <utlist.h>
#include "../database/aodv_database.h"
#include "aodv_pipeline.h"
#include "../config.h"
#include "../helper.h"

uint16_t data_seq_global = 0;
pthread_rwlock_t data_seq_lock = PTHREAD_RWLOCK_INITIALIZER;

void aodv_send_packets_from_buffer(mac_addr ether_dhost, mac_addr next_hop, dessert_meshif_t* iface) {
    // drop RREQ schedule, since we already know the route to destination
    aodv_pipeline_delete_series_ether(ether_dhost);

    dessert_debug("new route to " MAC " over " MAC " found -> send out packet from buffer", EXPLODE_ARRAY6(ether_dhost), EXPLODE_ARRAY6(next_hop));

    // send out packets from buffer
    dessert_msg_t* buffered_msg;

    while((buffered_msg = aodv_db_pop_packet(ether_dhost)) != NULL) {
        struct ether_header* l25h = dessert_msg_getl25ether(buffered_msg);
        uint16_t data_seq_copy = 0;
        pthread_rwlock_wrlock(&data_seq_lock);
        data_seq_copy = ++data_seq_global;
        pthread_rwlock_unlock(&data_seq_lock);
        buffered_msg->u16 = data_seq_copy;

        /*  no need to search for next hop. Next hop is the last_hop that send RREP */
        mac_copy(buffered_msg->l2h.ether_dhost, next_hop);
        dessert_meshsend(buffered_msg, iface);

        dessert_trace("data packet - id=%" PRIu16 " - to mesh - to " MAC " route is known - send over " MAC, data_seq_copy, EXPLODE_ARRAY6(l25h->ether_dhost), EXPLODE_ARRAY6(next_hop));

        dessert_msg_destroy(buffered_msg);
    }
}

int aodv_forward_broadcast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    if(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST) {
        struct ether_header* l25h = dessert_msg_getl25ether(msg);

        if(msg->ttl <= 0) {
            dessert_trace("got data from " MAC " but TTL is <= 0", EXPLODE_ARRAY6(l25h->ether_dhost));
            return DESSERT_MSG_DROP;
        }

        msg->ttl--;
        msg->u8++; /*hop count */

        struct timeval timestamp;

        gettimeofday(&timestamp, NULL);

        if(false == aodv_db_capt_data_seq(l25h->ether_shost, msg->u16, msg->u8, &timestamp)) {
            dessert_trace("data packet is known -> DUP");
            return DESSERT_MSG_DROP;
        }

        dessert_trace("got BROADCAST from " MAC " over " MAC, EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost));
        dessert_meshsend(msg, NULL); //forward to mesh
        dessert_syssend_msg(msg); //forward to sys
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

int aodv_forward_multicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    if(proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
        // dessert_meshsend(msg, NULL); //forward to mesh
        // dessert_syssend_msg(msg); //forward to sys
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

int aodv_forward(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    int a = (proc->lflags & DESSERT_RX_FLAG_L2_DST);
    int b = (proc->lflags & DESSERT_RX_FLAG_L25_DST);

    if(!a || b) {
        return DESSERT_MSG_KEEP;
    }

    struct timeval timestamp;

    gettimeofday(&timestamp, NULL);

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    if(msg->ttl <= 0) {
        dessert_trace("got data from " MAC " but TTL is <= 0", EXPLODE_ARRAY6(l25h->ether_dhost));
        return DESSERT_MSG_DROP;
    }

    msg->ttl--;
    msg->u8++; /*hop count */

    if(false == aodv_db_capt_data_seq(l25h->ether_shost, msg->u16, msg->u8, &timestamp)) {
        dessert_trace("data packet is known -> DUP");
        return DESSERT_MSG_DROP;
    }

    dessert_meshif_t* output_iface;
    mac_addr next_hop;

    if(aodv_db_getroute2dest(l25h->ether_dhost, next_hop, &output_iface, &timestamp, AODV_FLAGS_UNUSED)) {
        mac_copy(msg->l2h.ether_dhost, next_hop);

        dessert_meshsend(msg, output_iface);
        dessert_trace(MAC " over " MAC " ----ME----> " MAC " to " MAC,
                      EXPLODE_ARRAY6(l25h->ether_shost),
                      EXPLODE_ARRAY6(msg->l2h.ether_shost),
                      EXPLODE_ARRAY6(msg->l2h.ether_dhost),
                      EXPLODE_ARRAY6(l25h->ether_dhost));
    }
    else {
        uint32_t rerr_count;
        aodv_db_getrerrcount(&timestamp, &rerr_count);

        if(rerr_count >= RERR_RATELIMIT) {
            return DESSERT_MSG_DROP;
        }

        // route unknown -> send rerr towards source
        aodv_link_break_element_t* head = NULL;
        aodv_link_break_element_t* entry = malloc(sizeof(aodv_link_break_element_t));
        memset(entry, 0x0, sizeof(aodv_link_break_element_t));
        mac_copy(entry->host, l25h->ether_dhost);
        entry->sequence_number = UINT32_MAX;
        DL_APPEND(head, entry);
        dessert_msg_t* rerr_msg = aodv_create_rerr(&head);

        if(rerr_msg != NULL) {
            dessert_meshsend(rerr_msg, NULL);
            dessert_msg_destroy(rerr_msg);
            aodv_db_putrerr(&timestamp);
        }

        dessert_trace(MAC " over " MAC " ----XXX----> " MAC " to " MAC,
                      EXPLODE_ARRAY6(l25h->ether_shost),
                      EXPLODE_ARRAY6(msg->l2h.ether_shost),
                      EXPLODE_ARRAY6(msg->l2h.ether_dhost),
                      EXPLODE_ARRAY6(l25h->ether_dhost));
    }

    return DESSERT_MSG_DROP;
}

// --------------------------- TUN ----------------------------------------------------------
int aodv_sys_drop_multicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_sysif_t* sysif, dessert_frameid_t id) {
    /* check if we have an processing header */
    if(proc == NULL) {
        return DESSERT_MSG_NEEDMSGPROC;
    }

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    if(mac_equal(l25h->ether_dhost, ether_broadcast)) {
        proc->lflags |= DESSERT_RX_FLAG_L25_BROADCAST;
    }
    else if(l25h->ether_dhost[0] & 0x01) {    /* broadcast also has this bit set */
        proc->lflags |= DESSERT_RX_FLAG_L25_MULTICAST;
    }

    if(proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
        dessert_debug("dropped Multicast packet in Ethernet frame");
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

int aodv_sys2rp(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_sysif_t* sysif, dessert_frameid_t id) {
    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    msg->ttl = UINT8_MAX;
    msg->u8 = 0; /*hop count */

    if(mac_equal(l25h->ether_dhost, ether_broadcast)) {
        pthread_rwlock_wrlock(&data_seq_lock);
        msg->u16 = ++data_seq_global;
        pthread_rwlock_unlock(&data_seq_lock);

        dessert_meshsend(msg, NULL);
    }
    else {
        mac_addr dhost_next_hop;
        dessert_meshif_t* output_iface;
        struct timeval ts;
        gettimeofday(&ts, NULL);
        int a = aodv_db_getroute2dest(l25h->ether_dhost, dhost_next_hop, &output_iface, &ts, AODV_FLAGS_ROUTE_LOCAL_USED);

        if(a == true) {
            pthread_rwlock_wrlock(&data_seq_lock);
            msg->u16 = ++data_seq_global;
            pthread_rwlock_unlock(&data_seq_lock);

            mac_copy(msg->l2h.ether_dhost, dhost_next_hop);
            dessert_meshsend(msg, output_iface);

            dessert_trace("send data packet to mesh - to " MAC " over " MAC " id=%" PRIu16 " route is known", EXPLODE_ARRAY6(l25h->ether_dhost), EXPLODE_ARRAY6(dhost_next_hop), msg->u16);
        }
        else {
            aodv_db_push_packet(l25h->ether_dhost, msg, &ts);
            aodv_send_rreq(l25h->ether_dhost, &ts); // create and send RREQ - without initial metric
            dessert_trace("try to send data packet to mesh - to " MAC ", but route is unknown -> push packet to FIFO and send RREQ", EXPLODE_ARRAY6(l25h->ether_dhost));
        }
    }

    return DESSERT_MSG_DROP;
}

// ----------------- common callbacks ---------------------------------------------------

/**
 * Forward packets addressed to me to tun pipeline
 */
int aodv_local_unicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    if(proc->lflags & DESSERT_RX_FLAG_L25_DST) {
        struct ether_header* l25h = dessert_msg_getl25ether(msg);

        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);

        if(msg->ttl <= 0) {
            dessert_trace("got data from " MAC " but TTL is <= 0", EXPLODE_ARRAY6(l25h->ether_dhost));
            return DESSERT_MSG_DROP;
        }

        msg->ttl--;
        msg->u8++; /*hop count */

        if(false == aodv_db_capt_data_seq(l25h->ether_shost, msg->u16, msg->u8, &timestamp)) {
            dessert_trace("data packet is known -> DUP");
            return DESSERT_MSG_DROP;
        }

        dessert_debug("got UNICAST from " MAC " over " MAC " hop_count=% " PRIu8 "", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), msg->u8);
        dessert_syssend_msg(msg);
    }

    return DESSERT_MSG_DROP;
}
