/******************************************************************************
 Copyright 2009, Bastian Blywis, Sebastian Hofmann, Freie Universitaet Berlin
 (FUB).
 All rights reserved.

 These sources were originally developed by Bastian Blywis
 at Freie Universitaet Berlin (http://www.fu-berlin.de/),
 Computer Systems and Telematics / Distributed, Embedded Systems (DES) group
 (http://cst.mi.fu-berlin.de/, http://www.des-testbed.net/)
 ------------------------------------------------------------------------------
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program. If not, see http://www.gnu.org/licenses/ .
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
        http://www.des-testbed.net/
*******************************************************************************/

#include "gossiping.h"

uint8_t init_seq = 5;
uint16_t seq = 0;

// parameters
double p  = 0.6;
double p2 = 0;
uint8_t k = 0;
uint8_t n = 0;
uint8_t m = 0;
struct timeval timeout = {0, 100};
uint8_t gossip = 0;
uint8_t helloTTL = 0;
struct timeval hello_interval = {2, 0};
struct timeval cleanup_interval = {30, 0};
double p_min = 0.4;
double p_max = 0.9;
uint16_t T_MAX_ms = 30;
uint8_t forwarder = 0;

pthread_mutex_t seq_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Add a sequence number to a dessert msg
 *
 * This function requires not mutex or rwlock for the
 * seq counter as there is only one sysif thread
 */
void addSeq(dessert_msg_t* msg) {
    pthread_mutex_lock(&seq_mutex);
    if(seq == 0xFFFF) {
        seq = 0;
    }
    msg->u16 = seq++;
    msg->u8 |= USE_SEQ;
    if(init_seq) {
        msg->u8 |= SYN_SEQ;
        init_seq--;
    }
    pthread_mutex_unlock(&seq_mutex);
}

/**
 * Forwards all packets from the sysif over all meshifs
 */
dessert_cb_result sendToNetwork(dessert_msg_t *msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id) {
    msg->u8 = 0x00;

    if(activated & USE_KHOPS) {
        msg->ttl = k;
        msg->u8 |= USE_K;
    }
    addSeq(msg);

    // add extension to count the number of hops
    dessert_ext_t* ext;
    dessert_msg_addext(msg, &ext, EXT_HOPS, sizeof(gossip_ext_hops_t));
    ((gossip_ext_hops_t*) ext->data)->hops = 1;
#ifndef ANDROID
    if(gossip == gossip_13) {
        gossip13_seq2((gossip_ext_hops_t*) ext->data);
    }
#endif
    /* gossip9 requires 2-hop neighborhood information to determine the
     * additional coverage that can be achieved
     *
     * gossip13 can piggyback information about tx and rx packets
     */
    if(gossip == gossip_9) {
        attachNodeList(msg);
    }
#ifndef ANDROID
    else if(gossip == gossip_13 && gossip13_piggyback) {
        gossip13_add_ext(msg);
    }
#endif
    dessert_meshsend(msg, NULL);
    logTX(msg, NULL);

    if(gossip == gossip_10) {
        storeForwardedPacket(msg, 1);
    }
#ifndef ANDROID
    else if(gossip == gossip_13) {
        gossip13_schedule_retransmission(msg, false);
    }
#endif

    return DESSERT_MSG_KEEP;
}

dessert_cb_result drop_zero_mac(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;
    if(shost[0] == 0x00 && shost[1] == 0x00 && shost[2] == 0x00 && shost[3] == 0x00 && shost[4] == 0x00 && shost[5] == 0x00) {
        dessert_warn("dropping frame with layer 2.5 zero MAC source");
        return DESSERT_MSG_DROP;
    }
    return DESSERT_MSG_KEEP;
}

/**
 * Deliver packets for this host via the sys interface (includes unicast, broadcast, and multicast packets)
 **/
dessert_cb_result deliver(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(proc->lflags & DESSERT_RX_FLAG_L25_DST
    || proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
    || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
        u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;
        dessert_debug("received message, src=" MAC ", seq=%d, flags=%#x", EXPLODE_ARRAY6(shost), msg->u16, msg->u8);
        struct ether_header *eth;
        size_t eth_len;
        eth_len = dessert_msg_ethdecap(msg, &eth);
        dessert_syssend(eth, eth_len);
        free(eth);
    }

    // No need to forward packets destined to me
    if(proc->lflags & DESSERT_RX_FLAG_L25_DST) {
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

/**
 * Forwards packets on the first k-hops and prevents the handling by the futher
 * gossip functions.
 *
 * The remaining number of k-hops is stored in the ttl field
 */
dessert_cb_result floodOnFirstKHops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(activated & USE_KHOPS
    && msg->u8 & USE_K
    && msg->ttl > 0) {
        msg->ttl--;
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        dessert_meshsend(msg, iface);

        return DESSERT_MSG_DROP;
    }
    return DESSERT_MSG_KEEP;
}

/**
 * forwarding probability p only; stores packet for gossip3, gossip5, and gossip6
 */
dessert_cb_result gossip0(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(random() < (((long double) p)*((long double) RAND_MAX))) {
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        dessert_meshsend(msg, iface);
    }
    else if(gossip == gossip_3 || gossip == gossip_5 || gossip == gossip_6) {
        storeDroppedPacket(msg);
    }
    return DESSERT_MSG_DROP;
}

/**
 * forwardings with probability p or p2 depending on the node degree and
 * threshold n
 */
dessert_cb_result gossip2(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    double probability = p;

    if(numNeighbors() < n) {
        probability = p2;
        dessert_info("too few neighbors=%n, using p2", numNeighbors());
    }
    else if(random() < (((long double) probability)*((long double) RAND_MAX))) {
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        dessert_meshsend(msg, iface);
    }
    return DESSERT_MSG_DROP;
}

/**
 * gossip0 forwarding until packet is only helloTTL hops away from destination and
 * then unicast forwarding
 * TODO: unicast forwarding not implemented
 */
dessert_cb_result gossip4(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(isInZone(msg)) {
        dessert_warn("packet reached zone: dropping because unicast not implemented");
        /* TODO: query zone database, retrieve path
        create extension, fill with path
        dessert_msg_send_raw(msg, NULL);*/
    }
    else {
        if(random() < (((long double) p)*((long double) RAND_MAX))) {
            dessert_meshif_t* iface = NULL;
            logForwarded(msg, 0, NULL, iface);
            dessert_meshsend(msg, iface);
        }
    }
    return DESSERT_MSG_DROP;
}

/**
 * store packets until random timout and count duplicates
 */
dessert_cb_result gossip7(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    // always save packet and forward later if receiving < m duplicates until timeout
    storeDroppedPacket(msg);
    return DESSERT_MSG_DROP;
}

/**
 * flooding when no known neighbors (should not happen) and else
 * calculate bounded forwarding probability p based on node degree
 */
dessert_cb_result gossip8(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    // flood if no known neighbors (should not happen)
    if(numNeighbors() == 0) {
        dessert_warn("no known neighbors, flooding packet");
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        logadaptivep(msg, 0, NULL, iface, 1.0);
        dessert_meshsend(msg, iface);
    }
    else {
        // TODO review this formula
        float Sn = p_max * ((1-pow(p_max,numNeighbors()))/(1-p_max));
        float Sn1 = p_max * ((1-pow(p_max,(numNeighbors()-1)))/(1-p_max));
        double probability = Sn - Sn1;

        probability = max(probability, p_min);
        dessert_debug("calculating p, using p=%f with %d neighbors", probability, numNeighbors());
        if(random() < (((long double) probability)*((long double) RAND_MAX))) {
            dessert_meshif_t* iface = NULL;
            logForwarded(msg, 0, NULL, iface);
            logadaptivep(msg, 0, NULL, iface, probability);
            dessert_meshsend(msg, iface);
        }
    }
    return DESSERT_MSG_DROP;
}

/**
 * flooding when low node degree and else consider covered number of neighbors
 * resp. additional coverage that can be achieved
 */
dessert_cb_result gossip9(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    uint8_t neighbors = numNeighbors();

    if(neighbors < n) {
        dessert_debug("too few neighbors, flooding packet");
        attachNodeList(msg);
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        logadaptivep(msg, 0, NULL, iface, 1.0);
        dessert_meshsend(msg, iface);
    }
    else {
        uint8_t nc = coveredNeighbors(msg, iface);
        long double express = (max(neighbors - nc, 0) / (double) neighbors);
        long double probability = 1 - exp(-(express));
        dessert_debug("covered_neighbors=%d, neighbors=%d, probability=%f", nc , neighbors, probability);

        if(random() < (((long double) probability)*((long double) RAND_MAX))) {
            attachNodeList(msg);
            dessert_meshif_t* iface = NULL;
            logForwarded(msg, 0, NULL, iface);
            logadaptivep(msg, 0, NULL, iface, probability);
            dessert_meshsend(msg, iface);
        }
    }
    return DESSERT_MSG_DROP;
}

/**
 * like gossip0 but overhear if sent packets are forwarded by a particular
 * number of neighbors or if the packet was lost during transmission
 */
dessert_cb_result gossip10(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(random() < (((long double) p)*((long double) RAND_MAX))) {
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        dessert_meshsend(msg, iface);
        storeForwardedPacket(msg, 0);
    }
    else {
        storeDroppedPacket(msg);
    }
    return DESSERT_MSG_DROP;
}

/**
 * Forwards packets like gossip0, but additionally checks if the current node is
 * a mpr node, i.e. if its allowed to forwards packets.
  */
dessert_cb_result gossip11(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(random() < (((long double) p)*((long double) RAND_MAX))) {
        if(is_mpr(msg)) {
            dessert_meshif_t* iface = NULL;
            logForwarded(msg, 0, NULL, iface);
            dessert_meshsend(msg, iface);
        }
    }
    return DESSERT_MSG_DROP;
}

/**
 * MCDS Mode
 *
 * Forward packets if node is forwarder resp. a MCDS node and drop packets otherwise.
 */
dessert_cb_result gossip12(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(forwarder) {
        dessert_meshif_t* iface = NULL;
        logForwarded(msg, 0, NULL, iface);
        dessert_meshsend(msg, iface);
    }
    return DESSERT_MSG_DROP;
}

dessert_cb_result gossip14(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    storeDroppedPacket(msg);
    return DESSERT_MSG_DROP;
}

/** Forward packet depending on the gossip variant
 */
dessert_cb_result forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    switch(gossip) {
        case gossip_1:
            return gossip0(msg, len, proc, iface, id);
        case gossip_2:
            return gossip2(msg, len, proc, iface, id);
        case gossip_3:
            return gossip0(msg, len, proc, iface, id);
        case gossip_4:
            return gossip4(msg, len, proc, iface, id);
        case gossip_5:
            return gossip0(msg, len, proc, iface, id);
        case gossip_6:
            return gossip0(msg, len, proc, iface, id);
        case gossip_7:
            return gossip7(msg, len, proc, iface, id);
        case gossip_8:
            return gossip8(msg, len, proc, iface, id);
        case gossip_9:
            return gossip9(msg, len, proc, iface, id);
        case gossip_10: // TODO complete implemention
            return gossip10(msg, len, proc, iface, id);
        case gossip_11: // NHDP+MPR
            return gossip11(msg, len, proc, iface, id);
        case gossip_12: // MCDS mode
            return gossip12(msg, len, proc, iface, id);
#ifndef ANDROID
        case gossip_13: // Blywis-Reinecke mode
            // forwarding probability usually p=1.0
            return gossip0(msg, len, proc, iface, id);
#endif
        case gossip_14: // like gossip3 but no p
            return gossip14(msg, len, proc, iface, id);
        case gossip_0:
        default:
            return gossip0(msg, len, proc, iface, id);
    }
    return DESSERT_MSG_DROP;
}
