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

uint8_t ara_retry_max = ARA_RETRY_MAX;
uint16_t ara_retry_delay_ms = ARA_RETRY_DELAY_MS;

/** trapped packet entry for packet trap */
typedef struct trapped_packet {
    dessert_msg_t* pkg;         ///< the message
    size_t len;                 ///< the message buffer length
    dessert_msg_proc_t* proc;   ///< message processing info
    dessert_frameid_t id;       ///< the frameid
} trapped_packet_t;

/** packet trap entry to store trapped packets */
typedef struct packettrap {
    ara_address_t dst;          ///< destination address
    trapped_packet_t* pkgs;     ///< NULL terminated List of packets waiting in the trap
    UT_hash_handle hh;          ///< handle for uthash
} packettrap_t;

packettrap_t* traped_packets = NULL; ///< packet trap hash table
pthread_rwlock_t traped_packets_lock = PTHREAD_RWLOCK_INITIALIZER; ///< packet trap lock

/** trap an undeliverable packet (no next hop available)
 *
 * @arg dst destination with discovery waiting
 * @arg pkg packet to trap - pointer will simply be copied, so you MUST NOT free it
 * @returns count of packets now trapped for this dst, -1 if packet was discarded
 */
int trap_packet(ara_address_t dst, dessert_msg_t* pkg, size_t len, dessert_msg_proc_t* proc, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    packettrap_t* mytrap = NULL;
    trapped_packet_t* pkti = NULL;
    int i = 0;

    /* check if the packet has been trapped too often */
    assert(proc != NULL);

    if(++(ap->trapcount) > ara_retry_max) {
        dessert_debug("discarding packet %d to " MAC " after %d times in trap", ap->seq, EXPLODE_ARRAY6(dst), ap->trapcount);
        return(-1);
    }
    else {
        dessert_debug("trapping packet %d to " MAC " for the %d. time", ap->seq, EXPLODE_ARRAY6(dst), ap->trapcount);
    }

    pthread_rwlock_wrlock(&traped_packets_lock);

    /* look up dst in hashtable - otherwise build list */
    HASH_FIND(hh, traped_packets, dst, sizeof(dst), mytrap);

    /* no packets trapped yet - build trap... */
    if(mytrap == NULL) {

        /* create the trap */
        mytrap = malloc(sizeof(packettrap_t));

        if(mytrap == NULL) {
            dessert_err("failed to allocate new hash table entry for packet trap");
            goto trap_packet_out;
        }

        memcpy((mytrap->dst), dst, sizeof(ara_address_t));

        mytrap->pkgs = malloc(ARA_TRAP_SLOTCHUNKSIZE * sizeof(trapped_packet_t));

        if(mytrap->pkgs == NULL) {
            dessert_err("failed to allocate new hash table entry for packet trap");
            free(mytrap);
            goto trap_packet_out;
        }

        mytrap->pkgs->pkg = NULL;
        mytrap->pkgs->len = 0;
        mytrap->pkgs->proc = NULL;
        mytrap->pkgs->id = 0;

        HASH_ADD_KEYPTR(hh, traped_packets, &(mytrap->dst), sizeof(dst), mytrap);
    }

    /* look for free slot */
    i = 0;

    for(pkti = mytrap->pkgs; pkti->pkg != NULL; pkti++) {
        i++;
    }

    /* do we need to grow ? */
    if(i % ARA_TRAP_SLOTCHUNKSIZE == ARA_TRAP_SLOTCHUNKSIZE - 1) {
        pkti = realloc(mytrap->pkgs, (i + ARA_TRAP_SLOTCHUNKSIZE + 1) * sizeof(trapped_packet_t));

        if(pkti == NULL) {
            dessert_err("failed to modify hash table entry for packet trap");
            goto trap_packet_out;
        }

        mytrap->pkgs = pkti;
        pkti += i;
    }

    /* copy/insert packet */
    dessert_msg_clone(&(pkti->pkg), pkg, false);
    dessert_msg_proc_clone(&(pkti->proc), proc);
    pkti->len = len;
    pkti->id = id;

    /* fix list */
    pkti++;
    i++;
    pkti->pkg = NULL;
    pkti->len = 0;
    pkti->proc = NULL;
    pkti->id = 0;

    /* done! */
trap_packet_out:
    pthread_rwlock_unlock(&traped_packets_lock);
    return(i);
}

/** untrap packets that have become deliverable
  * @arg dst destination with discovery waiting
  * @arg c callback to send the packets
  * @returns count of packets now trapped for this dst
  */
int untrap_packets(ara_address_t dst, dessert_meshrxcb_t* c) {
    packettrap_t* mytrap = NULL;
    trapped_packet_t* mypkt = NULL;
    int i = 0;

    pthread_rwlock_wrlock(&traped_packets_lock);
    HASH_FIND(hh, traped_packets, dst, sizeof(dst), mytrap);

    if(mytrap != NULL) {
        HASH_DELETE(hh, traped_packets, mytrap);
    }

    pthread_rwlock_unlock(&traped_packets_lock);

    /* untrap all packets */
    if(mytrap != NULL) {
        for(mypkt = mytrap->pkgs; mypkt->pkg != NULL; mypkt++) {
            if(c != NULL) {
                c(mypkt->pkg, mypkt->len, mypkt->proc, NULL, mypkt->id);
            }

            dessert_msg_destroy(mypkt->pkg);
            dessert_msg_proc_destroy(mypkt->proc);
            i++;
        }

        free(mytrap->pkgs);
        free(mytrap);
    }

    return(i);
}

/** Untrap packet for further handling
 *
 */
dessert_per_result_t ara_routefail_untrap_packets(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_address_t* dst = (ara_address_t*) data;
    untrap_packets(*dst, ara_retrypacket);
    free(data);
    return DESSERT_PER_KEEP;
}

/** CLI command - show trapped packets
 *
 * Print the content of the packet trap.
 */
int cli_showpackettrap(struct cli_def* cli, char* command, char* argv[], int argc) {
    packettrap_t* tr = NULL;

    pthread_rwlock_rdlock(&traped_packets_lock);

    for(tr = traped_packets; tr != NULL; tr = (tr->hh).next) {

        trapped_packet_t* pkti = NULL;
        int i = 0;

        cli_print(cli, "\ndst=" MAC ":", EXPLODE_ARRAY6(tr->dst));

        for(pkti = tr->pkgs; pkti->pkg != NULL; pkti++) {
            ara_proc_t* ap = ara_proc_get(pkti->proc);
            cli_print(cli, "\t#%04d seq=%06d trapcount=%03d", i++, ap->seq, ap->trapcount);
        }

    }

    pthread_rwlock_unlock(&traped_packets_lock);

    return CLI_OK;
}
