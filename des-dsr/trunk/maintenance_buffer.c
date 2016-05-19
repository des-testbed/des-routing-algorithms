/******************************************************************************
 Copyright 2009,  2010, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
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
 ------------------------------------------------------------------------------

 ******************************************************************************/

#include "dsr.h"

dsr_maintenance_buffer_t* dsr_maintenance_buffer = NULL;

pthread_rwlock_t _dsr_maintenance_buffer_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _MB_READLOCK pthread_rwlock_rdlock(&_dsr_maintenance_buffer_rwlock)
#define _MB_WRITELOCK pthread_rwlock_wrlock(&_dsr_maintenance_buffer_rwlock)
#define _MB_UNLOCK pthread_rwlock_unlock(&_dsr_maintenance_buffer_rwlock)

#define _SAFE_RETURN(x) do {_MB_UNLOCK; return(x);} while(0)

/* local forward declarations */
static inline int _remove_all_el_with_link(const uint8_t out_interface[ETHER_ADDR_LEN], const uint8_t msg_dhost[ETHER_ADDR_LEN], dsr_maintenance_buffer_t* dont_delete_mb_el);
static inline dsr_maintenance_buffer_t* _get_msg_by_key(uint16_t id);
static inline void _add_el_to_buffer(dsr_maintenance_buffer_t* mb_el);
static inline void _remove_el_from_buffer(dsr_maintenance_buffer_t* mb_el);
static inline void _readd_el_after_retransmission(dsr_maintenance_buffer_t* mb_el, __suseconds_t timeout);
static inline void _destroy_el(dsr_maintenance_buffer_t* mb_el);
static inline int _is_passive_ack(const dessert_msg_t* candidate_msg, const uint8_t source_src[ETHER_ADDR_LEN], const uint8_t source_dst[ETHER_ADDR_LEN], const uint8_t source_sl, const uint8_t source_hops);

inline int dsr_maintenance_buffer_add_msg(const uint16_t id, dessert_msg_t* msg, const uint8_t in_iface_address[ETHER_ADDR_LEN], const uint8_t out_iface_address[ETHER_ADDR_LEN]) {
    dsr_maintenance_buffer_t* mb_el = NULL;

    _MB_WRITELOCK;

    mb_el = _get_msg_by_key(id);

    if(mb_el != NULL) {
        _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_ERROR_MESSAGE_ALREADY_IN_BUFFER);
    }

    mb_el = malloc(sizeof(dsr_maintenance_buffer_t));
    mb_el->identification = id;
    gettimeofday(&(mb_el->error_threshold), NULL);
    TIMEVAL_ADD(&mb_el->error_threshold, 0, dsr_conf_get_retransmission_timeout());
    mb_el->msg = msg;
    ADDR_CPY(mb_el->out_iface_address, out_iface_address);
    ADDR_CPY(mb_el->in_iface_address, in_iface_address);
    mb_el->retransmission_count = 0;

    _add_el_to_buffer(mb_el);

    _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_SUCCESS);
}

inline int dsr_maintenance_buffer_add_msg_delay(const uint16_t id,
    dessert_msg_t* msg, const uint8_t in_iface_address[ETHER_ADDR_LEN],
    const uint8_t out_iface_address[ETHER_ADDR_LEN], __suseconds_t delay) {

    dsr_maintenance_buffer_t* mb_el = NULL;

    _MB_WRITELOCK;

    mb_el = _get_msg_by_key(id);

    if(mb_el != NULL) {
        _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_ERROR_MESSAGE_ALREADY_IN_BUFFER);
    }

    mb_el = malloc(sizeof(dsr_maintenance_buffer_t));
    mb_el->identification = id;
    gettimeofday(&(mb_el->error_threshold), NULL);
    TIMEVAL_ADD(&mb_el->error_threshold, 0, delay);
    mb_el->msg = msg;
    ADDR_CPY(mb_el->out_iface_address, out_iface_address);
    ADDR_CPY(mb_el->in_iface_address, in_iface_address);
    mb_el->retransmission_count = -1;

    _add_el_to_buffer(mb_el);

    _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_SUCCESS);
}

inline int dsr_maintenance_buffer_delete_msg(const uint16_t id) {
    dsr_maintenance_buffer_t* mb_el = NULL;
    dsr_maintenance_buffer_t* tbd_mb_el = NULL;

    _MB_WRITELOCK;

    mb_el = _get_msg_by_key(id);

    if(mb_el == NULL) {
        dessert_debug("MB: no such msg id(%d)", id);
        _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_ERROR_NO_SUCH_MESSAGE);
    }
    else {
        struct timeval now;
        struct timeval diff;
        gettimeofday(&now, NULL);
        TIMEVAL_SUB(&mb_el->error_threshold, &now, &diff);
        dessert_debug("####\nMB: diff: %i secs %i usecs", diff.tv_sec, diff.tv_usec);

        tbd_mb_el = mb_el;
        _remove_el_from_buffer(mb_el);
        dessert_msg_destroy(tbd_mb_el->msg);
        _destroy_el(tbd_mb_el);
        //		dessert_debug("MB: Removed msg for id (%d)", id);
        _SAFE_RETURN(DSR_MAINTENANCE_BUFFER_SUCCESS);
    }
}

int maintenance_buffer_passive_ack_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    if(dsr_conf_get_routemaintenance_passive_ack() == 0) {
        return DESSERT_MSG_KEEP;
    }

    dessert_ext_t* source_ext;
    dsr_source_ext_t* source;

    if(proc->lflags & DESSERT_RX_FLAG_L2_DST) {
        return DESSERT_MSG_KEEP;
    }

    dessert_debug("PASSIVE_SOURCE[%"PRIi64"]: overheard this msg ...", id);

    /* if there is no source extension in the message, just return */
    if(!dessert_msg_getext(msg, &source_ext, DSR_EXT_SOURCE, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        dessert_debug("PASSIVE_SOURCE[%"PRIi64"]: DSR_EXT_SOURCE in msg ...", id);
        source = (dsr_source_ext_t*) source_ext->data;
    }

    uint8_t source_src[ETHER_ADDR_LEN];
    uint8_t source_dst[ETHER_ADDR_LEN];
    uint8_t source_sl = source->segments_left;
    uint8_t source_hops = dsr_source_get_address_count(source);

    ADDR_CPY(source_src, dsr_source_get_address_begin_by_index(source, 0));
    ADDR_CPY(source_dst, dsr_source_get_address_begin_by_index(source, source_hops - 1));

    _MB_WRITELOCK;

    dsr_maintenance_buffer_t* mb_el = NULL;
    dsr_maintenance_buffer_t* next_mb_el = NULL;

    mb_el = dsr_maintenance_buffer;

    while(mb_el) {
        if(_is_passive_ack(mb_el->msg, source_src, source_dst, source_sl, source_hops) == 0) {
            dessert_debug("PASSIVE_SOURCE[%"PRIi64"]: overheard DSR_EXT_SOURCE in msg ... is a passive ACK", id);
            int removed = _remove_all_el_with_link(mb_el->out_iface_address, mb_el->msg->l2h.ether_dhost, mb_el);
            dessert_debug("PASSIVE_SOURCE[%"PRIi64"]: removed [%i] elements due to the passive ack", id, removed);

            /* cleanup */
            next_mb_el = mb_el->hh.next;
            _remove_el_from_buffer(mb_el);
            dessert_msg_destroy(mb_el->msg);
            _destroy_el(mb_el);

            if(next_mb_el == NULL) {
                break;
            }
        }
        else {
            next_mb_el = mb_el->hh.next;
        }

        mb_el = next_mb_el;
    }

    _MB_UNLOCK;

    return DESSERT_MSG_KEEP;
}

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t cleanup_maintenance_buffer(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_maintenance_buffer_t* mb_el = NULL;
    dsr_maintenance_buffer_t* next_mb_el = NULL;

    dessert_msg_t* msg = NULL;

    struct timeval now;

    _MB_WRITELOCK;

    gettimeofday(&now, NULL);
    int RETRANSMISSION_COUNT = dsr_conf_get_retransmission_count();
    __suseconds_t RETRANSMISSION_TIMEOUT = dsr_conf_get_retransmission_timeout();

    //dessert_info("Maintenance buffer: cleanup.");

    mb_el = dsr_maintenance_buffer;

    while(mb_el) {
        if(TIMEVAL_COMPARE(&now, &mb_el->error_threshold) >= 0) {
            msg = mb_el->msg;

            dessert_debug("MB: now: %i secs %i usecs", now.tv_sec, now.tv_usec);
            dessert_debug("MB: el : %i secs %i usecs", mb_el->error_threshold.tv_sec, mb_el->error_threshold.tv_usec);

            dessert_debug("MB: el is [%i]usecs too late (relative to sched. [%i]usecs)", now.tv_usec - mb_el->error_threshold.tv_usec, scheduled->tv_usec - mb_el->error_threshold.tv_usec);

            if(mb_el->retransmission_count < RETRANSMISSION_COUNT) {
                dessert_debug("MB: id[%i] retransmitting... %d/%d", mb_el->identification, mb_el->retransmission_count + 1, RETRANSMISSION_COUNT);

                /* retransmit the msg*/
                dessert_msg_t* cloned;
                dessert_msg_clone(&cloned, msg, false);

                next_mb_el = mb_el->hh.next;
                _readd_el_after_retransmission(mb_el, RETRANSMISSION_TIMEOUT);

                if(ADDR_CMP(mb_el->in_iface_address, ether_null) == 0) {
                    dsr_statistics_emit_msg(mb_el->out_iface_address, cloned->l2h.ether_dhost, cloned);
                }
                else {
                    dsr_statistics_tx_msg(mb_el->out_iface_address, cloned->l2h.ether_dhost, cloned);
                }

                dessert_meshsend_fast_hwaddr(cloned, mb_el->out_iface_address);
                dessert_msg_destroy(cloned);

                mb_el = next_mb_el;

            }
            else {
                /* retransmission limit reached, emit rerr */

#if (METRIC == ETX)
                if(dsr_unicast_etx_get_value(mb_el->out_iface_address, msg->l2h.ether_dhost) >= 3.0) {
#endif
                    dessert_debug("MB: id[%d] Link error [" MAC "]->[" MAC "]", mb_el->identification, EXPLODE_ARRAY6(mb_el->out_iface_address), EXPLODE_ARRAY6(msg->l2h.ether_dhost));
                    dessert_debug("We got the msg from " MAC , EXPLODE_ARRAY6(msg->l2h.ether_shost));
                    /* dont expect mb_el->msg->l2h.ether_shost to be part of the failed link
                     * because of interface changes, only mb_el->out_iface_address is valid!
                     * mb_el->msg->l2h.ether_dhost is fine, because we set it already before
                     * adding the msg to mb*/
                    dsr_routecache_process_link_error(mb_el->out_iface_address,
                                                      msg->l2h.ether_dhost);

                    int removed = _remove_all_el_with_link(mb_el->out_iface_address, msg->l2h.ether_dhost, mb_el);
                    dessert_debug("MB: removed [%i] elements due to the link error", removed);

                    /* DONE: Blacklist (REPL handling, add entry) rfc4728 p74
                     When using a MAC protocol that requires bidirectional links for
                     unicast transmission, a unidirectional link may be discovered by the
                     propagation of the Route Request.  When the Route Reply is sent over
                     the reverse path, a forwarding node may discover that the next-hop is
                     unreachable.  In this case, it MUST add the next-hop address to its
                     blacklist (Section 4.6). */
                    if(dessert_msg_get_ext_count(msg, DSR_EXT_REPL) > 0) {
                        dsr_blacklist_add_node(msg->l2h.ether_dhost);
                    }

                    if(ADDR_CMP(mb_el->in_iface_address, ether_null) == 0) {
                        /* sys2dsr was sending entity */
                        dessert_debug("MB: Sending entity was sys2dsr.");
                    }
                    else {
                        /* source_meshrx_cb was sending entity */
                        dessert_debug("MB: Sending entity was source_meshrx_cb.");

                        dsr_send_rerr_for_msg(msg, mb_el->in_iface_address,
                                              mb_el->out_iface_address);
                    }

#if (METRIC == ETX)
                }

#endif

                /* cleanup */
                next_mb_el = mb_el->hh.next;
                _remove_el_from_buffer(mb_el);
                _destroy_el(mb_el);
                dessert_msg_destroy(msg);

                if(next_mb_el == NULL) {
                    break;
                }

                mb_el = next_mb_el;
            }

        }
        else if(TIMEVAL_COMPARE(&now, &mb_el->error_threshold) == -1) {
            /* the maintenance buffer is sorted in insertion order*/

            _SAFE_RETURN((0));
        }
    }

    _SAFE_RETURN((0));

}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

static inline int _is_passive_ack(const dessert_msg_t* candidate_msg, const uint8_t source_src[ETHER_ADDR_LEN], const uint8_t source_dst[ETHER_ADDR_LEN], const uint8_t source_sl, const uint8_t source_hops) {
    dessert_ext_t* candidate_source_ext;
    dsr_source_ext_t* candidate_source;

    /* if there is no source extension in the message, just return */
    if(!dessert_msg_getext(candidate_msg, &candidate_source_ext, DSR_EXT_SOURCE, 0)) {
        return -1;
    }
    else {
        candidate_source = (dsr_source_ext_t*) candidate_source_ext->data;
    }

    uint8_t candidate_source_src[ETHER_ADDR_LEN];
    uint8_t candidate_source_dst[ETHER_ADDR_LEN];
    uint8_t candidate_source_sl = candidate_source->segments_left;
    uint8_t candidate_source_hops = dsr_source_get_address_count(candidate_source);

    ADDR_CPY(candidate_source_src, dsr_source_get_address_begin_by_index(candidate_source, 0));
    ADDR_CPY(candidate_source_dst, dsr_source_get_address_begin_by_index(candidate_source, candidate_source_hops - 1));


    if(candidate_source_hops == source_hops
       && ADDR_CMP(candidate_source_src, source_src) == 0
       && ADDR_CMP(candidate_source_dst, source_dst) == 0 && source_sl
       < candidate_source_sl) {

        /* is PASSIVE_ACK */

        return 0;
    }

    return -1;
}

static inline int _remove_all_el_with_link(const uint8_t out_interface[ETHER_ADDR_LEN], const uint8_t msg_dhost[ETHER_ADDR_LEN], dsr_maintenance_buffer_t* dont_delete_mb_el) {
    dsr_maintenance_buffer_t* buffer;
    dsr_maintenance_buffer_t* next_buffer;
    int elements_removed = 0;

    buffer = dsr_maintenance_buffer;

    while(buffer) {
        next_buffer = buffer->hh.next;

        if(buffer != dont_delete_mb_el && ADDR_CMP(buffer->out_iface_address, out_interface) == 0 &&
           ADDR_CMP(buffer->msg->l2h.ether_dhost, msg_dhost) == 0) {

            _remove_el_from_buffer(buffer);
            elements_removed++;
            dessert_msg_destroy(buffer->msg);
            _destroy_el(buffer);

        }

        buffer = next_buffer;
    }

    return elements_removed;
}

static inline void _add_el_to_buffer(dsr_maintenance_buffer_t* mb_el) {
    assert(mb_el != NULL);
    HASH_ADD(hh, dsr_maintenance_buffer, identification, sizeof(uint16_t), mb_el);
}

static inline void _remove_el_from_buffer(dsr_maintenance_buffer_t* mb_el) {
    assert(mb_el != NULL);
    HASH_DELETE(hh, dsr_maintenance_buffer, mb_el);
}

static inline void _destroy_el(dsr_maintenance_buffer_t* mb_el) {
    assert(mb_el != NULL);
    free(mb_el);
    mb_el = NULL;
}

static inline void _readd_el_after_retransmission(
    dsr_maintenance_buffer_t* mb_el, __suseconds_t timeout) {
    _remove_el_from_buffer(mb_el);
    mb_el->retransmission_count++;
    TIMEVAL_ADD_SAFE(&mb_el->error_threshold, 0, timeout);
    _add_el_to_buffer(mb_el);
}

static inline dsr_maintenance_buffer_t* _get_msg_by_key(uint16_t id) {
    dsr_maintenance_buffer_t* mb_el;

    HASH_FIND(hh, dsr_maintenance_buffer, &id, sizeof(id), mb_el);

    return mb_el;
}

