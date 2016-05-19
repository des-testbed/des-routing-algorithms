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

static uint32_t seq_num_global = 0;
static pthread_rwlock_t seq_num_lock = PTHREAD_RWLOCK_INITIALIZER;

/* tracks a running series of RREQs to msg->dhost_ether */
/* Nachbedingung: die Serie wird nicht mehr referenziert. Es kann sofort eine neue Serie zum ziel gestartet werden. */
//Invariante: Es gibt nur zwei mögliche Zustände für eine Serie: geschedulet oder in Verarbeitung durch send_rreq, dass noch MINDESTENS EINMAL die markierung prüft
//Invariante: a series is either owned by a invocation of send_rreq, delete_series, reschedule_series or of the schedule table
//Invariante: a schedule is only added by a valid series and dropped or popped before the series is deleted
struct aodv_rreq_series {
    dessert_msg_t *msg;
    int retries;
    uint64_t key;
    /* the series is not in the series_list anymore. Implies the series should be terminated at the next possibility */
    bool stop;
    struct aodv_rreq_series *prev, *next;
};
static aodv_rreq_series_t *series_list = NULL;
/* synchronizes access to the attributes key, stop, prev and next in all elements of the list. *msg and retries can be changed by the owner of the respective series (except the destination address in *msg, which is cached in key and interacts with the schedule table) */
static pthread_rwlock_t series_list_lock = PTHREAD_RWLOCK_INITIALIZER;

static aodv_rreq_series_t *aodv_pipeline_find_series_unlocked(mac_addr addr) {
    aodv_rreq_series_t *el;
    uint64_t addr_as_uint = hf_mac_addr_to_uint64(addr);
    DL_SEARCH_SCALAR(series_list, el, key, addr_as_uint);
    return el;
}

/** creates a series if one is not already running
 *  takes ownership of msg
 *  @return the newly created series (destroy with aodv_pipeline_delete_series) or NULL if a series for the msg's destination is running already
 */
static aodv_rreq_series_t *aodv_pipeline_new_series(dessert_msg_t *msg) {
    struct ether_header* l25h = dessert_msg_getl25ether(msg);
    pthread_rwlock_wrlock(&series_list_lock);
    aodv_rreq_series_t *pre_existing = aodv_pipeline_find_series_unlocked(l25h->ether_dhost);
    if(pre_existing) {
        pthread_rwlock_unlock(&series_list_lock);
        return NULL;
    }
    //we can safely start a new series
    aodv_rreq_series_t *series = malloc(sizeof(*series));
    series->msg = msg;
    series->key = hf_mac_addr_to_uint64(l25h->ether_dhost);
    series->retries = 0;
    series->stop = false;
    DL_APPEND(series_list, series);
    pthread_rwlock_unlock(&series_list_lock);
    return series;
}

// Don't call this directly, but one of the two (locking) versions below
static void aodv_pipeline_delete_series_unlocked(aodv_rreq_series_t *series) {
    if(!series->stop) {
        DL_DELETE(series_list, series);
        series->stop = true; //mark for deletion by the owner
        struct ether_header* l25h = dessert_msg_getl25ether(series->msg);
        bool dropped = aodv_db_dropschedule(l25h->ether_dhost, AODV_SC_REPEAT_RREQ);
        if(dropped) {
            // we took ownership of the series and can safely delete it
            dessert_msg_destroy(series->msg);
            free(series);
        }
    }
}

static inline void aodv_pipeline_delete_series(aodv_rreq_series_t *series) {
    pthread_rwlock_wrlock(&series_list_lock);
    aodv_pipeline_delete_series_unlocked(series);
    pthread_rwlock_unlock(&series_list_lock);
}

void aodv_pipeline_delete_series_ether(mac_addr addr) {
    pthread_rwlock_wrlock(&series_list_lock);
    aodv_rreq_series_t *series = aodv_pipeline_find_series_unlocked(addr);
    if(series) {
        aodv_pipeline_delete_series_unlocked(series);
    }
    pthread_rwlock_unlock(&series_list_lock);
}

static void aodv_pipeline_reschedule_series(struct timeval when, aodv_rreq_series_t *series) {
    pthread_rwlock_wrlock(&series_list_lock);
    if(!series->stop) {
        struct ether_header* l25h = dessert_msg_getl25ether(series->msg);
        //pass ownership to the db
        aodv_db_addschedule(&when, l25h->ether_dhost, AODV_SC_REPEAT_RREQ, series);
    }
    else {
        dessert_msg_destroy(series->msg);
        free(series);
    }
    pthread_rwlock_unlock(&series_list_lock);
}

// ---------------------------- help functions ---------------------------------------

dessert_msg_t* _create_rreq(mac_addr dhost_ether, uint8_t ttl, metric_t initial_metric) {
    dessert_msg_t* msg;
    dessert_ext_t* ext;
    dessert_msg_new(&msg);

    msg->ttl = ttl;
    msg->u8 = 0; /*hop count */

    // add l25h header
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
    mac_copy(rreq_l25h->ether_shost, dessert_l25_defsrc);
    mac_copy(rreq_l25h->ether_dhost, dhost_ether);

    // add RREQ ext
    dessert_msg_addext(msg, &ext, RREQ_EXT_TYPE, sizeof(struct aodv_msg_rreq));
    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) ext->data;
    msg->u16 = initial_metric;
    rreq_msg->flags = 0;

    //this is for local repair, we know that the latest rrep we saw was last_destination_sequence_number
    uint32_t last_destination_sequence_number;

    if(aodv_db_get_destination_sequence_number(dhost_ether, &last_destination_sequence_number) != true) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_U;
    }

    if(dest_only) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_D;
    }

    rreq_msg->destination_sequence_number = last_destination_sequence_number;

    int d = aodv_db_get_warn_status(dhost_ether);

    if(d == true) {
        rreq_msg->flags |= AODV_FLAGS_RREQ_D;
    }

    dessert_msg_dummy_payload(msg, rreq_size);

    return msg;
}

dessert_msg_t* _create_rrep(mac_addr route_dest, mac_addr route_source, mac_addr rrep_next_hop, uint32_t destination_sequence_number, uint8_t flags, uint8_t hop_count, metric_t initial_metric) {
    dessert_msg_t* msg;
    dessert_ext_t* ext;
    dessert_msg_new(&msg);

    msg->ttl = TTL_MAX;
    msg->u8 = hop_count;

    // add l25h header
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* rreq_l25h = (struct ether_header*) ext->data;
    mac_copy(rreq_l25h->ether_shost, route_dest);
    mac_copy(rreq_l25h->ether_dhost, route_source);

    // set next hop
    mac_copy(msg->l2h.ether_dhost, rrep_next_hop);

    // and add RREP ext
    dessert_msg_addext(msg, &ext, RREP_EXT_TYPE, sizeof(struct aodv_msg_rrep));
    struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) ext->data;
    rrep_msg->flags = flags;
    msg->u16 = initial_metric;
    rrep_msg->lifetime = 0;
    rrep_msg->destination_sequence_number = destination_sequence_number;
    return msg;
}

static void aodv_send_rreq_real(aodv_rreq_series_t *series) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
    // if we sent too many RREQs in the last second, try again later
    uint32_t rreq_count;
    aodv_db_getrreqcount(&ts, &rreq_count);

    if(rreq_count > RREQ_RATELIMIT) {
        dessert_trace("we have reached RREQ_RATELIMIT");
        struct timeval postpone = hf_tv_add_ms(ts, 20);
        aodv_pipeline_reschedule_series(postpone, series);
        return;
    }

    dessert_msg_t *msg = series->msg;
    dessert_ext_t* ext;
    dessert_msg_getext(msg, &ext, RREQ_EXT_TYPE, 0);
    struct aodv_msg_rreq* rreq = (struct aodv_msg_rreq*) ext->data;

    pthread_rwlock_wrlock(&seq_num_lock);
    rreq->originator_sequence_number = ++seq_num_global;
    pthread_rwlock_unlock(&seq_num_lock);

    struct ether_header* l25h = dessert_msg_getl25ether(series->msg);
    dessert_debug("sending RREQ to " MAC " ttl=%ju id=%ju", EXPLODE_ARRAY6(l25h->ether_dhost), (uintmax_t)msg->ttl, (uintmax_t)rreq->originator_sequence_number);
    dessert_meshsend(msg, NULL);
    gettimeofday(&ts, NULL);
    aodv_db_putrreq(&ts);

    if(series->retries >= RREQ_RETRIES) {
        /* RREQ has been tried for the max. number of times -- give up */
        aodv_pipeline_delete_series(series);
        return;
    }
    dessert_trace("add task to repeat RREQ");

    /* RING_TRAVERSAL_TIME equals NET_TRAVERSAL_TIME if ring_search is off */
    uintmax_t ring_traversal_time = 2 * NODE_TRAVERSAL_TIME * min(NET_DIAMETER, msg->ttl);
    struct timeval repeat_time  = hf_tv_add_ms(ts, ring_traversal_time);

    series->retries++;
    if(ring_search && msg->ttl <= TTL_THRESHOLD) {
        msg->ttl += TTL_INCREMENT;
        if(msg->ttl > TTL_THRESHOLD) {
            msg->ttl = TTL_MAX;
        }
    }
    aodv_pipeline_reschedule_series(repeat_time, series);
}

void aodv_send_rreq(mac_addr dhost_ether, struct timeval* ts) {
    // RFC uses NET_DIAMETER as maximum ttl value, but we don't need ttl for loop detection
    uint8_t ttl = ring_search ? TTL_START : TTL_MAX;
    dessert_msg_t* msg = _create_rreq(dhost_ether, ttl, metric_startvalue);

    aodv_rreq_series_t* series = aodv_pipeline_new_series(msg);
    if(!series) {
        dessert_trace("There is a rreq schedule to this dest. We dont start a new series.");
        return;
    }
    aodv_send_rreq_real(series);
}

void aodv_send_rreq_repeat(struct timeval* ts, aodv_rreq_series_t* series) {
    assert(series);
    aodv_send_rreq_real(series);
}

// ---------------------------- pipeline callbacks ---------------------------------------------

int aodv_drop_errors(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    // drop packets sent by myself.
    if(proc->lflags & DESSERT_RX_FLAG_L2_SRC) {
        return DESSERT_MSG_DROP;
    }

    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
        return DESSERT_MSG_DROP;
    }

    /**
     * First check neighborhood, since it is possible that one
     * RREQ from one unidirectional neighbor can be added to broadcast id table
     * and then dropped as a message from unidirectional neighbor!
     */
    dessert_ext_t* ext;
    struct timeval ts;
    gettimeofday(&ts, NULL);

    // check whether control messages were sent over bidirectional links, otherwise DROP
    // Hint: RERR must be resent in both directions.
    if((dessert_msg_getext(msg, &ext, RREQ_EXT_TYPE, 0) != 0) || (dessert_msg_getext(msg, &ext, RREP_EXT_TYPE, 0) != 0)) {
        if(aodv_db_check2Dneigh(msg->l2h.ether_shost, iface, &ts) != true) {
            dessert_debug("DROP RREQ/RREP from " MAC " metric=%" AODV_PRI_METRIC " hop_count=%" PRIu8 " ttl=%" PRIu8 "-> neighbor is unidirectional!", EXPLODE_ARRAY6(msg->l2h.ether_shost), msg->u16, msg->u8, msg->ttl);
            return DESSERT_MSG_DROP;
        }
    }

    return DESSERT_MSG_KEEP;
}

int aodv_handle_hello(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* hallo_ext;

    if(dessert_msg_getext(msg, &hallo_ext, HELLO_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    struct aodv_msg_hello* hello_msg = (struct aodv_msg_hello*) hallo_ext->data;

    struct timeval ts;
    gettimeofday(&ts, NULL);

    msg->ttl--;

    if(msg->ttl >= 1) {
        // hello req
        uint8_t rcvd_hellos = 0;
        aodv_db_pdr_cap_hello(msg->l2h.ether_shost, msg->u16, hello_msg->hello_interval, &ts);
        if(aodv_db_pdr_get_rcvdhellocount(msg->l2h.ether_shost, &rcvd_hellos, &ts) == true) {
            hello_msg->hello_rcvd_count = rcvd_hellos;
        }
        hello_msg->hello_interval = hello_interval; 
        mac_copy(msg->l2h.ether_dhost, msg->l2h.ether_shost);
        dessert_meshsend(msg, iface);
        // dessert_trace("got hello-req from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_shost));
    }
    else {
        //hello rep
        if(mac_equal(iface->hwaddr, msg->l2h.ether_dhost)) {
            aodv_db_pdr_cap_hellorsp(msg->l2h.ether_shost, hello_msg->hello_interval, hello_msg->hello_rcvd_count, &ts);
            // dessert_trace("got hello-rep from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_dhost));
            aodv_db_cap2Dneigh(msg->l2h.ether_shost, msg->u16, iface, &ts);
        }
    }

    return DESSERT_MSG_DROP;
}

int aodv_handle_rreq(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rreq_ext;
    char* comment;
    if(!dessert_msg_getext(msg, &rreq_ext, RREQ_EXT_TYPE, 0)) {
        return DESSERT_MSG_KEEP;
    }

    struct aodv_msg_rreq* rreq_msg = (struct aodv_msg_rreq*) rreq_ext->data;

    if(msg->ttl) {
        msg->ttl--;
    }
    msg->u8++; /* hop count */

    struct timeval ts;
    gettimeofday(&ts, NULL);
    aodv_metric_do(&(msg->u16), msg->l2h.ether_shost, iface, &ts);

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    aodv_capt_rreq_result_t capt_result;
    aodv_db_capt_rreq(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, iface, rreq_msg->originator_sequence_number, msg->u16, msg->u8, &ts, &capt_result);

    aodv_gossip_capt_rreq(msg);

    if(capt_result == AODV_CAPT_RREQ_OLD) {
        comment = "discarded";
        goto drop;
    }

    /* Process RREQ also as RREP */
    int updated_route = aodv_db_capt_rrep(l25h->ether_shost, msg->l2h.ether_shost, iface, 0 /* force */, msg->u16, msg->u8, &ts);
    if(updated_route) {
        // no need to search for next hop. Next hop is RREQ.msg->l2h.ether_shost
        aodv_send_packets_from_buffer(l25h->ether_shost, msg->l2h.ether_shost, iface);
    }

    uint16_t unknown_seq_num_flag = rreq_msg->flags & AODV_FLAGS_RREQ_U;
    if(proc->lflags & DESSERT_RX_FLAG_L25_DST) {
        pthread_rwlock_wrlock(&seq_num_lock);
        uint32_t rrep_seq_num;
        /* increase our sequence number on metric hit, so that the updated
         * RREP doesn't get discarded as old */
        if(capt_result == AODV_CAPT_RREQ_METRIC_HIT) {
            seq_num_global++;
        }
        /* set our sequence number to the maximum of the current value and
         * the destination sequence number in the RREQ (RFC 6.6.1) */
        if(!unknown_seq_num_flag && hf_comp_u32(rreq_msg->destination_sequence_number, seq_num_global) > 0) {
            seq_num_global = rreq_msg->destination_sequence_number;
        }
        rrep_seq_num = seq_num_global;
        pthread_rwlock_unlock(&seq_num_lock);

        dessert_msg_t* rrep_msg = _create_rrep(dessert_l25_defsrc, l25h->ether_shost, msg->l2h.ether_shost, rrep_seq_num, 0, 0, metric_startvalue);
        dessert_meshsend(rrep_msg, iface);
        dessert_msg_destroy(rrep_msg);
        comment = "for me";
    }
    else {
        uint16_t dest_only_flag = rreq_msg->flags & AODV_FLAGS_RREQ_D;
        uint32_t our_dest_seq_num;
        int we_have_seq_num = aodv_db_get_destination_sequence_number(l25h->ether_dhost, &our_dest_seq_num);
        // do local repair if D flag is not set and we have a valid route to dest
        bool local_repair = !dest_only_flag && we_have_seq_num;
        if(we_have_seq_num && !unknown_seq_num_flag) {
            uint32_t rreq_dest_seq_num = rreq_msg->destination_sequence_number;
            // but don't repair if rreq has newer dest_seq_num
            if(hf_comp_u32(rreq_dest_seq_num, our_dest_seq_num) > 0) {
                local_repair = false;
            }
        }

        if(local_repair) {
            metric_t dest_metric;
            uint8_t dest_hop_count;
            aodv_db_get_metric(l25h->ether_dhost, &dest_metric);
            aodv_db_get_hopcount(l25h->ether_dhost, &dest_hop_count);

            dessert_msg_t* rrep_msg = _create_rrep(l25h->ether_dhost, l25h->ether_shost, msg->l2h.ether_shost, our_dest_seq_num, 0, dest_hop_count, dest_metric);
            dessert_meshsend(rrep_msg, iface);
            dessert_msg_destroy(rrep_msg);
            comment = "locally repaired";
        }
        else {
            if(gossip_type == GOSSIP_NONE && msg->ttl == 0) {
                comment = "dropped (TTL)";
                goto drop;
            }
            if(gossip_type == GOSSIP_NONE || aodv_gossip(msg)) {
                dessert_meshsend(msg, NULL);
                comment = "rebroadcasted";
            }
            else {
                comment = "dropped (gossip)";
            }
        }
    }
drop:
    dessert_debug("incoming RREQ from " MAC " over " MAC " to " MAC " seq=%ju ttl=%ju | %s", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), (uintmax_t)rreq_msg->originator_sequence_number, (uintmax_t)msg->ttl, comment);
    return DESSERT_MSG_DROP;
}

int aodv_handle_rerr(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rerr_ext;

    if(dessert_msg_getext(msg, &rerr_ext, RERR_EXT_TYPE, 0) == 0) {
        return DESSERT_MSG_KEEP;
    }

    struct aodv_msg_rerr* rerr_msg = (struct aodv_msg_rerr*) rerr_ext->data;

    int rerrdl_num = 0;

    int rebroadcast_rerr = false;

    dessert_ext_t* rerrdl_ext;

    while(dessert_msg_getext(msg, &rerrdl_ext, RERRDL_EXT_TYPE, rerrdl_num++) > 0) {
        struct aodv_mac_seq* destination_list = (void*)rerrdl_ext->data;
        int destination_count = (rerrdl_ext->len - 2) / (int)sizeof(struct aodv_mac_seq);

        for(int i = 0; i < destination_count; ++i) {
            struct aodv_mac_seq* dest = &destination_list[i];
            mac_addr next_hop;

            if(!aodv_db_getnexthop(dest->host, next_hop)) {
                continue;
            }

            /* If our next_hop to dest is a interface of the neighbor that generated the RERR,
             * this route is affected and must be invalidated!*/
            for(int j = 0; j < rerr_msg->iface_addr_count; ++j) {
                if(mac_equal(rerr_msg->ifaces[j], next_hop)) {
                    bool inv_route = aodv_db_markrouteinv(dest->host, dest->sequence_number);
                    if(inv_route) {
                        dessert_debug("invalidated route to " MAC " (RERR)", EXPLODE_ARRAY6(dest->host));
                        rebroadcast_rerr = true;
                    }
                }
            }
        }
    }

    if(rebroadcast_rerr) {
        // write addresses of all my mesh interfaces
        dessert_meshif_t* iface = dessert_meshiflist_get();
        rerr_msg->iface_addr_count = 0;
        void* iface_addr_pointer = rerr_msg->ifaces;

        while(iface != NULL && rerr_msg->iface_addr_count < MAX_MESH_IFACES_COUNT) {
            mac_copy(iface_addr_pointer, iface->hwaddr);
            iface_addr_pointer += ETH_ALEN;
            iface = iface->next;
            rerr_msg->iface_addr_count++;
        }

        dessert_meshsend(msg, NULL);
    }

    return DESSERT_MSG_DROP;
}

int aodv_handle_rrep(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rrep_ext;
    char* comment;
    if(!dessert_msg_getext(msg, &rrep_ext, RREP_EXT_TYPE, 0)) {
        return DESSERT_MSG_KEEP;
    }

    if(!(proc->lflags & DESSERT_RX_FLAG_L2_DST)) {
        return DESSERT_MSG_DROP;
    }

    struct aodv_msg_rrep* rrep_msg = (struct aodv_msg_rrep*) rrep_ext->data;

    if(msg->ttl) {
        msg->ttl--;
    }
    msg->u8++; /* hop count */

    struct timeval ts;
    gettimeofday(&ts, NULL);
    aodv_metric_do(&(msg->u16), msg->l2h.ether_shost, iface, &ts);

    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    int rrep_used = aodv_db_capt_rrep(l25h->ether_shost, msg->l2h.ether_shost, iface, rrep_msg->destination_sequence_number, msg->u16, msg->u8, &ts);
    if(!rrep_used) {
        // capture and re-send only if route is unknown OR
        // sequence number is greater then that in database OR
        // if seq_nums are equal and known metric is worse than RREP's
        comment = "discarded";
        goto drop;
    }

    if(!(proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
        // forward RREP to RREQ originator
        mac_addr next_hop;
        dessert_meshif_t* output_iface;

        int reverse_route_found = aodv_db_getroute2dest(l25h->ether_dhost, next_hop, &output_iface, &ts, AODV_FLAGS_UNUSED);

        if(reverse_route_found) {
            aodv_db_add_precursor(l25h->ether_shost, next_hop, output_iface);
            mac_copy(msg->l2h.ether_dhost, next_hop);
            dessert_meshsend(msg, output_iface);
            comment = "forwarded";
        }
        else {
            comment = "dropped (no reverse route)";
        }
    }
    else {
        comment = "for me";
        /* Pop all packets from FIFO buffer and send to destination
         * no need to search for next hop. Next hop is RREP.prev_hop */

        aodv_send_packets_from_buffer(l25h->ether_shost, msg->l2h.ether_shost, iface);
    }
drop:
    dessert_debug("incoming RREP from " MAC " over " MAC " to " MAC " ttl=%ju hops=%ju | %s", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), (uintmax_t)msg->ttl, (uintmax_t)msg->u8, comment);
    return DESSERT_MSG_DROP;
}

