/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
 Extended and debugged by Bastian Blywis, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by Philipp Schmidt
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

void _ara_sendbant(ara_address_t addr);

/** PANT table entry */
typedef struct ara_rusage {
    ara_address_t dst;  ///< the destination
    struct timeval t;   ///< last time a packet was sent to the destination
    UT_hash_handle hh;  ///< handle for hash table usage
} ara_usage_t;

ara_usage_t* ut = NULL; ///< PANT table
pthread_rwlock_t ullock = PTHREAD_RWLOCK_INITIALIZER; ///< PANT table lock

uint8_t ara_pant_interval = ARA_PANT_INTERVAL; ///< PANT interval

/** Save timestamp when sending packets
 *
 * Saves the timestamp when the last packet was send to a particular destination.
 * This is used for route maintenance.
 */
int ara_maintainroute_timestamp(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != 0);

    ara_maintainroute_stamp(ap->dst);

    return DESSERT_MSG_KEEP;
}

void ara_maintainroute_stamp(ara_address_t dst) {
    struct timeval now;
    ara_usage_t* last;
    gettimeofday(&now, NULL);

    pthread_rwlock_wrlock(&ullock);

    HASH_FIND(hh, ut, &(dst), sizeof(ara_address_t), last);

    if(last == NULL) {
        last = malloc(sizeof(ara_usage_t));
        memcpy(&(last->dst), &(dst), sizeof(ara_address_t));
        HASH_ADD_KEYPTR(hh, ut, &(last->dst), sizeof(ara_address_t), last);
    }

    /* update */
    last->t.tv_sec = now.tv_sec;
    last->t.tv_usec = now.tv_usec;

    pthread_rwlock_unlock(&ullock);
}

/** Send PANT for route maintenance
 *
 * Send a (Periodic) ANT if a packet was received from a dst the host has not sent a packet
 * to for ara_pant_interval seconds.
 */
int ara_maintainroute_pant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != 0);

    // a value of zero disables PANTs
    if(ara_pant_interval == 0
       || !(ap->flags & proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
        return DESSERT_MSG_KEEP;
    }

    struct timeval now;

    ara_usage_t* last;

    gettimeofday(&now, NULL);

    pthread_rwlock_rdlock(&ullock);

    HASH_FIND(hh, ut, &(ap->src), sizeof(ara_address_t), last);

    if(last == NULL) {
        last = malloc(sizeof(ara_usage_t));
        memcpy(&(last->dst), &(ap->src), sizeof(ara_address_t));
        HASH_ADD_KEYPTR(hh, ut, &(last->dst), sizeof(ara_address_t), last);
        last->t.tv_sec = 0;
        last->t.tv_usec = 0;
    }

    /* need to send pant ?*/
    if((last->t.tv_sec) + ara_pant_interval < now.tv_sec) {
        last->t.tv_sec = now.tv_sec;
        last->t.tv_usec = now.tv_usec;

        _ara_sendbant(ap->src);
        dessert_info("the last BANT was a PANT");
    }

    pthread_rwlock_unlock(&ullock);

    return DESSERT_MSG_KEEP;
}

/** Flush route management table
 *
 * Flush route management table. This will generate a BANT for each first unicast
 * packet received from any source.
 */
int cli_flush_rmnt(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_usage_t* cur = NULL;

    pthread_rwlock_wrlock(&ullock);

    while(ut) {
        cur = ut;
        HASH_DEL(ut, cur);
        free(cur);
    }

    pthread_rwlock_unlock(&ullock);

    cli_print(cli, "flushed route management table");
    dessert_warn("flushed route management table");

    return CLI_OK;
}
