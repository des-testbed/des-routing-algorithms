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
#include <unistd.h>

/** loop protection table entry */
typedef struct ara_lpte {
    ara_address_t src;  ///< source address
    ara_seq_t seq;      ///< last sequence number seen
    uint64_t seen;      ///< bit-array of seen packets
    uint16_t ttl;       ///< time to live for this entry
    UT_hash_handle hh;  ///< used by uthash
} ara_lpte_t;

/* variables to keep some statistics */
uint32_t loopprotect_prevhop_self = 0;
uint32_t loopprotect_seen = 0;
uint32_t loopprotect_late = 0;
uint32_t loopprotect_looping = 0;
uint32_t loopprotect_duplicate = 0;

ara_lpte_t* lptt = NULL; ///< loop protection table
pthread_rwlock_t lplock = PTHREAD_RWLOCK_INITIALIZER; ///< loop protection table lock

int16_t loopprotect_tick_interval = 10; ///< how often run cleanup
int16_t loopprotect_ttl = 30;           ///< how long should an entry stay here

/**
 * Results of the loop and duplicate check functions
 */
typedef enum _ara_check_results {
    _LP_OK = 0,         ///< no looping packet or duplicate
    _LP_LATE,           ///< packet arrived late (after packets with higher sequence number)
    _LP_LOOPING,        ///< packet is looping or a duplicate
    _LP_NOTRACEHDR,     ///< no trace headers found (loop check not performed)
    _LP_INTERR = 255    ///< internal error
} _ara_check_res_t ;

/** Check for loops
 *
 * Check if a packet is looping. A detected loop is in fact always as loop but some
 * loops may stay undetected (e.g. because a node could no add a trace header).
 * @arg msg message to check
 * @returns _LP_NOTRACEHDR if trace header is missing
 * @returns _LP_OK if packet has trace header and was not already processed
 * @returns _LP_LOOPING if packet has trace header and was already processed
 **/
static _ara_check_res_t _ara_loopprotect_checkloop(dessert_msg_t* msg, ara_proc_t* ap) {
    dessert_ext_t*  ext;
    int x = dessert_msg_getext(msg, NULL, DESSERT_EXT_TRACE_REQ, 0);

    if(x < 1) { // no trace header
        return _LP_NOTRACEHDR;
    }

    int i;

    for(i = 0; i < x; i++) {

        dessert_msg_getext(msg, &ext, DESSERT_EXT_TRACE_REQ, i);

        if(memcmp(ext->data, dessert_l25_defsrc, ETHER_ADDR_LEN) == 0) {
            return _LP_LOOPING;
        }
    }

    return _LP_OK;
}

/** Check for duplicate
 *
 * Check if a packet is a duplicate based on sequence number. This is more
 * restrictive than check on the trace extension.
 * @param src sender of the packet
 * @param seq sequence number of the packet
 * @returns _LP_OK if packet has not been received yet and has the next expected sequence number
 * @return _LP_LATE if packet has not been received yet but it arrived later than some subsequently sent packets
 * @returns _LP_LOOPING if packet was already received
 */
static _ara_check_res_t _ara_loopprotect_checkduplicate(ara_address_t src, ara_seq_t seq) {
    ara_lpte_t* lp = NULL;
    ara_seq_t ds;
    int ret = _LP_INTERR;

    pthread_rwlock_wrlock(&lplock);
    HASH_FIND(hh, lptt, src, sizeof(ara_address_t), lp);

    if(lp == NULL) {
        lp = malloc(sizeof(ara_lpte_t));

        if(lp == NULL) {
            dessert_crit("failed to allocate memory");
            ret = _LP_INTERR;
            goto _ara_loopprotect_checkduplicate_out;
        }

        memcpy(&(lp->src), src, sizeof(ara_address_t));
        lp->seq = seq - 1;
        lp->seen = 0x0000000000000000;
        HASH_ADD_KEYPTR(hh, lptt, &(lp->src), sizeof(lp->src), lp);
    }

    /* update ttl */
    lp->ttl = loopprotect_ttl;

    /* no loop */
    ds = lp->seq - seq;

    if(lp->seq < seq || ara_seq_overflow(lp->seq, seq)) {
        uint64_t nseen = (lp->seen) << (seq - lp->seq);
        nseen |= 0x0000000000000001;
        dessert_debug("packet is new:\n\tsrc=" MAC " seq=%d, lp->seq=%d, (seq - lp->seq)=%d, lp->seen=%ld, nseen=%d",
            EXPLODE_ARRAY6(src), seq, (lp->seq), (seq - lp->seq), (long)(lp->seen), (long) nseen);

        lp->seq = seq;
        lp->seen = nseen;
        lp->seen |= 0x0000000000000001;
        ret = _LP_OK;
    }
    else if(ds < 64 && ((lp->seen) & ((uint64_t) 0x0000000000000001 << ds)) == 0) {
        uint64_t nseen = lp->seen | ((uint64_t) 0x0000000000000001 << ds);
        dessert_debug("packet arrived late:\n\tsrc=" MAC " seq=%d, lp->seq=%d, (lp->seq - seq)=%d, lp->seen=%ld, nseen=%d",
            EXPLODE_ARRAY6(src), seq, (lp->seq), (lp->seq - seq), (long)(lp->seen), (long) nseen);

        (lp->seen) = nseen;
        ret = _LP_LATE;
    }
    else {
        dessert_debug("duplicate packet:\n\tsrc=" MAC " seq=%d, lp->seq=%d,lp->seen=%ld",
            EXPLODE_ARRAY6(src), seq, (lp->seq), (long)(lp->seen));

        ret = _LP_LOOPING;
    }

_ara_loopprotect_checkduplicate_out:
    pthread_rwlock_unlock(&lplock);

    return ret;
}

static void _ara_send_loop_notification(dessert_msg_t* msg, dessert_meshif_t* iface, ara_proc_t* ap) {
    dessert_warn("returning looping unicast packet to previous hop:\n\tsrc=" MAC " dst=" MAC " prev=" MAC " seq=%06d iface=%s",
        EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop), ap->seq, iface->if_name);

    /* send route fail */
    msg->u8 |= ARA_LOOPING_PACKET;
    ara_address_t tmp;
    memcpy(tmp, (msg->l2h.ether_dhost), ETHER_ADDR_LEN);
    memcpy((msg->l2h.ether_dhost), (msg->l2h.ether_shost), ETHER_ADDR_LEN);
    memcpy((msg->l2h.ether_shost), tmp, ETHER_ADDR_LEN);
    msg->ttl--;

    if(msg->ttl > 0) {
        dessert_meshsend_fast(msg, iface);
    }
}

/** Check for duplicates
 *
 * Check for duplicates based on the sequence number. The message will be dropped if this
 * node did forward exactly this message.
 * ARA messages to signal loops are route fails may always pass.
 * ANTs may also pass to enter the routes in the routing table. They are marked with ARA_DUPLICATE
 * and thus are subsequently dropped and never forwarded twice.
 * For duplicated unicast messages, a notification is sent to the previous hop.
 */
dessert_cb_result ara_checkdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    int res = _ara_loopprotect_checkduplicate(ap->src, ap->seq);

    // we want to handle some specific duplicates in subsequent callbacks
    if(msg->u8 & ARA_LOOPING_PACKET
       || msg->u8 & ARA_ROUTEFAIL) {
        if(res == _LP_LOOPING) {
            ap->flags |= ARA_DUPLICATE;
        }

        return DESSERT_MSG_KEEP;
    }

    switch(res) {
        case _LP_INTERR:
            return DESSERT_MSG_DROP;
        case _LP_OK:
            return DESSERT_MSG_KEEP;
        case _LP_LATE:
            loopprotect_late++;
            return DESSERT_MSG_KEEP;
        case _LP_LOOPING:
            dessert_debug("duplicate packet:\n\tsrc=" MAC " dst=" MAC " prev=" MAC " seq=%06d iface=%s",
                EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop), ap->seq, iface->if_name);
            loopprotect_duplicate++;

            // (unicast) duplicates are dropped and the previous hop is notified
            // but not for packets destined to this host
            if(!(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
                 || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST)
               && !(proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
                _ara_send_loop_notification(msg, iface, ap);
                return DESSERT_MSG_DROP;
            }
            // learn new routes from broadcasts (includes ANTs) and multicasts
            else {
                ap->flags |= ARA_DUPLICATE;
                return DESSERT_MSG_KEEP;
            }

        default:
            assert(0);
    }

    return(DESSERT_MSG_KEEP);
}

/** Check for looping packets
 *
 * Check for loops based on trace extensions. The message will be dropped if this
 * node did forward exactly this message.
 * ARA messages to signal loops are route fails may always pass.
 * A notification is sent to the previous hop if this was an unicast message (ANT are broadcasts!).
 */
dessert_cb_result ara_checkloop(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    int res = _ara_loopprotect_checkloop(msg, ap);

    // we want to handle some specific duplicates in subsequent callbacks
    if(msg->u8 & ARA_LOOPING_PACKET
       || msg->u8 & ARA_ROUTEFAIL) {
        if(res == _LP_LOOPING) {
            ap->flags |= ARA_LOOPING;
        }

        return DESSERT_MSG_KEEP;
    }

    // try to evaluate the trace header first
    switch(res) {
        case _LP_OK:
            return(DESSERT_MSG_KEEP);
        case _LP_LOOPING:
            dessert_debug("dropping looping packet:\n\tsrc=" MAC " dst=" MAC " prev=" MAC " seq=%06d iface=%s",
                EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop), ap->seq, iface->if_name);
            loopprotect_looping++;

            if(!(proc->lflags & (DESSERT_RX_FLAG_L25_BROADCAST | DESSERT_RX_FLAG_L25_MULTICAST))
               && !(proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
                _ara_send_loop_notification(msg, iface, ap);
            }

            return DESSERT_MSG_DROP;
        case _LP_NOTRACEHDR:
            // if there is no trace header try to detect loops based on sequence number (more restrictive)
            return DESSERT_MSG_KEEP;
        default:
            assert(0);
    }

    return(DESSERT_MSG_KEEP);
}

/** Handle returned looping packets
 *
 * Handle packets with msg->u8 & ARA_LOOPING_PACKET returned by the next hop due to a loop.
 * Reset the flags and delete the route. Subsequent callbacks will try to find an alternative route.
 */
dessert_cb_result ara_handle_loops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    if(msg->u8 & ARA_LOOPING_PACKET) {
        dessert_info("received looping packet returned from " MAC ":\n\tsrc=" MAC " dst=" MAC " seq=%06d iface=%s",
            EXPLODE_ARRAY6(ap->prevhop), EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq, iface_in->if_name);
//      ara_rt_update(ap->dst, ap->prevhop, iface_in, -1, ap->seq, 0);
        ara_rt_delete(ap->dst, ap->prevhop, iface_in, -1, ap->seq, 0);
        msg->u8 &= ~ARA_LOOPING_PACKET;
        ap->flags &= ~ARA_LOOPING;
        ap->flags &= ~ARA_DUPLICATE;
        ap->flags |= ARA_RT_UPDATE_IGN;
    }

    return DESSERT_MSG_KEEP;
}

/** Drop duplicates
 *
 * Drop all packets tagged as duplicate.
 */
dessert_cb_result ara_dropdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    if(ap->flags & ARA_DUPLICATE) {
        dessert_debug("dropping duplicate packet:\n\tsrc=" MAC " dst=" MAC " prev=" MAC " seq=%06d iface=%s",
            EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop), ap->seq, iface->if_name);
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

/** age loop protection table entries */
dessert_per_result_t ara_loopprotect_tick(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_lpte_t* lp;
    int i = 0;

    scheduled->tv_sec += loopprotect_tick_interval;
    dessert_periodic_add(ara_loopprotect_tick, NULL, scheduled, NULL);

    pthread_rwlock_wrlock(&lplock);

    for(lp = lptt; lp != NULL; /* must be done below */) {
        if(--(lp->ttl) <= 0) {
            ara_lpte_t* tmp = lp;
            lp = (lp->hh).next;
            HASH_DELETE(hh, lptt, tmp);
            free(tmp);
            i++;
        }
        else {
            lp = (lp->hh).next;
        }
    }

    pthread_rwlock_unlock(&lplock);

    if(i > 0) {
        dessert_debug("deleted %d loopprotect entries", i);
    }

    return DESSERT_PER_KEEP;
}

/** set up loop protection table */
void ara_loopprotect_init() {
    dessert_debug("initalizing loop protection");
    dessert_periodic_add(ara_loopprotect_tick, NULL, NULL, NULL);
    dessert_debug("loop protection initialized");
}

/** CLI command - config mode - interface tap $iface, $ipv4-addr, $netmask */
int cli_showloopprotect_table(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_lpte_t* lp = NULL;

    pthread_rwlock_rdlock(&lplock);

    for(lp = lptt; lp != NULL; lp = (lp->hh).next) {
        char x[65];
        int i;

        for(i = 0; i < 64; i++) {
            x[63-i] = (((uint64_t)((uint64_t) 0x0000000000000001 << i) & (lp->seen)) == 0) ? '_' : 'x';
        }

        x[64] = 0x00;

        cli_print(cli, "\n" MAC " %s..%06d", EXPLODE_ARRAY6(lp->src), x, lp->seq);
    }

    pthread_rwlock_unlock(&lplock);
    return CLI_OK;
}

/** CLI command - config mode - interface tap $iface, $ipv4-addr, $netmask */
int cli_showloopprotect_statistics(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "\nloopprotect_statistics:");
    cli_print(cli, "\tseen:         %10d", loopprotect_seen);
    cli_print(cli, "\tlate:         %10d", loopprotect_late);
    cli_print(cli, "\tprevhop_self: %10d", loopprotect_prevhop_self);
    cli_print(cli, "\tlooping:      %10d", loopprotect_looping);
    cli_print(cli, "\tduplicate:    %10d", loopprotect_duplicate);

    return CLI_OK;
}

/** Flush loop protection table
 *
 * Flush loop protection table. Subsequently received duplicates will not be
 * detected!
 */
int cli_flush_loopprotec_table(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_lpte_t* cur = NULL;

    pthread_rwlock_wrlock(&lplock);

    while(lptt) {
        cur = lptt;
        HASH_DEL(lptt, cur);
        free(cur);
    }

    pthread_rwlock_unlock(&lplock);

    cli_print(cli, "flushed loop protection table");
    dessert_warn("flushed loop protection table");

    return CLI_OK;
}
