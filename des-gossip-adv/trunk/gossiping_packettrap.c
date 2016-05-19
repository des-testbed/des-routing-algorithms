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

typedef struct {
    u_char addr[ETHER_ADDR_LEN]; ///< source address
    uint16_t seq;                ///< sequence number
} pkey_t;

typedef struct _trappedpacket {
    dessert_msg_t* msg;
    pkey_t key;
    uint8_t counter;            ///< number of received duplicates during waiting
    uint8_t counter2;           ///< number of received duplicates after forwarding
    uint8_t forwarded;
    UT_hash_handle hh;
} trappedpacket_t;

trappedpacket_t* trappedpackets = NULL;
pthread_rwlock_t packettrap_lock = PTHREAD_RWLOCK_INITIALIZER;

seqlog_t* seqlog = NULL;
pthread_rwlock_t seqlog_lock = PTHREAD_RWLOCK_INITIALIZER;

/** Check if sequence number is in the list of received packets
 *
 * Note: You should hold a read lock for the seqlog_t entry!
 */
inline uint8_t isDuplicate(seqlog_t* s, uint16_t seq);
uint8_t isDuplicate(seqlog_t* s, uint16_t seq) {
    uint16_t i;
    for(i=0; i < LOG_SIZE; i++) {
        if(s->seqs[i] == seq) {
            return 1;
        }
    }
    return 0;
}

/** Insert sequence number in the list of received packets
 *
 * Note: You should hold a lock for the seqlog_t entry!
 */
inline void insertSeq(seqlog_t* s, uint16_t seq);
void insertSeq(seqlog_t* s, uint16_t seq) {
    s->seqs[s->next_seq] = seq;
    s->next_seq++;
    if(s->next_seq >= LOG_SIZE) {
        s->next_seq = 0;
    }
}

/** Check if this packet is a duplicate
 *
 * Registers the sequence number as received if it is the first copy of the packet.
 * All subsequent received copies (duplicates) are dropped and the packet count is
 * increased, if the first copy is still in the packet trap.
 *
 * Looping packets sent by this host are also dropped.
 * The hop count value in the corresponding extension is incremented.
 */
int checkSeq(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(/*proc->lflags & DESSERT_RX_FLAG_L2_SRC
    ||*/ proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
        // TODO gossip_10
        dessert_debug("dropped looping packet from myself");
        return DESSERT_MSG_DROP;
    }

    seqlog_t* s = NULL;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    HASH_FIND(hh, seqlog, shost, ETHER_ADDR_LEN, s);
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/

    bool seq2_duplicate = false;
    if(s) {
        /*###*/
        pthread_rwlock_wrlock(&seqlog_lock);
        uint8_t dup = isDuplicate(s, msg->u16);
        if(!dup) {
            insertSeq(s, msg->u16);
#ifndef ANDROID
            seq2_duplicate = gossip13_rx_packet(s, msg, false);
#endif
            pthread_rwlock_unlock(&seqlog_lock);
            /*###*/
        }
        else {
            pthread_rwlock_unlock(&seqlog_lock);
            /*###*/
            if(!(msg->u8 & HELLO)) { // HELLOs are never trapped
                trappedpacket_t* t;
                pkey_t lookup_key;
                memcpy(lookup_key.addr, shost, ETHER_ADDR_LEN);
                lookup_key.seq = msg->u16;

                /*###*/
                pthread_rwlock_wrlock(&packettrap_lock);
                HASH_FIND(hh, trappedpackets, &lookup_key, sizeof(pkey_t), t);
                if(t) {
                    // TODO gossip_10
                    t->counter++;
                }
                pthread_rwlock_unlock(&packettrap_lock);
                /*###*/
            }
            return DESSERT_MSG_DROP;
        }
    }
    else {
        s = malloc(sizeof(seqlog_t));
        if(!s) {
            dessert_crit("could not allocate memory");
            return DESSERT_MSG_KEEP;
        }
        uint16_t i = 0;
        for(i=0; i<LOG_SIZE; i++) {
            s->seqs[i] = msg->u16;
        }
        s->next_seq = 1;
#ifndef ANDROID
        // what the other node tells me
        gossip13_new_history(&(s->rx_history), false); // does not require observation initialization
        gossip13_new_history(&(s->tx_history), false); // does not require observation initialization
        gossip13_new_history(&(s->total_tx_history), false); // does not require observation initialization
        gossip13_new_history_mean(&(s->hops_history), false); // does not require observation initialization
        gossip13_new_history(&(s->nodes_history), false); // does not require observation initialization
        gossip13_new_history_mean(&(s->mean_dist_history), false); // does not require observation initialization
        gossip13_new_history_mean(&(s->eccentricity_history), false); // does not require observation initialization

        // what I know about the other node
        gossip13_new_history(&(s->observed_history), true);
        gossip13_new_observation(&(s->observed_history->observations));
        gossip13_new_history_mean(&(s->my_hops_history), true);
        gossip13_new_observation_mean(&(s->my_hops_history->observations));
#endif

        s->tau = 0;
        memcpy(s->addr, shost, ETHER_ADDR_LEN);
        dessert_debug("registering seq=%d for src=" MAC " (previously unknown host)", msg->u16, EXPLODE_ARRAY6(shost));
#ifndef ANDROID
        seq2_duplicate = gossip13_rx_packet(s, msg, true);
#endif

        /*###*/
        pthread_rwlock_wrlock(&seqlog_lock);
        HASH_ADD(hh, seqlog, addr, ETHER_ADDR_LEN, s);
        pthread_rwlock_unlock(&seqlog_lock);
        /*###*/
    }
#ifndef ANDROID
    if(seq2_duplicate && gossip13_drop_seq2_duplicates
        && (proc->lflags & DESSERT_RX_FLAG_L25_DST || proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST)) {
        return DESSERT_MSG_DROP;
    }
#endif

    dessert_ext_t* ext = NULL;
    if(dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
        gossip_ext_hops_t* h = (gossip_ext_hops_t*) ext->data;
        h->hops++;
    }
    return DESSERT_MSG_KEEP;
}

/** Frees a trapped packet
 */
inline void destroyTrappedPacket(trappedpacket_t* t);
void destroyTrappedPacket(trappedpacket_t* t) {
    dessert_msg_destroy(t->msg);
    free(t);
}

/** Get number of receveived copies
 *
 * Returns the number of received copies of a packet that is currently
 * stored in the packet trap.
 *
 * @return number of received copies
 */
uint8_t packetStored(dessert_msg_t* msg) {
    trappedpacket_t* t;
    pkey_t lookup_key;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    memcpy(lookup_key.addr, shost, ETHER_ADDR_LEN);
    lookup_key.seq = msg->u16;

    /*###*/
    pthread_rwlock_rdlock(&packettrap_lock);
    HASH_FIND(hh, trappedpackets, &lookup_key, sizeof(pkey_t), t);
    uint8_t c = 0;
    if(t) {
        c = t->counter;
    }
    pthread_rwlock_unlock(&packettrap_lock);
    /*###*/

    return c;
}

/** Handles a trapped packet after timeout
 *
 */
dessert_per_result_t handleTrappedPacket(void *data, struct timeval *scheduled, struct timeval *interval) {
    trappedpacket_t* t = (trappedpacket_t*) data;
    uint8_t forwarded = 0;
    switch(gossip) {
        // forward if less than m duplicates received
        case gossip_14:
        case gossip_3:
            if((t->counter)-1 < m) {
                dessert_debug("forwarding msg: src=" MAC ", seq=%d, received %d<%d duplicates",
                              EXPLODE_ARRAY6(t->key.addr), t->msg->u16, t->counter-1, m);
                t->msg->u8 |= DELAYED;
                dessert_meshif_t* iface = NULL;
                logForwarded(t->msg, 0, NULL, iface);
                dessert_meshsend(t->msg, iface);
                forwarded = 1;
            }
            break;
        // send if not all neighbors sent a copy
        case gossip_5:
            if((t->counter) < numNeighbors()) {
                dessert_debug("forwarding msg: src=" MAC ", seq=%d, received %d<%d (=neighbors) duplicates",
                              EXPLODE_ARRAY6(t->key.addr), t->msg->u16, t->counter, numNeighbors());
                t->msg->u8 |= DELAYED;
                dessert_meshif_t* iface = NULL;
                logForwarded(t->msg, 0, NULL, iface);
                dessert_meshsend(t->msg, iface);
                forwarded = 1;
            }
            break;
        // forward if less than m duplicates received with adapted probability
        case gossip_6:
            if((t->counter)-1 < m) {
                float  new_p = (p/(m + 1));
                if(random() < (((long double) new_p)*((long double) RAND_MAX))) {
                    dessert_debug("forwarding msg: src=" MAC ", seq=%d, received %d<%d duplicates and send with probability %f",
                                  EXPLODE_ARRAY6(t->key.addr), t->msg->u16, t->counter-1, m, new_p);
                    t->msg->u8 |= DELAYED;
                    dessert_meshif_t* iface = NULL;
                    logForwarded(t->msg, 0, NULL, iface);
                    dessert_meshsend(t->msg, iface);
                    forwarded = 1;
                }
            }
            break;
        // forward if less than m-1 duplicates received
        case gossip_7:
            if((t->counter-1) <= m) {
                if(random() < (((long double) p)*((long double) RAND_MAX))) {
                    dessert_debug("forwarding msg: src=" MAC ", seq=%d, received %d<=%d duplicates", EXPLODE_ARRAY6(t->key.addr), t->msg->u16, t->counter-1, m);
                    t->msg->u8 |= DELAYED;
                    dessert_meshif_t* iface = NULL;
                    logForwarded(t->msg, 0, NULL, iface);
                    dessert_meshsend(t->msg, iface);
                    forwarded = 1;
                }
            }
            break;
        case gossip_10:
            if(t->forwarded) {
                if((max(0, t->counter) + t->counter2) < m) {
                    t->msg->u8 |= DELAYED;
                    dessert_meshif_t* iface = NULL;
                    logForwarded(t->msg, 0, NULL, iface);
                    dessert_meshsend(t->msg, iface);
                    forwarded = 1;
                }
                // always drop packet after 2nd forwarding chance
            }
            else {
                // like gossip_3
                if(max(0, t->counter - 1) < m) {
                    t->msg->u8 |= DELAYED;
                    dessert_meshif_t* iface = NULL;
                    logForwarded(t->msg, 0, NULL, iface);
                    dessert_meshsend(t->msg, iface);
                    t->forwarded = 1;
                    uint32_t sec = timeout.tv_sec;
                    uint32_t usec = timeout.tv_usec;
                    struct timeval handle_interval;
                    gettimeofday(&handle_interval, NULL);
                    TIMEVAL_ADD(&handle_interval, sec, usec);
                    dessert_periodic_add((dessert_periodiccallback_t *) handleTrappedPacket, t, &handle_interval, NULL);
                    dessert_debug("keeping packet to monitor forwarding of neighbors: src=" MAC ", seq=%d", EXPLODE_ARRAY6(t->key.addr), t->key.seq);
                    return 0; // do not delete msg and give it a second chance
                }
            }
            break;
        case gossip_11: // TODO: remove/replace
            if((t->counter)-1 < m) {
                float new_p = (p+(((1-p)*p)/(m + 1)));
                if(random() < (((long double) new_p)*((long double) RAND_MAX))) {
                    dessert_debug("forwarding msg: src=" MAC ", seq=%d, received %d<%d duplicates, p_new=%f",
                                  EXPLODE_ARRAY6(t->key.addr), t->msg->u16, t->counter-1, m, new_p);
                    t->msg->u8 |= DELAYED;
                    dessert_meshif_t* iface = NULL;
                    logForwarded(t->msg, 0, NULL, iface);
                    dessert_meshsend(t->msg, iface);
                    forwarded = 1;
                }
            }
            break;
        default:
            dessert_warn("unsupported gossip variant");
    }

    if(!forwarded) {
        dessert_debug("packet not forwarded, dropping it now: src=" MAC ", seq=%d", EXPLODE_ARRAY6(t->key.addr), t->key.seq);
    }

    /*###*/
    pthread_rwlock_wrlock(&packettrap_lock);
    HASH_DEL(trappedpackets, t);
    pthread_rwlock_unlock(&packettrap_lock);
    /*###*/

    destroyTrappedPacket(t);

    return 0;
}

void storeDroppedPacket(dessert_msg_t* msg) {
    trappedpacket_t* t;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    pkey_t lookup_key;
    memcpy(lookup_key.addr, shost, ETHER_ADDR_LEN);
    lookup_key.seq = msg->u16;

    /*###*/
    pthread_rwlock_wrlock(&packettrap_lock);
    HASH_FIND(hh, trappedpackets, &lookup_key, sizeof(pkey_t), t);
    if(t) {
        // TODO: gossip_10
        t->counter++;
    }
    pthread_rwlock_unlock(&packettrap_lock);
    /*###*/

    if(!t) {
        trappedpacket_t* t = malloc(sizeof(trappedpacket_t));
        if(!t) {
            dessert_crit("could not allocate memory");
            return;
        }

        dessert_msg_t* clone = NULL;
        dessert_msg_clone(&clone, msg, false);
        t->msg = clone;

        memcpy(&(t->key), &lookup_key, sizeof(pkey_t));
        t->counter = 1;
        t->counter2 = 0;
        t->forwarded = 0;

        /*###*/
        pthread_rwlock_wrlock(&packettrap_lock);
        HASH_ADD(hh, trappedpackets, key, sizeof(pkey_t), t);
        pthread_rwlock_unlock(&packettrap_lock);
        /*###*/

        uint32_t sec = timeout.tv_sec;
        uint32_t usec = timeout.tv_usec;
        if(gossip == gossip_7) {
            sec = 0;
            usec = 1000 * (random() % T_MAX_ms);
            while(usec >= 1000000) {
                sec += 1;
                usec -= 1000000;
            }
            dessert_debug("gossip7 random timeout: %ld s, %ld us", sec, usec);
        }
        struct timeval handle_interval;
        gettimeofday(&handle_interval, NULL);
        TIMEVAL_ADD(&handle_interval, sec, usec);
        dessert_periodic_add((dessert_periodiccallback_t *) handleTrappedPacket, t, &handle_interval, NULL);
    }
}

void storeForwardedPacket(dessert_msg_t* msg, uint8_t source) {
    trappedpacket_t* t;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    pkey_t lookup_key;
    memcpy(lookup_key.addr, shost, ETHER_ADDR_LEN);
    lookup_key.seq = msg->u16;

    /*###*/
    pthread_rwlock_wrlock(&packettrap_lock);
    HASH_FIND(hh, trappedpackets, &lookup_key, sizeof(pkey_t), t);
    if(t) {
        t->counter++;
    }
    pthread_rwlock_unlock(&packettrap_lock);
    /*###*/

    if(!t) {
        trappedpacket_t* t = malloc(sizeof(trappedpacket_t));
        if(!t) {
            dessert_crit("could not allocate memory");
            return;
        }

        dessert_msg_t* clone = NULL;
        dessert_msg_clone(&clone, msg, false);
        t->msg = clone;

        memcpy(&(t->key), &lookup_key, sizeof(pkey_t));
        if(source) {
            t->counter = 0;
        }
        else {
            t->counter = 1;
        }
        t->counter2 = 0;
        t->forwarded = 1;

        /*###*/
        pthread_rwlock_wrlock(&packettrap_lock);
        HASH_ADD(hh, trappedpackets, key, sizeof(pkey_t), t);
        pthread_rwlock_unlock(&packettrap_lock);
        /*###*/

        uint32_t sec = timeout.tv_sec;
        uint32_t usec = timeout.tv_usec;
        struct timeval handle_interval;
        gettimeofday(&handle_interval, NULL);
        TIMEVAL_ADD(&handle_interval, sec, usec);
        dessert_periodic_add((dessert_periodiccallback_t *) handleTrappedPacket, t, &handle_interval, NULL);
    }
}

void resetPacketTrap() {
    trappedpacket_t* current;

    pthread_rwlock_wrlock(&packettrap_lock);
    while(trappedpackets) {
        current = trappedpackets;
        HASH_DEL(trappedpackets, current);
        destroyTrappedPacket(current);
    }
    pthread_rwlock_unlock(&packettrap_lock);

    dessert_notice("packet trap flushed");
}

void resetSeqLog() {
    seqlog_t* current;

    pthread_rwlock_wrlock(&seqlog_lock);
    while(seqlog) {
        current = seqlog;
        HASH_DEL(seqlog, current);
        free(current);
    }
    pthread_rwlock_unlock(&seqlog_lock);

    dessert_notice("sequence number log flushed");
}

/******************************************************************************/


