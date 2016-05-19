/******************************************************************************
 Copyright 2009, Bastian Blywis, Freie Universitaet Berlin (FUB).
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

#include "ara.h"

/** Key of ara_wait_ack_t
 *
 * A necessary information to uniquely identify a packet.
 */
typedef struct _key {
    ara_address_t src;              ///< source address
    ara_seq_t seq;                  ///< sequence number of the packet
    ara_address_t nexthop;          ///< nexthop we did forward the packet to
} ara_waitfor_key_t;

/**
 * Data of a packet waiting for acknowledgement
 */
typedef struct ara_waitfor_ack {
    ara_waitfor_key_t key;          ///< structure as key
    ara_address_t dst;              ///< destination address; required for rt update
    dessert_meshif_t* iface;        ///< output interface; required for rt update
    struct timeval t;               ///< time until we expect an acknowledgement
    UT_hash_handle hh;              ///< used by uthash
} ara_wait_ack_t;

ara_wait_ack_t* wait = NULL; ///< hash map for all packets expecting an acknowledgement
pthread_rwlock_t acklock = PTHREAD_RWLOCK_INITIALIZER; ///< acknowledgement lock

uint8_t ara_ack_mode = ARA_ACK_DISABLED;
uint16_t ara_ack_wait_ms = ARA_ACK_WAIT_MS;
uint32_t ara_ack_missed = 0;

/** Wait for an acknowledgement
 *
 * Wait for an acknowledgement of a particular packet.
 *
 * @param src address of the sender of the packet
 * @param seq sequence number of the packet
 * @param nexthop the node the packet was forwarded to
 * @return -1 on error else 0
 */
int8_t ara_ack_waitfor(ara_address_t src, ara_seq_t seq, ara_address_t nexthop, ara_address_t dst, dessert_meshif_t* iface) {
    if(ara_ack_mode == ARA_ACK_DISABLED) {
        dessert_debug("acknowledgements are disabled");
        return -1;
    }

    ara_wait_ack_t* pkg = malloc(sizeof(ara_wait_ack_t));

    if(pkg == NULL) {
        dessert_crit("could not allocate memory; continuing without storing data");
        return -1;
    }

    memset(pkg, 0x00, sizeof(ara_wait_ack_t));
    memcpy(pkg->key.src, src, sizeof(ara_address_t));
    pkg->key.seq = seq;
    memcpy(pkg->key.nexthop, nexthop, sizeof(ara_address_t));

    memcpy(pkg->dst, dst, sizeof(ara_address_t));
    pkg->iface = (dessert_meshif_t*) iface;

    gettimeofday(&(pkg->t), NULL);
    TIMEVAL_ADD(&(pkg->t), 0, ara_ack_wait_ms * 1000);

    pthread_rwlock_wrlock(&acklock);
//  HASH_ADD_KEY(hh, wait, key, sizeof(ara_waitfor_key_t), pkg);
    HASH_ADD_KEYPTR(hh, wait, &(pkg->key), sizeof(ara_waitfor_key_t), pkg);
    pthread_rwlock_unlock(&acklock);

    return 0;
}

/** Evaluate an overheard packet
 *
 * Evaluate an overheard packet. If we are waiting for the packet as
 * passive acknowledgement, increase the pheromone trail in forward direction.
 *
 * @param src address of the node that sent the packet
 * @param seq sequence number of the packet
 * @param nexthop address of the node the packet was overheard from
 * @return 1 if we are not waiting for a passive acknowledgement for this packet else 0
 */
int8_t ara_ack_eval_packet(ara_address_t src, ara_seq_t seq, ara_address_t nexthop) {
    int8_t ret = 1;
    ara_waitfor_key_t key;
    ara_wait_ack_t* entry;

    memset(&key, 0x00, sizeof(ara_waitfor_key_t));
    memcpy(key.src, src, sizeof(ara_address_t));
    key.seq = seq;
    memcpy(key.nexthop, nexthop, sizeof(ara_address_t));

    /*###*/
    pthread_rwlock_wrlock(&acklock);

    HASH_FIND(hh, wait, &key, sizeof(ara_waitfor_key_t), entry);

    if(entry) {
        HASH_DEL(wait, entry);
        dessert_debug("got acknowledgement for packet:\n\tsrc=" MAC " seq=%d next=" MAC, EXPLODE_ARRAY6(src), seq, EXPLODE_ARRAY6(nexthop));

        double delta_p;

        switch(ara_ptrail_mode) {
            case ARA_PTRAIL_CLASSIC:
                /// \todo which value should the pheromone have??? TTL does not make any sense!
                dessert_warn("ARA_PTRAIL_CLASSIC is missing a value for delta_p here!!!");
                //delta_p = ((double) (msg->ttl))/((double) 10);
                break;
            case ARA_PTRAIL_LINEAR:
            case ARA_PTRAIL_CUBIC:
                delta_p = rt_inc;
                break;
            default:
                assert(0); // should never happen
                break;
        }

        ara_rt_update(entry->dst, nexthop, entry->iface, delta_p, seq, 0); // ttl=0 as we do not know the distance in forward direction
        free(entry);
        ret = 0;
    }

    pthread_rwlock_unlock(&acklock);
    /*###*/

    return ret;
}

/** Handle missing acknowledgements
 *
 * Search the data structure for missing acknowledgements.
 */
dessert_per_result_t ara_ack_tick(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_wait_ack_t* cur;
    ara_wait_ack_t* tmp;

    pthread_rwlock_wrlock(&acklock);

    //for(cur = wait; cur != NULL;) {
    HASH_ITER(hh, wait, cur, tmp){
        if(scheduled->tv_sec > cur->t.tv_sec
           || (scheduled->tv_sec == cur->t.tv_sec && scheduled->tv_usec >= cur->t.tv_usec)) {
            dessert_warn("no ACK for packet: src=" MAC " seq=%d next=" MAC, EXPLODE_ARRAY6(cur->key.src), cur->key.seq, EXPLODE_ARRAY6(cur->key.nexthop));

            int res = 0;
            switch(ara_ack_mode) {
                case ARA_ACK_NETWORK: // should be pretty reliable due to layer 2 retransmits
                    res = ara_rt_modify_credit(cur->dst, cur->key.nexthop, cur->iface, -1);

                    if(res == ARA_RT_CREDIT_DEPLETED) {
//                      ara_rt_update(cur->dst, cur->key.nexthop, cur->iface, -1, cur->key.seq, 0);
                        ara_rt_delete(cur->dst, cur->key.nexthop, cur->iface, -1, cur->key.seq, 0);
                        dessert_warn("removed route, no credit: src=" MAC " seq=%d next=" MAC, EXPLODE_ARRAY6(cur->key.src), cur->key.seq, EXPLODE_ARRAY6(cur->key.nexthop));
                    }
                    break;

                case ARA_ACK_PASSIVE: {
                    /// \todo we are probably much too generous on the last hop where network layer ACKs are used!
                    res = ara_rt_modify_credit(cur->dst, cur->key.nexthop, cur->iface, -1);

                    if(res == ARA_RT_CREDIT_DEPLETED) {
//                      ara_rt_update(cur->dst, cur->key.nexthop, cur->iface, -1, cur->key.seq, 0);
                        ara_rt_delete(cur->dst, cur->key.nexthop, cur->iface, -1, cur->key.seq, 0);
                        dessert_warn("removed route, no credit: src=" MAC " seq=%d next=" MAC, EXPLODE_ARRAY6(cur->key.src), cur->key.seq, EXPLODE_ARRAY6(cur->key.nexthop));
                    }

                    break;
                }
                case ARA_ACK_LINK:
                    /// \todo implement link layer acknowledgement support
                    dessert_warn("Link layer acknowledgements not implemented yet!!!");
                    break;
                case ARA_ACK_DISABLED:
                default:
                    break; // we should never be here?!?
            }

            //ara_wait_ack_t* tmp  = cur;
            //HASH_DELETE(hh, wait, tmp);
            //free(tmp);
            HASH_DEL(wait, cur);
            free(cur);

            ara_ack_missed++;
        }

        //cur = (cur->hh).next;
    }

    pthread_rwlock_unlock(&acklock);

    TIMEVAL_ADD(scheduled, 0, ara_ack_wait_ms * 1000);
    dessert_periodic_add(ara_ack_tick, NULL, scheduled, NULL);
    return DESSERT_PER_KEEP;
}

/** Initialize acknowledgement processing
 *
 * Registers a function to periodically look for
 * unacknowledged packets.
 */
void ara_ack_init() {
    dessert_debug("initalizing ack monitor");
    dessert_periodic_add(ara_ack_tick, NULL, NULL, NULL);
    dessert_debug("ack monitor initialized");
}

/** Print routing table
 *
 * Print all routing table entries to the CLI.
 */
int cli_show_ack_monitor(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_wait_ack_t* cur = NULL;

    pthread_rwlock_rdlock(&acklock);

    for(cur = wait; cur != NULL; cur = (cur->hh).next) {
        cli_print(cli, "\nsrc=" MAC " seq=%d next=" MAC, EXPLODE_ARRAY6(cur->key.src), cur->key.seq, EXPLODE_ARRAY6(cur->key.nexthop));
    }

    pthread_rwlock_unlock(&acklock);

    cli_print(cli, "\n%d packets have not yet been acknowledged", ara_ack_missed);

    return CLI_OK;
}

/** Flush ack monitor
 *
 * Flush all ack monitor entries.
 */
int cli_flush_ack_monitor(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_wait_ack_t* cur = NULL;

    pthread_rwlock_wrlock(&acklock);

    while(wait) {
        cur = wait;
        HASH_DEL(wait, cur);
        free(cur);
    }

    pthread_rwlock_unlock(&acklock);

    cli_print(cli, "flushed ack monitor");
    dessert_warn("flushed ack monitor");

    return CLI_OK;
}
