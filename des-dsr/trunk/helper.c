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
#include <string.h>

dsr_rreq_identification_t _dsr_rreq_nextidentification = 0;
pthread_mutex_t _dsr_next_rreq_identification_mutex = PTHREAD_MUTEX_INITIALIZER;

uint16_t _dsr_ackreq_nextidentification = 0;
pthread_mutex_t _dsr_next_ackreq_identification_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void _dsr_new_path_from_patharray_helper(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight, int reversed);

inline void dsr_msg_send_via_path(dessert_msg_t* msg, dsr_path_t* path) {
    int segments_left = (path->len) - 2;

    dsr_source_ext_t* source = dsr_msg_add_source_ext(msg, path, segments_left);
    assert(source != NULL);

    ADDR_CPY(msg->l2h.ether_dhost,  dsr_source_indicated_next_hop_begin(source));

    dessert_meshif_t* outif = dessert_meshif_get_hwaddr(ADDR_IDX(path, 1));

    if(dsr_conf_get_routemaintenance_passive_ack() == 1) {
        if(source->segments_left > 2) {
            dsr_msg_send_with_route_maintenance(msg, NULL, outif, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_PASSIVE);
        }
        else {
            if(dsr_conf_get_routemaintenance_network_ack() == 1) {
                dsr_msg_send_with_route_maintenance(msg, NULL, outif, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
            }
            else {
                dsr_statistics_emit_msg(outif->hwaddr, msg->l2h.ether_dhost, msg);
                int res = dessert_meshsend_fast(msg, outif);
                assert(res == DESSERT_OK);
            }
        }
    }
    else if(dsr_conf_get_routemaintenance_network_ack() == 1) {
        dsr_msg_send_with_route_maintenance(msg, NULL, outif, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
    }
    else {
        dsr_statistics_emit_msg(outif->hwaddr, msg->l2h.ether_dhost, msg);
        int res = dessert_meshsend_fast(msg, outif);
        assert(res == DESSERT_OK);
    }


}

inline void dsr_msg_send_via_path_delay(dessert_msg_t* msg, dsr_path_t* path, __suseconds_t delay) {
    int segments_left = (path->len) - 2;

    dsr_source_ext_t* source = dsr_msg_add_source_ext(msg, path, segments_left);
    assert(source != NULL);

    ADDR_CPY(msg->l2h.ether_dhost,  dsr_source_indicated_next_hop_begin(source));

    dessert_meshif_t* outif = dessert_meshif_get_hwaddr(ADDR_IDX(path, 1));

    if(dsr_conf_get_routemaintenance_passive_ack() == 1) {
        if(source->segments_left > 2) {
            dsr_msg_send_with_route_maintenance_delay(msg, NULL, outif, delay, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_PASSIVE);
        }
        else {

            if(dsr_conf_get_routemaintenance_network_ack() == 1) {
                dsr_msg_send_with_route_maintenance_delay(msg, NULL, outif, delay, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
            }
            else {
                dsr_statistics_emit_msg(outif->hwaddr, msg->l2h.ether_dhost, msg);
                int res = dessert_meshsend_fast(msg, outif);
                assert(res == DESSERT_OK);
            }
        }
    }
    else if(dsr_conf_get_routemaintenance_network_ack() == 1) {
        dsr_msg_send_with_route_maintenance_delay(msg, NULL, outif, delay, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
    }
    else {
        dsr_statistics_emit_msg(outif->hwaddr, msg->l2h.ether_dhost, msg);
        int res = dessert_meshsend_fast(msg, outif);
        assert(res == DESSERT_OK);
    }
}

inline int dsr_msg_send_with_route_maintenance(dessert_msg_t* msg, dessert_meshif_t* in_iface, dessert_meshif_t* out_iface, int nexthop_reachability) {
    int res = 0;

    /* also generate a id for frames for which we only expect
     * passive acknowledgment, because id is the hash key*/
    uint16_t id = dsr_new_ackreq_identification();

    if(nexthop_reachability == DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK) {
        /* add ackreq to msg */
        res = dsr_msg_add_ackreq_ext(msg, id);
        assert(res == DESSERT_OK);
    }

    /* clone the msg*/
    dessert_msg_t* cloned;
    res = dessert_msg_clone(&cloned, msg, false);
    assert(res == DESSERT_OK);

    /* put clone in the route maintenance buffer */
    uint8_t in_iface_address[ETHER_ADDR_LEN];

    if(in_iface) {
        ADDR_CPY(in_iface_address, in_iface->hwaddr);
    }
    else {
        ADDR_CPY(in_iface_address, ether_null);
    }


    res = dsr_maintenance_buffer_add_msg(id, cloned, in_iface_address, out_iface->hwaddr);
    assert(res == DESSERT_OK);

    if(in_iface == NULL) {
        dsr_statistics_emit_msg(out_iface->hwaddr, msg->l2h.ether_dhost, msg);
    }
    else {
        dsr_statistics_tx_msg(out_iface->hwaddr, msg->l2h.ether_dhost, msg);
    }

    /* send msg */
    res = dessert_meshsend_fast(msg, out_iface);
    assert(res == DESSERT_OK);

    return res;
}

inline int dsr_msg_send_with_route_maintenance_delay(dessert_msg_t* msg, const dessert_meshif_t* in_iface, const dessert_meshif_t* out_iface, __suseconds_t delay, int nexthop_reachability) {
    int res = 0;

    /* also generate a id for frames for which we only expect
     * passive acknowledgment, because id is the hash key*/
    uint16_t id = dsr_new_ackreq_identification();

    if(nexthop_reachability == DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK) {
        /* add ackreq to msg */
        res = dsr_msg_add_ackreq_ext(msg, id);
        assert(res == DESSERT_OK);
    }

    /* clone the msg*/
    dessert_msg_t* cloned;
    res = dessert_msg_clone(&cloned, msg, false);
    assert(res == DESSERT_OK);

    /* put clone in the route maintenance buffer */
    uint8_t in_iface_address[ETHER_ADDR_LEN];

    if(in_iface) {
        ADDR_CPY(in_iface_address, in_iface->hwaddr);
    }
    else {
        ADDR_CPY(in_iface_address, ether_null);
    }


    res = dsr_maintenance_buffer_add_msg_delay(id, cloned, in_iface_address, out_iface->hwaddr, delay);
    assert(res == DESSERT_OK);

    /* don't send the msg yet*/

    return res;
}

inline int dsr_send_rerr_for_msg(dessert_msg_t* msg, uint8_t in_iface[ETHER_ADDR_LEN], uint8_t out_iface[ETHER_ADDR_LEN]) {

    assert(msg != NULL);

    dessert_ext_t* msg_source_ext;
    dsr_source_ext_t* msg_source;

    if(dessert_msg_getext(msg, &msg_source_ext, DSR_EXT_SOURCE, 0) < 1) {
        dessert_err("E: No source extension in msg found!");
        return 1;
    }

    msg_source = (dsr_source_ext_t*) msg_source_ext->data;

    dsr_path_t* error_path;
    dsr_path_new_from_reversed_patharray(&error_path, msg_source->address, dsr_source_get_address_count(msg_source), 0);

    dessert_msg_t* error_msg;
    dessert_msg_new(&error_msg);

    dsr_msg_add_rerr_ext(error_msg, 0, out_iface, msg_source->address, msg->l2h.ether_dhost);

    int error_next_hop_index = dsr_path_get_index(msg->l2h.ether_shost, error_path);
    int error_segments_left = dsr_source_get_address_count(msg_source) - error_next_hop_index;

    dessert_debug("\n\terror_next_hop_index (%d)\n"
        "\terror_segments_left (%d)",
        dsr_path_get_index(msg->l2h.ether_shost, error_path),
        dsr_source_get_address_count(msg_source) - error_next_hop_index);

    dsr_path_print_to_debug(error_path);

    dsr_source_ext_t* error_source = dsr_msg_add_source_ext(error_msg, error_path, error_segments_left);

    ADDR_CPY(error_msg->l2h.ether_dhost, dsr_source_indicated_next_hop_begin(error_source));
    dessert_debug("Setting " MAC " as error_msg->l2h.ether_dhost", EXPLODE_ARRAY6(dsr_source_indicated_next_hop_begin(error_source)));
    dsr_statistics_emit_msg(in_iface, error_msg->l2h.ether_dhost, error_msg);
    dessert_meshsend_fast(error_msg, dessert_meshif_get_hwaddr(in_iface));

    free(error_path);
    dessert_msg_destroy(error_msg);

    return 0;
}

inline void dsr_send_repl(dessert_meshif_t* iface, dsr_rreq_ext_t* rreq) {
    dessert_msg_t* repl_msg;
    uint8_t* path;

    dessert_info("sending REPL");

    /* create new msg */
    dessert_msg_new(&repl_msg);

    /* set the l2 destination to the last address in the rreq*/
    ADDR_CPY(repl_msg->l2h.ether_dhost, rreq->data[DSR_RREQ_GET_HOPCOUNT(rreq) - 1].address);

    /* add and initialize REPL extension */
    dsr_msg_add_repl_ext(repl_msg, rreq, iface);

    /* copy path from rreq*/
    path = (uint8_t*) malloc((DSR_RREQ_GET_HOPCOUNT(rreq) + 2) * ETHER_ADDR_LEN);
    int i;

    for(i = 0; i < DSR_RREQ_GET_HOPCOUNT(rreq); i++) {
        ADDR_CPY(path + (i * ETHER_ADDR_LEN), rreq->data[i].address);
    }

    /* add incoming meshif address and own l25 address to path */
    ADDR_CPY(path + (DSR_RREQ_GET_HOPCOUNT(rreq) * ETHER_ADDR_LEN), iface->hwaddr);
    ADDR_CPY(path + ((DSR_RREQ_GET_HOPCOUNT(rreq) + 1) * ETHER_ADDR_LEN), &(dessert_l25_defsrc));

    dsr_path_t* source_path;
    dsr_path_new_from_reversed_patharray(&source_path, path, DSR_RREQ_GET_HOPCOUNT(rreq) + 2, 0);

    /* add source extension with reversed and extended path to repl_msg */
    dsr_msg_add_source_ext(repl_msg, source_path, DSR_RREQ_GET_HOPCOUNT(rreq));

    /* send repl_msg via incoming interface */
    dsr_statistics_emit_msg(iface->hwaddr, repl_msg->l2h.ether_dhost, repl_msg);
    dessert_meshsend_fast(repl_msg, iface);

    /* cleanup */
    dessert_msg_destroy(repl_msg);
    free(source_path);
    free(path);

}

inline void dsr_propagate_rreq(dessert_meshif_t* iface, dessert_msg_t* msg, dsr_rreq_ext_t* rreq, dessert_ext_t* rreq_ext) {
    int res = 0;
    dessert_meshif_t* meshif = NULL;
    dessert_ext_t* new_ext;
    dsr_rreq_ext_t* new_rreq;
    dessert_msg_t* clone;
    dessert_ext_t* clone_old_rreq_ext;

    /*o  DONE: Conceptually create a copy of this entire packet and perform
     the following steps on the copy of the packet.

     o  DONE: Append this node's own IP address to the list of Address[i]
     values in the Route Request and increase the value of the Opt
     Data Len field in the Route Request by 4 (the size of an IP
     address). However, if the node has multiple network
     interfaces, this step MUST be modified by the special
     processing specified in Section 8.4. */

    pthread_rwlock_rdlock(&dessert_cfglock);
    DL_FOREACH(dessert_meshiflist_get(), meshif) {
        dessert_msg_clone(&clone, msg, false);

        dessert_msg_getext(clone, &clone_old_rreq_ext, DSR_EXT_RREQ, 0);
        dessert_msg_delext(clone, clone_old_rreq_ext);

        if(unlikely(meshif == iface)) {
            /* incoming iface == outgoing iface, just append address */
            res = dessert_msg_addext(clone, &new_ext, DSR_EXT_RREQ, dessert_ext_getdatalen(rreq_ext) + sizeof(dsr_hop_data_t));

            assert(res == DESSERT_OK);

            memcpy(new_ext->data, rreq_ext->data, dessert_ext_getdatalen(rreq_ext));

            new_rreq = (dsr_rreq_ext_t*) new_ext->data;
            new_rreq->opt_data_len = rreq->opt_data_len + sizeof(dsr_hop_data_t);

            ADDR_CPY(new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) -1].address, iface->hwaddr);

            uint16_t hop_weight = 0;

#if (METRIC == ETX)
            hop_weight = dsr_etx_encode(dsr_etx_get_value(iface->hwaddr,msg->l2h.ether_shost));
#else
            hop_weight = 100;
#endif

            new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) - 1].weight = htons(hop_weight);
            dsr_statistics_tx_msg(iface->hwaddr, msg->l2h.ether_dhost, msg);
            dessert_meshsend_fast(clone, iface);


        }
        else {

            /* incoming iface != outgoing iface
             *
             *   DONE: When a node with multiple network interfaces that
             *   support DSR propagates a Route Request on a network
             *   interface other than the one on which it received the
             *   Route Request, it MUST in this special case modify
             *   the Address list in the Route Request as follows:
             *
             *   -  Append the node's IP address for the incoming
             *      network interface.
             *
             *   -  Append the node's IP address for the outgoing
             *      network interface. */

            res = dessert_msg_addext(clone, &new_ext, DSR_EXT_RREQ,
                                     dessert_ext_getdatalen(rreq_ext) + (2
                                             * sizeof(dsr_hop_data_t)));
            assert(res == DESSERT_OK);

            memcpy(new_ext->data, rreq_ext->data,
                   dessert_ext_getdatalen(rreq_ext));

            new_rreq = (dsr_rreq_ext_t*) new_ext->data;
            new_rreq->opt_data_len = rreq->opt_data_len + (2
                                     * sizeof(dsr_hop_data_t));

            ADDR_CPY(new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) -2].address, iface->hwaddr);
            ADDR_CPY(new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) -1].address, meshif->hwaddr);

            uint16_t hop_weight = 0;

#if (METRIC == ETX)
            hop_weight = dsr_etx_encode(dsr_etx_get_value(iface->hwaddr,
                                        msg->l2h.ether_shost));
#else
            hop_weight = 100;
#endif

            new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) - 2].weight
            = htons(hop_weight);
            new_rreq->data[DSR_RREQ_GET_HOPCOUNT(new_rreq) - 1].weight
            = htons(0);

            dsr_statistics_tx_msg(meshif->hwaddr, msg->l2h.ether_dhost, msg);
            dessert_meshsend_fast(clone, meshif);
        }

        dessert_msg_destroy(clone);

    }
    pthread_rwlock_unlock(&dessert_cfglock);

}


inline void dsr_do_route_discovery(const uint8_t dest[ETHER_ADDR_LEN]) {
    int ttl = 0;

    ttl = dsr_rreqtable_is_routediscovery_ok_now(dest);

    if(ttl > 0) {
        dsr_send_rreq(dest, dsr_new_rreq_identification(), (uint8_t)ttl);
    }
}

/** Send a rreq to the host identified by
 *  @a dest via all mesh interfaces.
 *
 * @param dest the macaddress of the destination host
 * @param id the rreq identification to use
 *
 * @return DESSERT_OK on success, DESSERT_ERR otherwise
 */
inline int dsr_send_rreq(const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const uint8_t ttl) {
    dessert_meshif_t* meshif;
    dessert_msg_t* msg;
    int res = DESSERT_ERR;
    dessert_info("RD: sending RREQ for " MAC " , identification[%u], TTL[%u] via all mesh interfaces.", EXPLODE_ARRAY6(dest), id, ttl);

    MESHIFLIST_ITERATOR_START(meshif) {
        dessert_msg_new(&msg);
        res = dsr_msg_add_rreq_ext(msg, dest, id, ttl, meshif);

        if(likely(res == DESSERT_OK)) {
            dsr_statistics_emit_msg(meshif->hwaddr, msg->l2h.ether_dhost, msg);
            dessert_meshsend_fast(msg, meshif);
        }
        else {
            dessert_emerg("RD: Could't add extension. res[%i]", res);
            dessert_msg_destroy(msg);
            pthread_rwlock_unlock(&dessert_cfglock);
            return res;
        }

        dessert_msg_destroy(msg);
    }
    MESHIFLIST_ITERATOR_STOP;

    return res;
}

/** Send a rreq piggybacked with the original message to the host identified by
 *  @a dest via all mesh interfaces.
 *
 * @param dest the macaddress of the destination host
 * @param id the rreq identification to use
 * @param msg the message the rreq should be piggybacked to
 *
 * @return DESSERT_OK on success, DESSERT_ERR otherwise
 */
inline int dsr_send_rreq_piggyback(const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const dessert_msg_t* msg, const uint8_t ttl) {
    dessert_meshif_t* meshif;
    dessert_msg_t* cloned;
    int res = DESSERT_ERR;
    dessert_debug("RD: sending RREQ(piggy) for " MAC " , identification [%u], TTL[%u] via all mesh interfaces.", EXPLODE_ARRAY6(dest), id, ttl);

    MESHIFLIST_ITERATOR_START(meshif) {
        dessert_msg_clone(&cloned, msg, false);

        res = dsr_msg_add_rreq_ext(cloned, dest, id, ttl, meshif);

        if(likely(res == DESSERT_OK)) {

            dsr_statistics_emit_msg(meshif->hwaddr, cloned->l2h.ether_dhost, cloned);
            dessert_meshsend_fast(cloned, meshif);
            dessert_msg_destroy(cloned);

        }
        else {
            dessert_emerg("SYS2DSR: Could't add extension. res[%i]", res);
            dessert_msg_destroy(cloned);
            pthread_rwlock_unlock(&dessert_cfglock);
            return res;
        }

    }
    MESHIFLIST_ITERATOR_STOP;
    return res;
}

inline int dsr_msg_add_ackreq_ext(dessert_msg_t* msg, const uint16_t id) {
    dessert_ext_t* ackreq_ext;
    dsr_ackreq_ext_t* ackreq;
    int res = 0;

    res = dessert_msg_addext(msg, &ackreq_ext, DSR_EXT_ACKREQ, DSR_ACKREQ_EXTENSION_HDRLEN);

    if(likely(res == DESSERT_OK)) {
        ackreq = (dsr_ackreq_ext_t*) ackreq_ext->data;
        ackreq->identification = htons(id);
    }

    return res;
}

/** Add a rreq extension to a dessert message.
 *
 * @param msg the message to add the extension to
 * @param dest the mach address of the destination host
 * @param id the rreq identification to use
 * @param meshif the mac address of the interface the msg is going to be send with
 *
 * @return
 */
inline int dsr_msg_add_rreq_ext(dessert_msg_t* msg, const uint8_t dest[ETHER_ADDR_LEN], const uint16_t id, const uint8_t ttl, const dessert_meshif_t* meshif) {
    dessert_ext_t* rreq_ext;
    dsr_rreq_ext_t* rreq;
    int res;

    res = dessert_msg_addext(msg, &rreq_ext, DSR_EXT_RREQ, DSR_RREQ_EXTENSION_HDRLEN + (2 * sizeof(dsr_hop_data_t)));

    if(likely(res == DESSERT_OK)) {
        rreq = (dsr_rreq_ext_t*) rreq_ext->data;
        rreq->opt_data_len = (DSR_RREQ_INITIAL_OPT_DATA_LEN + (2 * sizeof(dsr_hop_data_t)));
        rreq->ttl = ttl;
        rreq->identification = htons(id);

        ADDR_CPY(rreq->target_address, dest);
        ADDR_CPY(rreq->data[0].address, dessert_l25_defsrc);
        ADDR_CPY(rreq->data[1].address, meshif->hwaddr);
        rreq->data[0].weight = htons(0);
        rreq->data[1].weight = htons(0);
    }

    return res;
}


inline dsr_repl_ext_t* dsr_msg_add_repl_ext(dessert_msg_t* repl_msg, dsr_rreq_ext_t* rreq, const dessert_meshif_t* iface) {
    dessert_ext_t* repl_ext;
    dsr_repl_ext_t* repl;

    assert(repl_msg != NULL);
    assert(rreq != NULL);
    assert(iface != NULL);

    int repl_ext_length = DSR_REPL_EXTENSION_HDRLEN 	+ ((DSR_RREQ_GET_HOPCOUNT(rreq) + 2) * sizeof(dsr_hop_data_t)) ;

    int res = dessert_msg_addext(repl_msg, &repl_ext, DSR_EXT_REPL, repl_ext_length);

    if(likely(res == DESSERT_OK)) {
        repl = (dsr_repl_ext_t*) repl_ext->data;
        repl->opt_data_len = DSR_REPL_INITIAL_OPT_DATA_LEN + ((DSR_RREQ_GET_HOPCOUNT(rreq) + 2) * sizeof(dsr_hop_data_t));

        /* copy hop data to repl and add incoming meshif and own l25 data */
        int i = 0;

        for(; i < DSR_RREQ_GET_HOPCOUNT(rreq); i++) {
            memcpy(&repl->data[i], &rreq->data[i], sizeof(dsr_hop_data_t));
        }

        ADDR_CPY(repl->data[i].address, iface->hwaddr);
        ADDR_CPY(repl->data[i+1].address, &(dessert_l25_defsrc));

        uint16_t last_hop_weight;
#if (METRIC == ETX)
        last_hop_weight = dsr_etx_encode(dsr_etx_get_value(iface->hwaddr, repl_msg->l2h.ether_dhost));
#else
        last_hop_weight = 100;
#endif

        repl->data[i].weight = htons(last_hop_weight);
        repl->data[i+1].weight = htons(0);

        return repl;
    }

    return NULL;
}

inline dsr_rerr_ext_t* dsr_msg_add_rerr_ext(dessert_msg_t* msg,
        const uint8_t salvage, const uint8_t error_source_address[ETHER_ADDR_LEN],
        const uint8_t error_destination_address[ETHER_ADDR_LEN],
        const uint8_t type_specific_information[6]) {
    dessert_ext_t* rerr_ext;
    dsr_rerr_ext_t* rerr;

    assert(msg != NULL);

    int res = dessert_msg_addext(msg, &rerr_ext, DSR_EXT_RERR, DSR_RERR_EXTENSION_HDRLEN);

    if(likely(res == DESSERT_OK)) {
        rerr = (dsr_rerr_ext_t*) rerr_ext->data;

        rerr->salvage = salvage;
        ADDR_CPY(rerr->error_source_address, error_source_address);
        ADDR_CPY(rerr->error_destination_address, error_destination_address);
        ADDR_CPY(rerr->type_specific_information, type_specific_information);
        dessert_debug("Added RERR to msg\n"
            "\t error_source_address\t\t" MAC "\n"
            "\t error_destination_address\t" MAC "\n"
            "\t type_specific_information\t" MAC , EXPLODE_ARRAY6(rerr->error_source_address), EXPLODE_ARRAY6(rerr->error_destination_address), EXPLODE_ARRAY6(rerr->type_specific_information));
        return rerr;
    }

    return NULL;
}

inline dsr_source_ext_t* dsr_msg_add_source_ext(dessert_msg_t* msg, dsr_path_t* path, int segments_left) {
    dessert_ext_t* source_ext;
    dsr_source_ext_t* source;

    assert(msg != NULL);
    assert(path != NULL);

    int res = dessert_msg_addext(msg, &source_ext, DSR_EXT_SOURCE, DSR_SOURCE_EXTENSION_HDRLEN + (path->len * ETHER_ADDR_LEN));

    if(likely(res == DESSERT_OK)) {
        source = (dsr_source_ext_t*) source_ext->data;
        ADDR_N_CPY(source->address, path->address, path->len);
        source->opt_data_len = DSR_SOURCE_INITIAL_OPT_DATA_LEN + (path->len * ETHER_ADDR_LEN);
        source->segments_left = segments_left;
        source->salvage = 0;
        source->flags = 0;

        return source;
    }

    return NULL;
}

#if (LINKCACHE == 0)
inline void dsr_msg_cache_repl_ext_to_routecache(dsr_repl_ext_t* repl) {
    assert(repl != NULL);

    dsr_path_t* path;
    uint32_t path_weight = 0;

    path = malloc(sizeof(dsr_path_t));

    int i = 0;

    for(; i < DSR_REPL_GET_HOPCOUNT(repl); i++) {
        ADDR_CPY(ADDR_IDX(path, i), repl->data[i].address);
        path_weight += ntohs(repl->data[i].weight);
    }

    path->len = DSR_REPL_GET_HOPCOUNT(repl);
    path->weight = path_weight;
    path->next = NULL;
    path->prev = NULL;
    dessert_info("Caching REPL with [%u] hops and weight[%u] for dest[" MAC "] to routecache ", DSR_REPL_GET_HOPCOUNT(repl), path->weight, EXPLODE_ARRAY6(ADDR_IDX(path, i - 1)));
    dsr_routecache_add_path(ADDR_IDX(path, i - 1), path);
}
#endif

#if (CACHE_FROM_SOURCE_EXT == 1)
inline void dsr_msg_cache_source_ext_to_linkcache(dsr_source_ext_t* source, int repl_present) {
    assert(repl_present == DSR_REPL_EXTENSION_IN_MSG || repl_present == DSR_NO_REPL_EXTENSION_IN_MSG);
    assert(source != NULL);

    int links_to_cache = 0;
    int linkcache_dirty = 0;
    int cache_index;

    if(repl_present == DSR_REPL_EXTENSION_IN_MSG) {
        links_to_cache = dsr_source_get_address_count(source) - source->segments_left;
    }
    else {
        links_to_cache = dsr_source_get_address_count(source) - 1;
    }

    for(cache_index = 0; cache_index < links_to_cache; cache_index++) {

        if(dsr_linkcache_add_link(ADDR_IDX(source, cache_index), ADDR_IDX(source, cache_index + 1), 1) == DSR_LINKCACHE_SUCCESS) {
            linkcache_dirty = 1;
        }

        if(dsr_linkcache_add_link(ADDR_IDX(source, cache_index + 1), ADDR_IDX(source, cache_index), 1) == DSR_LINKCACHE_SUCCESS) {
            linkcache_dirty = 1;
        }
    }

    if(linkcache_dirty) {
        dsr_linkcache_run_dijkstra(dessert_l25_defsrc);
    }
}
#endif

#if (LINKCACHE == 1)
inline void dsr_msg_cache_repl_ext_to_linkcache(dsr_repl_ext_t* repl) {
    assert(repl != NULL);

    uint32_t weight = 0;
    int linkcache_dirty = 0;

    int i = 1;

    for(; i < DSR_REPL_GET_HOPCOUNT(repl); i++) {
        if(dsr_linkcache_add_link(repl->data[i-1].address, repl->data[i].address, ntohs(repl->data[i].weight)) == DSR_LINKCACHE_SUCCESS) {
            linkcache_dirty = 1;
        }

        if(dsr_linkcache_add_link(repl->data[i].address, repl->data[i-1].address, ntohs(repl->data[i].weight)) == DSR_LINKCACHE_SUCCESS) {
            linkcache_dirty = 1;
        }

        weight += ntohs(repl->data[i].weight);
    }

    if(linkcache_dirty) {
        dsr_linkcache_run_dijkstra(dessert_l25_defsrc);
    }

    dessert_info("Cached REPL with [%u] hops and weight [%u]", DSR_REPL_GET_HOPCOUNT(repl), weight);
}
#endif

inline dsr_rreq_identification_t dsr_new_rreq_identification() {
    dsr_rreq_identification_t id;
    pthread_mutex_lock(&_dsr_next_rreq_identification_mutex);
    id = _dsr_rreq_nextidentification++;
    pthread_mutex_unlock(&_dsr_next_rreq_identification_mutex);
    return (id);
}

inline uint16_t dsr_new_ackreq_identification() {
    uint16_t id;
    pthread_mutex_lock(&_dsr_next_ackreq_identification_mutex);
    id = _dsr_ackreq_nextidentification++;
    pthread_mutex_unlock(&_dsr_next_ackreq_identification_mutex);
    return (id);
}

inline int dsr_patharray_get_index(uint8_t u[ETHER_ADDR_LEN], uint8_t* path, int path_len) {
    assert(path_len > 0);

    int i;

    for(i = 0; i < path_len; i++) {
        if(ADDR_CMP(u, path + (i * ETHER_ADDR_LEN)) == 0) {
            return i;
        }
    }

    assert(i + 1 == path_len);
    return -1;
}

inline int dsr_patharray_contains_link(uint8_t* path, int path_len, const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    assert(path_len > 0);

    int i;

    for(i = 0; i < path_len; i++) {
        /* does the current address match u?*/
        if(ADDR_CMP(u, path + (i * ETHER_ADDR_LEN)) == 0) {
            /* is there one more address in the path ? */
            if(i + 1 < path_len) {
                /* does the next address match v? */
                if(ADDR_CMP(v, path + ((i + 1) * ETHER_ADDR_LEN)) == 0) {
                    return DSR_PATH_LINK_FOUND;
                }
                else {
                    /* u can't be twice in the path, so the link <u,v> is not contained in the path*/
                    return DSR_PATH_NO_SUCH_LINK;
                }
            }
        }
    }

    return DSR_PATH_NO_SUCH_LINK;
}

inline int dsr_path_get_index(uint8_t u[ETHER_ADDR_LEN], dsr_path_t* path) {
    assert(path != NULL);
    return dsr_patharray_get_index(u, path->address, path->len);
}

inline int dsr_path_contains_link(dsr_path_t* path, const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    assert(path != NULL);
    return dsr_patharray_contains_link(path->address, path->len, u, v);
}

inline int dsr_path_is_linkdisjoint(dsr_path_t* p1, dsr_path_t* p2) {
    assert(p1 != NULL);
    assert(p2 != NULL);

    int common_link = DSR_PATH_NO_SUCH_LINK;

    int i;

    /* starting at i=2 because we ignore the first 'internal' link */
    for(i = 2; i < p1->len; i++) {
        if(dsr_path_contains_link(p2, ADDR_IDX(p1, i - 1), ADDR_IDX(p1, i)) == DSR_PATH_LINK_FOUND) {
            common_link = DSR_PATH_LINK_FOUND;
            dessert_debug("common link: [" MAC "]-[" MAC "]", EXPLODE_ARRAY6(ADDR_IDX(p1, i - 1)), EXPLODE_ARRAY6(ADDR_IDX(p1, i)));
            break;
        }
    }

    return (common_link == DSR_PATH_LINK_FOUND ? DSR_PATH_LINK_FOUND : DSR_PATH_NO_SUCH_LINK);
}

inline void dsr_patharray_reverse(uint8_t* to_revd, uint8_t* from_orig, int path_len) {
    assert(path_len > 0);

    int orig_index = path_len - 1;
    int revd_index = 0;

    while(orig_index >= 0) {
        ADDR_CPY(to_revd + (revd_index * ETHER_ADDR_LEN),
                 from_orig + (orig_index * ETHER_ADDR_LEN));
        revd_index++;
        orig_index--;
    }

    assert(revd_index == path_len);
}

inline void dsr_path_reverse(dsr_path_t* to_revd, dsr_path_t* from_orig) {
    assert(to_revd != NULL);
    assert(from_orig != NULL);

    dsr_patharray_reverse(to_revd->address, from_orig->address, from_orig->len);
    to_revd->len = from_orig->len;
}

inline void dsr_path_new_from_rreq(dsr_path_t** path, dsr_rreq_ext_t* rreq) {
    *path = malloc(sizeof(dsr_path_t));
    memset((*path)->address, 0, DSR_SOURCE_MAX_ADDRESSES_IN_OPTION * ETHER_ADDR_LEN);
    assert(*path != NULL);

    (*path)->next = NULL;
    (*path)->prev = NULL;
    (*path)->len = DSR_RREQ_GET_HOPCOUNT(rreq);
    (*path)->weight = dsr_rreq_get_weight(rreq);

    int i;

    for(i = 0; i < DSR_RREQ_GET_HOPCOUNT(rreq); i++) {
        ADDR_CPY((*path)->address + (i * ETHER_ADDR_LEN), rreq->data[i].address);
        dessert_debug("%i copied path[] rreq[" MAC "]", i, EXPLODE_ARRAY6((*path)->address) + (i * ETHER_ADDR_LEN) , EXPLODE_ARRAY6(rreq->data[i].address));
    }
}

inline void dsr_path_new_from_patharray(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight) {
    _dsr_new_path_from_patharray_helper(path, from_orig, path_len, path_weight, 0);
}

inline void dsr_path_new_from_reversed_patharray(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight) {
    _dsr_new_path_from_patharray_helper(path, from_orig, path_len, path_weight, 1);
}

inline uint32_t dsr_rreq_get_weight_incl_hop_to_self(const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq) {
    assert(iface != NULL);
    assert(rreq != NULL);

    uint32_t weight;
    uint16_t hop_weight = 0;

    weight = dsr_rreq_get_weight(rreq);

#if (METRIC == ETX)
    hop_weight = dsr_etx_encode(dsr_etx_get_value(iface->hwaddr, rreq->data[DSR_RREQ_GET_HOPCOUNT(rreq)-1].address));
#else
    hop_weight = 100;
#endif

    weight += hop_weight;

    return weight;
}

inline uint32_t dsr_rreq_get_weight(dsr_rreq_ext_t* rreq) {
    assert(rreq != NULL);

    uint32_t weight = 0;

    int i;

    for(i = 1; i < DSR_RREQ_GET_HOPCOUNT(rreq); i++) {
        weight += ntohs(rreq->data[i].weight);
    }

    return weight;
}

inline void dsr_path_print_to_debug(dsr_path_t* path) {
    char nodes_string[(path->len * ETHER_ADDR_LEN) + (21* path->len) + 1];
    memset(nodes_string, 0, (path->len * ETHER_ADDR_LEN) + (21 * path->len) + 1);

    char* node;
    int i = 0;

    for(; i < path->len; i++) {
        asprintf(&node, "\n\t%d " MAC , i ,  EXPLODE_ARRAY6(ADDR_IDX(path, i)));
        strcat(nodes_string, node);
        free(node);
    }

    dessert_debug("Path: len(%d) weight[%i] %s", path->len, path->weight, nodes_string);

}

static inline void _dsr_new_path_from_patharray_helper(dsr_path_t** path, uint8_t* from_orig, int path_len, uint32_t path_weight, int reversed) {
    assert(path_len > 0);
    assert(path_weight >= 0);
    assert(from_orig != NULL);

    *path = malloc(sizeof(dsr_path_t));
    memset((*path)->address, 0, DSR_SOURCE_MAX_ADDRESSES_IN_OPTION * ETHER_ADDR_LEN);
    assert(*path != NULL);

    (*path)->next = NULL;
    (*path)->prev = NULL;
    (*path)->len = path_len;
    (*path)->weight = path_weight;

    if(reversed) {
        dsr_patharray_reverse((*path)->address, from_orig, path_len);
    }
    else {
        ADDR_N_CPY((*path)->address, from_orig, path_len);
    }
}

inline int dsr_path_cmp(dsr_path_t* p1, dsr_path_t* p2) {
    assert(p1 != NULL);
    assert(p2 != NULL);

    if(p1->weight < p2->weight) {
        return -1;    /* sort p1 before p2 */
    }

    if(p1->weight > p2->weight) {
        return +1;    /* sort p1 after p2*/
    }

    /* p1->weight == p2->weight */

    if(p1->len < p2->len) {
        return -1;
    }

    if(p1->len > p2->len) {
        return +1;
    }

    return 0;
}

inline int dsr_path_hops_ident(dsr_path_t* p1, dsr_path_t* p2) {
    assert(p1->len == p2->len);

    int i;

    for(i = 0; i < p1->len; i++) {
        /* does the current address match u?*/
        if(ADDR_CMP(ADDR_IDX(p1, i), ADDR_IDX(p2, i)) != 0) {
            return 1;
        }
    }

    return 0;
}

