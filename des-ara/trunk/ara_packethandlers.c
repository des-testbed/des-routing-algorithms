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

ara_seq_t _ara_nextseq = 0;
pthread_mutex_t _ara_nextseq_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t ara_trace_broadcastlen = ARA_TRACE_MIN_LENGTH;
uint8_t ara_defttl = ARA_TTL;
uint8_t ara_rtprob_bants = ARA_RTPROB_BANTS;

size_t ara_ant_size = ARA_ANT_SIZE;

void _ara_sendbant(ara_address_t addr) {
    dessert_msg_t* bant;
    struct ether_header* eth;
    dessert_ext_t*  ext;

    dessert_msg_new(&bant);
    ara_addseq(bant);
    bant->ttl = ara_defttl;
    bant->u8 |= ARA_ANT;

    dessert_msg_addext(bant, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_addext(bant, &ext, ARA_EXT_BANT, ETHER_ADDR_LEN + 4);
    memcpy(ext->data, addr, sizeof(ara_address_t));
    memcpy(ext->data + ETHER_ADDR_LEN, "BANT", 4);

    dessert_msg_dummy_payload(bant, ara_ant_size);

    /* for loop vs duplicate detection */
    dessert_msg_trace_initiate(bant, DESSERT_EXT_TRACE_REQ, DESSERT_MSG_TRACE_HOST);
    dessert_info("sending BANT:\n\tdst=" MAC " seq=%d", EXPLODE_ARRAY6(addr), ntohs(bant->u16));

    ara_maintainroute_stamp(addr);
    dessert_meshsend_fast(bant, NULL);
    dessert_msg_destroy(bant);
}

void _ara_sendfant(ara_address_t addr) {
    dessert_msg_t* fant = NULL;
    dessert_ext_t* ext = NULL;
    struct ether_header* eth;

    /* start discovery */
    dessert_msg_new(&fant);
    ara_addseq(fant);
    fant->ttl = ara_defttl;
    fant->u8 |= ARA_ANT;

    dessert_msg_addext(fant, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_addext(fant, &ext, ARA_EXT_FANT, ETHER_ADDR_LEN + 4);
    memcpy(ext->data, addr, sizeof(ara_address_t));
    memcpy(ext->data + ETHER_ADDR_LEN, "FANT", 4);

    dessert_msg_dummy_payload(fant, ara_ant_size);

    /* for loop vs duplicate detection */
    dessert_msg_trace_initiate(fant, DESSERT_EXT_TRACE_REQ, DESSERT_MSG_TRACE_HOST);
    dessert_info("sending FANT:\n\tdst=" MAC " seq=%d", EXPLODE_ARRAY6(addr), ntohs(fant->u16));

    dessert_meshsend_fast(fant, NULL);
    dessert_msg_destroy(fant);
}

/** add a sequence number to a new ara packet */
ara_seq_t ara_seq_next() {
    ara_seq_t r;
    pthread_mutex_lock(&_ara_nextseq_mutex);
    r = _ara_nextseq++;
    pthread_mutex_unlock(&_ara_nextseq_mutex);
    return(r);
}

/** add a sequence number to a new ara packet */
inline void ara_addseq(dessert_msg_t* msg) {
    msg->u16 = htons(ara_seq_next());
}

/** send packets received via TUN/TAP interface **/
dessert_cb_result ara_tun2ara(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* sysif, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    msg->ttl = ara_defttl;
    ara_addseq(msg);
    ap->flags |= ARA_ORIG_LOCAL;

    /* add trace header if packet is small broadcast/multicast packet
       this helps to abuse such packets for better loop/duplicate detection */
    struct ether_header* eth;
    assert(dessert_msg_getpayload(msg, (void**) &eth));

    if((eth->ether_dhost[0] & 0x01) && len < ara_trace_broadcastlen) {
        dessert_msg_trace_initiate(msg, DESSERT_EXT_TRACE_REQ, DESSERT_MSG_TRACE_HOST);
    }

    return DESSERT_MSG_KEEP;
}

/** forward packets recvieved via ara to tun interface **/
dessert_cb_result ara_ara2tun(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    /* packet for myself - forward to tun */
    if(ap->flags & ARA_LOCAL) {
        struct ether_header* eth;
        size_t eth_len;
        eth_len = dessert_msg_ethdecap(msg, &eth);
        dessert_syssend(eth, eth_len);
        free(eth);
    }

    return DESSERT_MSG_KEEP;
}

/** Drops overheard packets
 *
 * Drop accidentally captured packets in promiscuous mode; only broadcast, multicast, and
 * packets for this host may pass. Some packets can be used as passive
 * acknowledgements.
 */
dessert_cb_result ara_checkl2dst(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    dessert_debug("rx packet:\n\tsrc=" MAC " dst=" MAC " prevhop=" MAC " nexthop=" MAC " seq=%d iface=%s",
                  EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(msg->l2h.ether_shost), EXPLODE_ARRAY6(msg->l2h.ether_dhost), ap->seq, iface_in->if_name);

    // this might happen, e.g., when two transceivers are tuned to the same channel
    if(proc->lflags & DESSERT_RX_FLAG_L2_SRC) {
        extern uint32_t loopprotect_prevhop_self;
        loopprotect_prevhop_self++;
        dessert_debug("ignoring packet sent by myself:\n\tsrc=" MAC " dst=" MAC " prevhop=" MAC " seq=%d iface=%s",
                      EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop), ap->seq, iface_in->if_name);
        return DESSERT_MSG_DROP;
    }

    if((proc->lflags & (DESSERT_RX_FLAG_L25_BROADCAST | DESSERT_RX_FLAG_L25_MULTICAST)
        || proc->lflags & DESSERT_RX_FLAG_L25_DST
        || proc->lflags & DESSERT_RX_FLAG_L2_DST
        || proc->lflags & DESSERT_RX_FLAG_L2_BROADCAST)
       && !(msg->u8 & ARA_ACK_RESPONSE)) {
        return DESSERT_MSG_KEEP;
    }

    // this must be an overheard unicast packet sent or forwarded by a neighbor
    ara_ack_eval_packet(ap->src, ap->seq, msg->l2h.ether_shost);

    return DESSERT_MSG_DROP;
}

dessert_cb_result ara_makeproc_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    return ara_makeproc(msg, len, proc, (dessert_meshif_t*) iface_in, id);
}

/** add ara processing info */
dessert_cb_result ara_makeproc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    struct ether_header* l25h;
    ara_proc_t* ap = ara_proc_get(proc);
    dessert_ext_t* ext;

    if(proc == NULL) {
        return DESSERT_MSG_NEEDMSGPROC;
    }

    /* get l2.5 header if possible */
    l25h = dessert_msg_getl25ether(msg);

    if(l25h != NULL) {
        memcpy(ap->src, l25h->ether_shost, ETHER_ADDR_LEN);
        memcpy(ap->dst, l25h->ether_dhost, ETHER_ADDR_LEN);

        /* flag local generated messages */
        if(ap->flags & ARA_ORIG_LOCAL || memcmp(ap->src, l25h->ether_shost, ETHER_ADDR_LEN) == 0) {
            ap->flags |= ARA_ORIG_LOCAL;
        }
    }
    else {
        /* drop packets without Ethernet extension */
        return DESSERT_MSG_DROP;
    }

    ap->iface_in = iface_in;
    memcpy(ap->prevhop, msg->l2h.ether_shost, ETHER_ADDR_LEN);
    ap->seq = (ara_seq_t) ntohs(msg->u16);

    if(msg->u8 & ARA_ANT) {
        dessert_ext_t* ext_fant;
        dessert_ext_t* ext_bant;

        if(dessert_msg_getext(msg, &ext_fant, ARA_EXT_FANT, 0) > 0) {
            if(memcmp(dessert_l25_defsrc, ext_fant->data, ETHER_ADDR_LEN) == 0) {
                ap->flags |= ARA_FANT_LOCAL;
            }
        }

        if(dessert_msg_getext(msg, &ext_bant, ARA_EXT_BANT, 0) > 0) {
            if(memcmp(dessert_l25_defsrc, ext_bant->data, ETHER_ADDR_LEN) == 0) {
                ap->flags |= ARA_BANT_LOCAL;
            }
        }
    }

    return DESSERT_MSG_KEEP;
}

// add sender data for path classification
dessert_cb_result ara_addext_classification(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    return ara_classification_sender(msg, len, proc, iface_in, id); 
}

// process and update data for path classification
dessert_cb_result ara_updateext_classification(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    return ara_classification_node(msg, len, proc, iface_in, id);
}

dessert_cb_result ara_forward_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != NULL);

    if(!(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
         || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST)) {
        ara_rt_inc_pkt_count(ap->dst, msg->plen);
    }

    return ara_forward(msg, len, proc, (dessert_meshif_t*) iface_in, id);
}

/** Forward a packet
 *
 * Send a packet over the network if a route exists. This callback is used for both types of packets.
 * Locally created packets require a route created by a BANT or other broadcast from the destination.
 * Packets that are received by intermediate nodes are forwarded if a route exists.
 */
dessert_cb_result ara_forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != NULL);

    /* check if a route/nexthop was found */
    if(ap->flags & ARA_DELIVERABLE
       && (ap->flags & ARA_FLOOD || ap->flags & ARA_FORWARD)) {
        if(!(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
             || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST)) {
            switch(ara_ack_mode) {
                case ARA_ACK_DISABLED: {
                    double delta_p;

                    switch(ara_ptrail_mode) {
                        case ARA_PTRAIL_CLASSIC:
                            delta_p = ((double)(msg->ttl)) / ((double) 10);
                            break;
                        case ARA_PTRAIL_LINEAR:
                        case ARA_PTRAIL_CUBIC:
                            delta_p = rt_inc;
                            break;
                        default:
                            assert(0); // should never happen
                            break;
                    }

                    ara_rt_update(ap->dst, ap->nexthop, ap->iface_out, delta_p, ap->seq, 0); // ttl=0 as we do not know the distance
                }
                break;
                case ARA_ACK_PASSIVE:

                    // passive acknowledgements cannot be used on the last hop
                    if(ap->flags & ARA_LAST_HOP) {
                        msg->u8 |= ARA_ACK_REQUEST;
                    }

                    ara_ack_waitfor(ap->src, ap->seq, ap->nexthop, ap->dst, ap->iface_out);
                    break;
                case ARA_ACK_NETWORK:
                    msg->u8 |= ARA_ACK_REQUEST;
                    ara_ack_waitfor(ap->src, ap->seq, ap->nexthop, ap->dst, ap->iface_out);
                    break;
                default:
                    assert(0);
            }
        }

        // the following code modifies the received packet so it can be forwarded; it is afterwards restored (for subsequent callbacks)
        ara_address_t tmp;
        memcpy(tmp, msg->l2h.ether_dhost, sizeof(ara_address_t));
        memcpy(msg->l2h.ether_dhost, ap->nexthop, sizeof(ara_address_t));

        msg->ttl--;

        if(msg->ttl > 0) {
            dessert_meshsend_fast(msg, ap->iface_out);
        }

        msg->ttl++;

        memcpy((msg->l2h.ether_dhost), tmp, sizeof(ara_address_t));
    }

    return (DESSERT_MSG_KEEP);
}

/** Helper function to trap a packet and send a FANT
 *
 */
void _ara_trap_and_fant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    int wp = 0;
    void* dst  = malloc(sizeof(ara_address_t)); // free in ara_routefail_untrap_packets
    memcpy(dst, &(ap->dst), sizeof(ara_address_t));
    wp = trap_packet(dst, msg, len, proc, id);

    if(wp == 1) {
        void* perdata;

        _ara_sendfant(ap->dst);

        perdata  = malloc(sizeof(ara_address_t)); // free in ara_routefail_untrap_packets
        memcpy(perdata, &(ap->dst), sizeof(ara_address_t));

        struct timeval scheduled;
        gettimeofday(&scheduled, NULL);
        TIMEVAL_ADD(&scheduled, 0, ara_retry_delay_ms * 1000);
        dessert_periodic_add(ara_routefail_untrap_packets, perdata, &scheduled, NULL);
    }
}

/** Send FANT if no route is found
 *
 */
dessert_cb_result ara_sendfant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    if(ap->flags & ARA_DELIVERABLE) {
        return DESSERT_MSG_KEEP;
    }

    /* route does not exists - we have to trap the packet and send a FANT*/
    _ara_trap_and_fant(msg, len, proc, id);

    return DESSERT_MSG_DROP;
}

/** Sends network layer ack replies on demand
 *
 * Packets that are received with a set ARA_ACK_REQUEST flag will generate an reply.
 */
dessert_cb_result ara_handle_ack_request(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    // skip if we are not in the right ack mode or the flag is not set
    if(!(ara_ack_mode == ARA_ACK_PASSIVE || ara_ack_mode == ARA_ACK_NETWORK)
       || !(msg->u8 & ARA_ACK_REQUEST)) {
        return DESSERT_MSG_KEEP;
    }

    // send network layer acknowledgement

    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != NULL);

    dessert_msg_t* resp = NULL;
    dessert_ext_t* ext = NULL;
    struct ether_header* eth;

    // create ack response message
    dessert_msg_new(&resp);
    resp->u8 |= ARA_ACK_RESPONSE;
    resp->ttl = 1; // ensure that the packet is never forwarded
    resp->u16 = msg->u16; // required to identify the ack

    // copy the ethernet extension
    memcpy(resp->l2h.ether_dhost, msg->l2h.ether_shost, sizeof(ara_address_t));
    dessert_msg_addext(resp, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, ap->src, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ap->dst, ETHER_ADDR_LEN);
    dessert_info("sending ACK response:\n\tdst=" MAC , EXPLODE_ARRAY6(resp->l2h.ether_dhost));
    dessert_meshsend_fast(resp, iface_in); /// \todo maybe we should query the routing table for the best interface or send over all meshifs?!
    dessert_msg_destroy(resp);

    msg->u8 &= ~ARA_ACK_REQUEST; // reset the flag
    return DESSERT_MSG_KEEP;
}

/** handle route fails */
dessert_cb_result ara_noroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    if(ap->flags & ARA_DELIVERABLE) {
        return DESSERT_MSG_KEEP;
    }

    /* if a packet was received with set route fail flag and no alternative route was found,
    the packet shall be flooded to all neighbors in the hop that there is some alternative path */
    if(msg->u8 & ARA_ROUTEFAIL) {
        // packets returned to the src require special handling
        if(proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
            dessert_warn("packet returned to source due to route fail:\n\tsrc=" MAC " dst=" MAC " seq=%06d",
                         EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq);
            msg->u8 &= ~ARA_ROUTEFAIL;
            _ara_trap_and_fant(msg, len, proc, id);
        }
        else {
            dessert_warn("flooding route fail packet because no alternative route found:\n\t dst=" MAC " src=" MAC " seq=%d",
                         EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq);
            msg->ttl--;

            if(msg->ttl > 0) {
                dessert_meshsend_fast(msg, NULL);
            }
        }

        return DESSERT_MSG_DROP;
    }
    else {
        /* Generate new route fail message if packet is not deliverable.
           This can only happen, when the packet was not received with
           set route fail flag. */
        dessert_msg_t* fmsg;
        struct ether_header* eth;

        dessert_msg_clone(&fmsg, msg, false);

        fmsg->u8 |= ARA_ROUTEFAIL;

        eth = dessert_msg_getl25ether(fmsg);
        memcpy((fmsg->l2h.ether_dhost), ether_broadcast, ETHER_ADDR_LEN);

        dessert_meshsend_fast(fmsg, NULL);
        dessert_warn("flooding route fail packet:\n\tdst=" MAC " src=" MAC " seq=%d", EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq);
        return DESSERT_MSG_DROP;
    }
}

/** Retry to send previously trapped packets
 *
 * Packets that are stored in the packettrap are untrapped after ara_trap_delay_ms. This function tries
 * to lookup a route which could have arrived in the meantime. If there is a route, sent the packet.
 * If there is no route, trap the packet again.
 *
 * This function is only used for locally created packets. Thus if no route is available after the timeout,
 * a FANT should be sent again.
 */
dessert_cb_result ara_retrypacket(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {

    /* These callbacks are the last callbacks that are relevant for nodes to
    forward packets. They are called for trapped packets, as currently there is now way to inject
    the packet back into the callback-pipeline. */

    /// \todo code review required to make sure we are not missing anything

    ara_proc_t* ap = ara_proc_get(proc);

    // have a look, if we finally got a route
    ara_getroute(msg, len, proc, iface_in, id);

    if(ap->flags & ARA_DELIVERABLE) {
        ara_sendbant(msg, len, proc, iface_in, id); // just in case...

        if(!(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
             || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST)) {
            ara_rt_inc_pkt_count(ap->dst, msg->plen);
        }

        ara_forward(msg, len, proc, iface_in, id);

        if(proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
            ara_maintainroute_timestamp(msg, len, proc, (dessert_sysif_t*) iface_in, id);
        }
    }
    else {
        dessert_warn("still no route for:\n\tsrc=" MAC " dst=" MAC " seq=%d", EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq);

        switch(trap_packet(ap->dst, msg, len, proc, id)) {
            case 1: { // it is the only packet trapped for this destination
                _ara_sendfant(ap->dst);

                void* perdata  = malloc(sizeof(ara_address_t)); // free in ara_routefail_untrap_packets
                memcpy(perdata, &(ap->dst), sizeof(ara_address_t));

                struct timeval scheduled;
                gettimeofday(&scheduled, NULL);
                TIMEVAL_ADD(&scheduled, 0, ara_retry_delay_ms * 1000);
                dessert_periodic_add(ara_routefail_untrap_packets, perdata, &scheduled, NULL);
                break;
            }
            case -1: // packet was not trapped
                dessert_warn("could not re-trap packet:\n\tsrc=" MAC " dst=" MAC " seq=%d", EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq);
                break;
            default: // there is at least one other packet in the trap waiting for a route
                break;
        }
    }

    // return value will be currently ignored by untrap_packets
    return 0;
}

/** send a BANT if no ANT has been sent before to the destination */
dessert_cb_result ara_sendbant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    /*
    * Send only a BANT if the routing table entry had a marking
    * and the current packet is no broadcast
    */
    if(ap->flags & ARA_RT_SEND_BANT
       && !(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST)) {
        struct ether_header* l25h = dessert_msg_getl25ether(msg);

        if(l25h != NULL) {
            _ara_sendbant(l25h->ether_dhost);
        }
    }

    return(DESSERT_MSG_KEEP);
}

/** handle incoming forward ant */
dessert_cb_result ara_handle_fant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    if(ap->flags & ARA_FANT_LOCAL
       && !(ap->flags & ARA_DUPLICATE)) {
        dessert_info("received FANT:\n\tdst=" MAC "  src=" MAC "  seq=%d prevhop=" MAC, EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq, EXPLODE_ARRAY6(ap->prevhop));
        _ara_sendbant(ap->src);
    }

    return DESSERT_MSG_KEEP;
}

/** handle packets with set ARA_ROUTEPROBLEM flag */
dessert_cb_result ara_handle_routeproblem(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);

    if(proc->lflags & DESSERT_RX_FLAG_L25_DST && msg->u8 & ARA_ROUTEPROBLEM && ara_rtprob_bants) {
        dessert_warn("sending BANT due to route problem:\n\tdst=" MAC " src=" MAC " prevhop=" MAC , EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->prevhop));
        _ara_sendbant(ap->src);
    }

    return (DESSERT_MSG_KEEP);
}

