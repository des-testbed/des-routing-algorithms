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

 general TODOs:




 ******************************************************************************/

#include "dsr.h"
#include "build.h"


struct cli_command* cli_cfg_set;
struct cli_command* cli_exec_info;

/* RREQ-identification*/
/* TODO_ENHANCEMENT: choose initial dsr_rreq_identification randomly
 * "...a node after rebooting SHOULD base its initial
 *  Identification value on a random number." rfc4728 p31 */


#define DROP_ON_ERROR(action) if ((action) < 0) return DESSERT_MSG_DROP


/* local forward declarations */


/******************************************************************************
 *
 * General routines --
 *
 ******************************************************************************/





/******************************************************************************
 *
 * C L I --
 *
 ******************************************************************************/




/******************************************************************************
 *
 * sysrxcb callbacks --
 *
 ******************************************************************************/

int sys2dsr_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* sysif, dessert_frameid_t id) {

    struct ether_header* eth = NULL;
    dsr_path_t path;
    int res;

    if(dessert_msg_ethdecap(msg, &eth) == -1) {
        dessert_err("SYS2DSR[%"PRIi64"]: Couldn't decapsulate l25 ethernet header!", id);
    }

    /* TODO_CORE: Originating a Packet rfc4728 p56

     When originating any packet, a node using DSR routing MUST perform
     the following sequence of steps: */

    /*DONE: Route Cache (originating a packet, search cache) rfc4728 p56
     - Search the node's Route Cache for a route to the address given in
     the IP Destination Address field in the packet's header. */

    /* never try to lookup a route to the broadcast address*/
    if(ADDR_CMP(eth->ether_dhost, ether_broadcast) == 0) {
        res = -1;
    }
    else {
#if (LOAD_BALANCING == 0)
        res = dsr_routecache_get_first(eth->ether_dhost, &path);
#elif (LOAD_BALANCING == 1)
        res = dsr_routecache_get_next_round_robin(eth->ether_dhost, &path);
#endif
    }

    if(res == -1) {
        /* destination is broadcast address */

        dsr_send_rreq_piggyback(ether_broadcast, dsr_new_rreq_identification(), msg, 255);

    }
    else 	if(unlikely(res == DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION)) {
        /* NO route found in cache */

        dessert_debug("SYS2DSR[%"PRIi64"]: NO route in cache found, initiating Route Discovery...", id);

        /* DONE: Perform Route Discovery (originating a packet, originate RREQ) rfc4728 p56
         TODO_CORE: Send Buffer (originating a packet, save packet) rfc4728 p56
         -  If no such route is found in the Route Cache, then perform Route
         Discovery for the Destination Address, as described in Section
         8.2.  Initiating a Route Discovery for this target node address
         results in the node adding a Route Request option in a DSR Options
         header in this existing packet, or saving this existing packet to
         its Send Buffer and initiating the Route Discovery by sending a
         separate packet containing such a Route Request option.  If the
         node chooses to initiate the Route Discovery by adding the Route
         Request option to this existing packet, it will replace the IP
         Destination Address field with the IP "limited broadcast" address
         (255.255.255.255) [RFC1122], copying the original IP Destination
         Address to the Target Address field of the new Route Request
         option added to the packet, as described in Section 8.2.1. */

        dessert_msg_t* clone = NULL;
        dessert_msg_clone(&clone, msg, false);
        dsr_sendbuffer_add(eth->ether_dhost, clone);
        dsr_do_route_discovery(eth->ether_dhost);

    }
    else {   // route in cache

        dessert_debug("SYS2DSR[%"PRIi64"]: route in cache found len[%i] weight[%u]:", id, path.len, path.weight);

        /*- If the length of this route is greater than 1 hop, or if the node
         determines to request a DSR network-layer acknowledgment from the
         first-hop node in that route, then insert a DSR Options header
         into the packet, as described in Section 8.1.2, and insert a DSR
         Source Route option, as described in Section 8.1.3.  The source
         route in the packet is initialized from the selected route to the
         Destination Address of the packet.*/

        dsr_msg_send_via_path(msg, &path);

        /* DONE: Route Maintenance (originating a packet) rfc4728 p57
         -  Transmit the packet to the first-hop node address given in
         selected source route, using Route Maintenance to determine the
         reachability of the next hop, as described in Section 8.3. */
    }

    free(eth);

    return DESSERT_MSG_DROP;
}

/******************************************************************************
 *
 * meshrxcb callbacks --
 *
 ******************************************************************************/

int debug_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_meshif_t* shost_if = dessert_meshif_get_hwaddr(msg->l2h.ether_shost);
    dessert_meshif_t* dhost_if = dessert_meshif_get_hwaddr(msg->l2h.ether_dhost);

    char* shost_if_name;
    char* dhost_if_name;

    if(shost_if == NULL) {
        shost_if_name = "????\0";
    }
    else {
        shost_if_name = shost_if->if_name;
    }

    if(dhost_if == NULL) {
        dhost_if_name = "????\0";
    }
    else {
        dhost_if_name = dhost_if->if_name;
    }

    dessert_debug(
        "DEBUG[%"PRIi64"]\n"
        "               receiving iface [%s]\n"
        "               proc->lflags:\n"
        "                 DESSERT_RX_FLAG_L2_SRC           [%d]\n"
        "                 DESSERT_RX_FLAG_L25_SRC          [%d]\n"
        "                 DESSERT_RX_FLAG_L25_DST          [%d]\n"
        "                 DESSERT_RX_FLAG_L2_DST           [%d]\n"
        "                 DESSERT_RX_FLAG_L25_OVERHEARD    [%d]\n"
        "                 DESSERT_RX_FLAG_L2_OVERHEARD     [%d]\n"
        "                 DESSERT_RX_FLAG_L25_BROADCAST    [%d]\n"
        "                 DESSERT_RX_FLAG_L25_MULTICAST    [%d]\n"
        "                 DESSERT_RX_FLAG_L2_BROADCAST     [%d]\n"
        "               msg->l2h\n"
        "                 ether_shost [" MAC "][%s]\n"
        "                 ether_dhost [" MAC "][%s]\n"
        , id, iface->if_name,
        proc->lflags & DESSERT_RX_FLAG_L2_SRC,
        proc->lflags & DESSERT_RX_FLAG_L25_SRC,
        proc->lflags & DESSERT_RX_FLAG_L25_DST,
        proc->lflags & DESSERT_RX_FLAG_L2_DST,
        proc->lflags & DESSERT_RX_FLAG_L25_OVERHEARD,
        proc->lflags & DESSERT_RX_FLAG_L2_OVERHEARD,
        proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST,
        proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST,
        proc->lflags & DESSERT_RX_FLAG_L2_BROADCAST,
        EXPLODE_ARRAY6(msg->l2h.ether_shost), shost_if_name,
        EXPLODE_ARRAY6(msg->l2h.ether_dhost), dhost_if_name
    );

    return DESSERT_MSG_KEEP;
}

int loopcheck_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {

    if(proc->lflags & DESSERT_RX_FLAG_L2_SRC || proc->lflags
       & DESSERT_RX_FLAG_L25_SRC) {
        //dessert_debug("LOOPCHECK[%"PRIi64"]: dropping this msg. i send it myself!", id);
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

int rreq_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* rreq_ext = NULL;
    dsr_rreq_ext_t* rreq = NULL;

    int i;
    dessert_meshif_t* meshif = NULL;

    int res;

    /* if there is no rreq extension in the message, just return */
    if(!(dessert_msg_getext(msg, &rreq_ext, DSR_EXT_RREQ, 0))) {
        return DESSERT_MSG_KEEP;
    }
    else {
        rreq = (dsr_rreq_ext_t*) rreq_ext->data;
        //		dessert_debug("RREQ[%"PRIi64"]: DSR_EXT_RREQ in msg. target_address[%M] initiator[%M] ttl[%i] id[%i] hops[%i] weight[%u]", id, rreq->target_address,rreq->data[0].address, rreq->ttl, ntohs(rreq->identification), DSR_RREQ_GET_HOPCOUNT(rreq)+1, dsr_rreq_get_weight_incl_hop_to_self(iface, rreq));
    }

    assert(rreq != NULL);

    /* If the DSR Options header contains a Route Request option, the
     node SHOULD extract the source route from the Route Request and
     add this routing information to its Route Cache, subject to the
     conditions identified in Section 3.3.1.

     RF4728 states in Section 3.3.1 on p15, that only SOURCE options
     should be cached! */

    if(ADDR_CMP(rreq->target_address, &dessert_l25_defsrc) == 0) {
        /* we ARE the target of the rreq */
        dessert_debug("RREQ[%"PRIi64"]: We [" MAC "] are the target [" MAC "] of the RREQ; "
            "initiator[" MAC "] ttl[%i] id[%i] hops[%i] weight[%u]",
            id,
            EXPLODE_ARRAY6(&dessert_l25_defsrc),
            EXPLODE_ARRAY6(rreq->target_address),
            EXPLODE_ARRAY6(rreq->data[0].address),
            rreq->ttl,
            ntohs(rreq->identification),
            DSR_RREQ_GET_HOPCOUNT(rreq) + 1,
            dsr_rreq_get_weight_incl_hop_to_self(iface, rreq));

#if (PROTOCOL == MDSR_PROTOKOLL_1)
        dsr_path_t* rreq_path;
        dsr_path_new_from_rreq(&rreq_path, rreq);
        res = dsr_mdsr_is_repl_ok(rreq->data[0].address, ntohs(rreq->identification), rreq->target_address, rreq_path);
        if(res != DSR_MDSR_REPLY_OK) {
            dessert_debug("RREQ[%"PRIi64"]: Dropping due to MDSR rules...", id);
            return DESSERT_MSG_DROP;
        }
        else {
            dessert_debug("RREQ[%"PRIi64"]: Replying according to MDSR rules...", id);
        }

#endif

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
        res = dsr_smr_is_repl_ok(rreq->data[0].address, ntohs(rreq->identification), rreq->target_address, iface, rreq);

        if(res != DSR_SMR_REPLY_OK) {
            dessert_debug("RREQ[%"PRIi64"]: Not replying due to SMR rules...", id);
            return DESSERT_MSG_DROP;
        }
        else {
            dessert_debug("RREQ[%"PRIi64"]: Replying according to SMR rules...", id);
        }

#endif

        /* DONE: Send REPL (RREQ handling) rfc4728 p66
         If the Target Address field in the Route Request matches this
         node's own IP address, then the node SHOULD return a Route Reply
         to the initiator of this Route Request (the Source Address in the
         IP header of the packet), as described in Section 8.2.4.  The
         source route for this Reply is the sequence of hop addresses

         initiator, Address[1], Address[2], ..., Address[n], target

         where initiator is the address of the initiator of this Route
         Request, each Address[i] is an address from the Route Request, and
         target is the target of the Route Request (the Target Address
         field in the Route Request).  The value n here is the number of
         addresses recorded in the Route Request, or
         (Opt Data Len - 6) / 4. */
        dsr_send_repl(iface, rreq);


        /* DONE: rfc4728 p67
         The node MUST NOT process the Route Request option further and
         MUST NOT retransmit the Route Request to propagate it to other
         nodes as part of the Route Discovery. */

        return DESSERT_MSG_KEEP;

    }
    else {   /* we are NOT the target of the rreq */
        dessert_debug("RREQ[%"PRIi64"]: We [" MAC "] are NOT the target [" MAC "] of the RREQ; initiator[" MAC "] ttl[%i] id[%i] hops[%i] weight[%u]",
            id,
            EXPLODE_ARRAY6(&dessert_l25_defsrc),
            EXPLODE_ARRAY6(rreq->target_address),
            EXPLODE_ARRAY6(rreq->data[0].address),
            rreq->ttl,
            ntohs(rreq->identification),
            DSR_RREQ_GET_HOPCOUNT(rreq) + 1,
            dsr_rreq_get_weight_incl_hop_to_self(iface, rreq));
        /* check TTL */
        rreq->ttl = rreq->ttl - 1;

        if(rreq->ttl == 0) {
            dessert_info("RREQ[%"PRIi64"]: TTL exceeded! ", id);
            return DESSERT_MSG_DROP;
        }

        if(proc->lflags & DESSERT_RX_FLAG_SPARSE) {
            dessert_debug("RREQ[%"PRIi64"]: msg is not sparse. Requesting NOSPARSE...", id);
            return DESSERT_MSG_NEEDNOSPARSE;
        }

        /* DONE: rfc4728 p 67
         Else, the node MUST examine the route recorded in the Route
         Request option (the IP Source Address field and the sequence of
         Address[i] fields) to determine if this node's own IP address
         already appears in this list of addresses.  If so, the node MUST
         discard the entire packet carrying the Route Request option.*/
        int address_already_in_list = 0;
        pthread_rwlock_rdlock(&dessert_cfglock);
        DL_FOREACH(dessert_meshiflist_get(), meshif) {
            /* checks against interfaces in list */
            for(i = 0; i < DSR_RREQ_GET_HOPCOUNT(rreq); i++) {
                if(ADDR_CMP(rreq->data[i].address, meshif->hwaddr) == 0) {
                    dessert_debug("RREQ[%"PRIi64"]: We are already in the address list of this RREQ", id);
                    address_already_in_list = 1;
                    break;
                }
            }

            if(address_already_in_list) {
                break;
            }
        }
        pthread_rwlock_unlock(&dessert_cfglock);

        if(address_already_in_list) {
            /* dropping the entire msg as stated in rfc4728 p67*/
            return DESSERT_MSG_DROP;
        }


        int blacklist_state = dsr_blacklist_get_state(msg->l2h.ether_shost);

        if(blacklist_state == DSR_BLACKLIST_FLAG_PROBABLE) {
            /* DONE: Blacklist (RREQ handling, search entry) rfc4728 p67
             Else, if the Route Request was received through a network
             interface that requires physically bidirectional links for unicast
             transmission, the node MUST check if the Route Request was last
             forwarded by a node on its blacklist (Section 4.6).  If such an
             entry is found in the blacklist, and the state of the
             unidirectional link is "probable", then the Request MUST be
             silently discarded. */
            dessert_info("REQ[%"PRIi64"]: Dropping this RREQ, msg->l2h.ether_shost is in BLACKLIST", id);
            return DESSERT_MSG_DROP;
        }
        else if(blacklist_state == DSR_BLACKLIST_FLAG_QUESTIONABLE) {
            /* DONE: Blacklist (RREQ handling, search entry) rfc4728 p67
             Else, if the Route Request was received through a network
             interface that requires physically bidirectional links for unicast
             transmission, the node MUST check if the Route Request was last
             forwarded by a node on its blacklist.

             RFC_UNCLEAR: unicast Route Request, why not ackreq and ack ???
             TODO_ENHANCEMENT: unicast Route Request (RREQ handling, blacklist) rfc4728 p67
             If such an entry is found  in the blacklist, and the state of the
             unidirectional link is "questionable", then the node MUST create
             and unicast a Route Request packet to that previous node, setting
             the IP Time-To-Live (TTL) to 1 to prevent the Request from being
             propagated.  If the node receives a Route Reply in response to the
             new Request, it MUST remove the blacklist entry for that node, and
             SHOULD continue processing.  If the node does not receive a Route
             Reply within some reasonable amount of time, the node MUST silently
             discard the Route Request packet. */

            //return DESSERT_MSG_DROP; // remove if ENHANCEMENT done
        }

#if (PROTOCOL == ETXDSR)
        uint32_t weight = dsr_rreq_get_weight_incl_hop_to_self(iface, rreq);
        res = dsr_is_rreqcache_entry_present_and_worse_than(rreq->data[0].address,
                ntohs(rreq->identification), rreq->target_address, weight);

        if(res == DSR_RREQTABLE_DONT_FORWARD_RREQ) {
            dessert_debug("RREQ[%"PRIi64"]: dropping RREQ due to ETX rules", id);
            return DESSERT_MSG_DROP;
        }

#endif
#if ((PROTOCOL == DSR) || PROTOCOL == MDSR_PROTOKOLL_1)
        /* DONE: Route Request Table (RREQ handling, search entry) rfc4728 p67
         Else, the node MUST search its Route Request Table for an entry
         for the initiator of this Route Request (the IP Source Address
         field). */

        /*If such an entry is found in the table, the node MUST
         search the cache of Identification values of recently received
         Route Requests in that table entry, to determine if an entry is
         present in the cache matching the Identification value and target
         node address in this Route Request.*/
        res = dsr_is_rreqcache_entry_present(rreq->data[0].address,
                                             ntohs(rreq->identification), rreq->target_address);

        if(res == DSR_RREQTABLE_DONT_FORWARD_RREQ) {
            /*If such an (Identification, target address) entry is found in this cache
             * in this entry in the Route Request Table, then the node MUST discard
             * the entire packet carrying the Route Request option. */
            dessert_debug("RREQ[%"PRIi64"]: seen this RREQ before, dropping it", id);
            return DESSERT_MSG_DROP;
        }

        /* Else, this node SHOULD further process the Route Request according
         to the following sequence of steps:

         o  DONE: Route Request Table (RREQ handling, add entry) rfc4728 p68
         Add an entry for this Route Request in its cache of
         (Identification, target address) values of recently received
         Route Requests. */
        res = dsr_add_node_to_rreqtable_cache( rreq->data[0].address, ntohs(rreq->identification), rreq->target_address);
        assert(res == DSR_RREQTABLE_SUCCESS);
#endif
#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
        uint32_t weight = dsr_rreq_get_weight_incl_hop_to_self(iface, rreq);
        res = dsr_smr_is_rreq_forward_ok(rreq->data[0].address, ntohs(rreq->identification), rreq->target_address, weight, msg->l2h.ether_shost);

        if(res == DSR_RREQTABLE_DONT_FORWARD_RREQ) {
            dessert_debug("RREQ[%"PRIi64"]: dropping RREQ due to SMR/BACKUPPATH rules", id);
            return DESSERT_MSG_DROP;
        }

#endif

        /* TODO_ENHANCEMENT: Cached Route Reply (RREQ handling) rfc4728 p68
         This node SHOULD search its own Route Cache for a route (from
         itself, as if it were the source of a packet) to the target of
         this Route Request.  If such a route is found in its Route
         Cache, then this node SHOULD follow the procedure outlined in
         Section 8.2.3 to return a "cached Route Reply" to the initiator
         of this Route Request, if permitted by the restrictions specified there. */

        /* TODO_ENHANCEMENT: Jitter (RREQ handling) rfc4728 p68
         If the node does not return a cached Route Reply, then this
         node SHOULD transmit this copy of the packet as a link-layer
         broadcast, with a short jitter delay before the broadcast is
         sent.  The jitter period SHOULD be chosen as a random period,
         uniformly distributed between 0 and BroadcastJitter. */

        if(unlikely(DSR_RREQ_GET_HOPCOUNT(rreq) >= DSR_RREQ_MAX_HOPS_IN_OPTION - 2)) {
            dessert_err("RREQ[%"PRIi64"]: There are already %i addresses in the received RREQ to dest [" MAC "]! Not adding another one.",
                id,
                DSR_RREQ_GET_HOPCOUNT(rreq),
                EXPLODE_ARRAY6(rreq->target_address));
            return DESSERT_MSG_DROP;
        }
        else {
            dsr_propagate_rreq(iface, msg, rreq, rreq_ext);
        }
    }

    return DESSERT_MSG_KEEP;
}

int repl_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* ext;
    dsr_repl_ext_t* repl;

    /* if there is no repl extension in the message, just return, else make sure
     * msg is NOT sparse */
    if(!dessert_msg_getext(msg, &ext, DSR_EXT_REPL, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        repl = (dsr_repl_ext_t*) ext->data;
        //		dessert_info("REPL[%"PRIi64"]: DSR_EXT_REPL in msg (%u hops)", id, DSR_REPL_GET_HOPCOUNT(repl));
    }

    /* mark msg as carrying a REPL, so SOURCE nows what to cache */
    proc->lreserved |= DSR_REPL_EXTENSION_IN_MSG;

    /* TODO: cache bei LINKCACHE==1 alle bereits traversierten links (-> segments_left in source_ext) */
    /* TODO: bei LINKCACHE==1 für alle msgs im sendbuffer clever einzeln prüfen ob route zum ziel jetzt besteht */
    if(ADDR_CMP(dessert_l25_defsrc, repl->data[0].address) == 0) {

#if (LINKCACHE == 1)

        /* DONE: Route Cache (REPL handling, caching)
         -If the DSR Options header contains a Route Reply option, the node
         SHOULD extract the source route from the Route Reply and add this
         routing information to its Route Cache, subject to the conditions
         identified in Section 3.3.1.  The source route from the Route
         Reply is the sequence of hop addresses

         initiator, Address[1], Address[2], ..., Address[n]

         where initiator is the value of the Destination Address field in
         the IP header of the packet carrying the Route Reply (the address
         of the initiator of the Route Discovery), and each Address[i] is a
         node through which the source route passes, in turn, on the route
         to the target of the Route Discovery.  Address[n] is the address
         of the target.  If the Last Hop External (L) bit is set in the
         Route Reply, the node MUST flag the last hop from the Route Reply
         (the link from Address[n-1] to Address[n]) in its Route Cache as
         External.  The value n here is the number of addresses in the
         source route being returned in the Route Reply option, or
         (Opt Data Len - 1) / 4.

         After possibly updating the node's Route Cache in response to the
         routing information in the Route Reply option, then if the
         packet's IP Destination Address matches one of this node's IP
         addresses, the node MUST then process the Route Reply option as
         described in Section 8.2.6. */

        dsr_msg_cache_repl_ext_to_linkcache(repl);

#else /* NO LINKCACHE */

        /* DONE: Route Cache (REPL handling, add path) rfc4728 p 74
         Section 8.1.4 describes the general processing for a received packet,
         including the addition of routing information from options in the
         packet's DSR Options header to the receiving node's Route Cache.

         If the received packet contains a Route Reply, no additional special
         processing of the Route Reply option is required beyond what is
         described there.  As described in Section 4.1, any time a node adds
         new information to its Route Cache (including the information added
         from this Route Reply option), the node SHOULD check each packet in
         its own Send Buffer (Section 4.2) to determine whether a route to
         that packet's IP Destination Address now exists in the node's Route
         Cache (including the information just added to the Cache).  If so,
         the packet SHOULD then be sent using that route and removed from the
         Send Buffer.  This general procedure handles all processing required
         for a received Route Reply option. */

        dsr_msg_cache_repl_ext_to_routecache(repl);

#endif
        dsr_rreqtable_got_repl(repl->data[DSR_REPL_GET_HOPCOUNT(repl) - 1].address);
        dsr_sendbuffer_send_msgs_to(
            repl->data[DSR_REPL_GET_HOPCOUNT(repl) - 1].address);
    }

    return DESSERT_MSG_KEEP;
}

int rerr_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* ext;
    dsr_rerr_ext_t* rerr;

    /* if there is no rerr extension in the message, just return */
    if(!dessert_msg_getext(msg, &ext, DSR_EXT_RERR, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        //		dessert_debug("RERR[%"PRIi64"]: DSR_EXT_RERR in msg",id);
        rerr = (dsr_rerr_ext_t*) ext->data;
    }

    /* When a node receives a packet containing a Route Error option, that
     node MUST process the Route Error option according to the following
     sequence of steps: */

    /* DONE: Route Cache (RERR handling, remove link) rfc4728 p 81
     -  The node MUST remove from its Route Cache the link from the node
     identified by the Error Source Address field to the node
     identified by the Unreachable Node Address field (if this link is
     present in its Route Cache).  If the node implements its Route
     Cache as a link cache, as described in Section 4.1, only this
     single link is removed; if the node implements its Route Cache as
     a path cache, however, all routes (paths) that use this link are
     either truncated before the link or removed completely. */
    dessert_debug("RERR[%"PRIi64"]: link failure [" MAC "]->[" MAC "]", id, EXPLODE_ARRAY6(rerr->error_source_address), EXPLODE_ARRAY6(rerr->type_specific_information));

    proc->lreserved |= DSR_RERR_EXTENSION_IN_MSG;

    dsr_routecache_process_link_error(rerr->error_source_address, rerr->type_specific_information);

    /* DONE: Route Cache (RERR handling, trigger update)*/

    /* TODO_ENHANCEMENT: Initiate new RREQ after RERR rfc4728 p82
     In addition, after processing the Route Error as described above, the
     node MAY initiate a new Route Discovery for any destination node for
     which it then has no route in its Route Cache as a result of
     processing this Route Error, if the node has indication that a route
     to that destination is needed.  For example, if the node has an open
     TCP connection to some destination node, then if the processing of
     this Route Error removed the only route to that destination from this
     node's Route Cache, then this node MAY initiate a new Route Discovery
     for that destination node.  Any node, however, MUST limit the rate at
     which it initiates new Route Discoveries for any single destination
     address, and any new Route Discovery initiated in this way as part of
     processing this Route Error MUST conform as a part of this limit.

     RFC_UNCLEAR: attach RERR to RREQ (RERR handling)
     "handle rerr prior to rreq" vs. "order of options" (p20 vs. pp58/59) */

    return DESSERT_MSG_KEEP;
}

int ackreq_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* ackreq_ext;
    dsr_ackreq_ext_t* ackreq = NULL;

    dessert_msg_t* new_ack_msg;
    dessert_ext_t* new_ack_ext;
    dsr_ack_ext_t* new_ack;

    if(!(proc->lflags & DESSERT_RX_FLAG_L2_DST) || proc->lflags
       & DESSERT_RX_FLAG_L2_OVERHEARD) {
        /* We are not the intended recipient of this message or - if we are - we
         * did not receive the message via the correct interface. */
        return DESSERT_MSG_KEEP;
    }

    /* if there is no ACKREQ extension in the message, just return */
    if(!(dessert_msg_getext(msg, &ackreq_ext, DSR_EXT_ACKREQ, 0))) {
        return DESSERT_MSG_KEEP;
    }
    else {
        //		dessert_debug("ACKREQ[%"PRIi64"]: DSR_EXT_ACKREQ in msg",id);
        ackreq = (dsr_ackreq_ext_t*) ackreq_ext->data;
    }

    assert(ackreq != NULL);

    /* -  Create a packet and set the IP Protocol field to the protocol
     number assigned for DSR (48). */
    dessert_msg_new(&new_ack_msg);

    ADDR_CPY(new_ack_msg->l2h.ether_shost, msg->l2h.ether_dhost);
    ADDR_CPY(new_ack_msg->l2h.ether_dhost, msg->l2h.ether_shost);

    /*-  Add an Acknowledgement option to the DSR Options header in the
     packet; set the Acknowledgement option's Option Type field to 6
     and the Opt Data Len field to 10. */
    if(!(dessert_msg_addext(new_ack_msg, &new_ack_ext, DSR_EXT_ACK, DSR_ACK_EXTENSION_HDRLEN) == DESSERT_OK)) {
        dessert_err("ACKREQ[%"PRIi64"]: Could not add ACK to new msg!", id);
        dessert_msg_destroy(new_ack_msg);
        return DESSERT_MSG_KEEP;
    }

    /*
     -  Copy the Identification field from the received Acknowledgement
     Request option into the Identification field in the
     Acknowledgement option.
     -  Set the ACK Source Address field in the Acknowledgement option to
     be the IP Source Address of this new packet (set above to be the
     IP address of this node).
     -  Set the ACK Destination Address field in the Acknowledgement
     option to be the IP Destination Address of this new packet (set
     above to be the IP address of the previous-hop node). */
    new_ack = (dsr_ack_ext_t*) new_ack_ext->data;

    new_ack->identification = ackreq->identification;
    ADDR_CPY(new_ack->ack_source_address, new_ack_msg->l2h.ether_shost);
    ADDR_CPY(new_ack->ack_destination_address, new_ack_msg->l2h.ether_dhost);

    /* Packets containing an Acknowledgement option SHOULD NOT be placed in
     the Maintenance Buffer. */
    dessert_debug("ACKREQ[%"PRIi64"]: Acking via [%s]...", id, iface->if_name);
    dsr_statistics_emit_msg(iface->hwaddr, new_ack_msg->l2h.ether_dhost, new_ack_msg);
    dessert_meshsend_fast(new_ack_msg, iface);
    dessert_msg_destroy(new_ack_msg);

    dessert_msg_delext(msg, ackreq_ext); /* deleting ACK_REQ extension from original msg */

    return DESSERT_MSG_KEEP;
}

int ack_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* ack_ext;
    dsr_ack_ext_t* ack;

    if(!(proc->lflags & DESSERT_RX_FLAG_L2_DST)
        || proc->lflags & DESSERT_RX_FLAG_L2_OVERHEARD) { // FIXME: is this correct?
        /* We are not the intended recipient of this message or - if we are - we
         * did not receive the message via the correct interface. */
        return DESSERT_MSG_KEEP;
    }

    /* if there is no ACK extension in the message, just return */
    if(!(dessert_msg_getext(msg, &ack_ext, DSR_EXT_ACK, 0))) {
        return DESSERT_MSG_KEEP;
    }
    else {
        dessert_debug("ACK[%"PRIi64"]: DSR_EXT_ACK in msg", id);
        ack = (dsr_ack_ext_t*) ack_ext->data;
    }

    dsr_blacklist_remove_node(ack->ack_source_address);

    if(unlikely(dsr_maintenance_buffer_delete_msg(ntohs(ack->identification)) != DSR_MAINTENANCE_BUFFER_SUCCESS)) {
        dessert_err("ACK[%"PRIi64"]: Could not remove ack->id[%i] from maintbuf", id, ntohs(ack->identification));
    }

    /* DONE: Route Cache (ACK handling, add link) rfc4728 p 59
     If the DSR Options header contains an Acknowledgement option, then
     subject to the conditions identified in Section 3.3.1, the node
     SHOULD add to its Route Cache the single link from the node
     identified by the ACK Source Address field to the node identified
     by the ACK Destination Address field. */

#if (LINKCACHE == 1)
    uint16_t link_weight;
#if (METRIC == HC)
    link_weight = 100;
#elif (METRIC == ETX)
    link_weight = dsr_etx_encode(dsr_etx_get_value(ack->ack_destination_address, ack->ack_source_address));
#endif

    if(dsr_linkcache_add_link(ack->ack_source_address, ack->ack_destination_address, link_weight) == DSR_LINKCACHE_SUCCESS) {
        /* DONE: Route Cache (ACK handling, trigger update)*/
        dsr_linkcache_run_dijkstra(dessert_l25_defsrc);
    }

#endif

    return DESSERT_MSG_KEEP;
}

int source_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* source_ext;
    dsr_source_ext_t* source;

    int next_address_index;
    uint8_t next_address[ETHER_ADDR_LEN];

    int network_interface_change = 0;
    dessert_meshif_t* nextif = NULL;

    /* if there is no source extension in the message, just return, else make sure
     * msg is NOT sparse */
    if(!dessert_msg_getext(msg, &source_ext, DSR_EXT_SOURCE, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        //dessert_debug("SOURCE[%"PRIi64"]: DSR_EXT_SOURCE in msg ...", id);
        source = (dsr_source_ext_t*) source_ext->data;
    }

    /* TODO_CORE: Route Cache (SOURCE handling, extract route) rfc4728 p60
     -  If the DSR Options header contains a DSR Source Route option, the
     node SHOULD extract the source route from the DSR Source Route
     option and add this routing information to its Route Cache,
     subject to the conditions identified in Section 3.3.1.  If the
     value of the Salvage field in the DSR Source Route option is zero,
     then the routing information from the DSR Source Route is the
     sequence of hop addresses

     source, Address[1], Address[2], ..., Address[n], destination

     Otherwise (i.e., if Salvage is nonzero), the routing information
     from the DSR Source Route is the sequence of hop addresses

     Address[1], Address[2], ..., Address[n], destination

     where source is the value of the Source Address field in the IP
     header of the packet carrying the DSR Source Route option (the
     original sender of the packet), each Address[i] is the value in
     the Address[i] field in the DSR Source Route option, and
     destination is the value of the Destination Address field in the
     packet's IP header (the last-hop address of the source route).
     The value n here is the number of addresses in source route in the
     DSR Source Route option, or (Opt Data Len - 2) / 4.

     After possibly updating the node's Route Cache in response to the
     routing information in the DSR Source Route option, the node MUST
     then process the DSR Source Route option as described in Section
     8.1.5. */

#if (CACHE_FROM_SOURCE_EXT == 1)

    if(!(proc->lreserved & DSR_RERR_EXTENSION_IN_MSG)) {
        /* only cache anything if there is no RERR in the msg! */
        if(proc->lreserved & DSR_REPL_EXTENSION_IN_MSG) {
            /* there is a REPL extension present in the msg, and therefore some
             * links might be unidirectional. only cache already traversed links.*/
            dessert_debug("SOURCE[%"PRIi64"]: REPL present, address count[%i] segments_left[%i] caching only already traversed links...", id,
                dsr_source_get_address_count(source),
                source->segments_left);

#if (LINKCACHE == 1)
            dsr_msg_cache_source_ext_to_linkcache(source, DSR_REPL_EXTENSION_IN_MSG);
#else
            /* TODO */
#endif
        }
        else {
            /* no REPL extension present. cache all links */
            dessert_debug("SOURCE[%"PRIi64"]: NO REPL present, address count[%i] segments_left[%i] caching all links...", id,
                dsr_source_get_address_count(source),
                source->segments_left);
#if (LINKCACHE == 1)
            dsr_msg_cache_source_ext_to_linkcache(source, DSR_NO_REPL_EXTENSION_IN_MSG);
#else
            /* TODO */
#endif
        }
    }

#endif

    /* DONE: Processing a Received DSR Source Route Option

     TODO_ENHANCEMENT: Automatic Route Shortening rfc4728 pp60/61
     When a node receives a packet containing a DSR Source Route option
     (whether for forwarding, overheard, or the final destination of the
     packet), that node SHOULD examine the packet to determine if the
     receipt of that packet indicates an opportunity for automatic route
     shortening, as described in Section 3.4.3.  Specifically, if this
     node is not the intended next-hop destination for the packet but is
     named in the later unexpended portion of the source route in the
     packet's DSR Source Route option, then this packet indicates an
     opportunity for automatic route shortening:  the intermediate nodes
     after the node from which this node overheard the packet and before
     this node itself are no longer necessary in the source route.  In
     this case, this node SHOULD perform the following sequence of steps
     as part of automatic route shortening:

     -  The node searches its Gratuitous Route Reply Table for an entry
     describing a gratuitous Route Reply earlier sent by this node, for
     which the original sender (of the packet triggering the gratuitous
     Route Reply) and the transmitting node (from which this node
     overheard that packet in order to trigger the gratuitous Route
     Reply) both match the respective node addresses for this new
     received packet.  If such an entry is found in the node's
     Gratuitous Route Reply Table, the node SHOULD NOT perform
     automatic route shortening in response to this receipt of this
     packet.

     -  Otherwise, the node creates an entry for this overheard packet in
     its Gratuitous Route Reply Table.  The timeout value for this new
     entry SHOULD be initialized to the value GratReplyHoldoff.  After
     this timeout has expired, the node SHOULD delete this entry from
     its Gratuitous Route Reply Table.

     -  After creating the new Gratuitous Route Reply Table entry above,
     the node originates a gratuitous Route Reply to the IP Source
     Address of this overheard packet, as described in Section 3.4.3.

     If the MAC protocol in use in the network is not capable of
     transmitting unicast packets over unidirectional links, as
     discussed in Section 3.3.1, then in originating this Route Reply,
     the node MUST use a source route for routing the Route Reply
     packet that is obtained by reversing the sequence of hops over
     which the packet triggering the gratuitous Route Reply was routed
     in reaching and being overheard by this node.  This reversing of
     the route uses the gratuitous Route Reply to test this sequence of
     hops for bidirectionality, preventing the gratuitous Route Reply
     from being received by the initiator of the Route Discovery unless
     each of the hops over which the gratuitous Route Reply is returned
     is bidirectional.

     -  Discard the overheard packet, since the packet has been received
     before its normal traversal of the packet's source route would
     have caused it to reach this receiving node.  Another copy of the
     packet will normally arrive at this node as indicated in the
     packet's source route; discarding this initial copy of the packet,
     which triggered the gratuitous Route Reply, will prevent the
     duplication of this packet that would otherwise occur. */

    /* FIXME: When we implemented Automatic Route Shorting above, remove
     *        this test */

    if(proc->lflags & DESSERT_RX_FLAG_L2_OVERHEARD || !(proc->lflags & DESSERT_RX_FLAG_L2_DST)) {
        return DESSERT_MSG_DROP;
    }

    /* DONE: Processing SOURCE w/o Route Shortening rfc4728 p62
     * If the packet is not discarded as part of automatic route shortening
     above, then the node MUST process the Source Route option according
     to the following sequence of steps:*/

check_segments_left:

    if(source->segments_left == 0) {
        /*-  If the value of the Segments Left field in the DSR Source Route
         option equals 0, then remove the DSR Source Route option from the
         DSR Options header. */
        dessert_msg_delext(msg, source_ext);
    }
    else {

        /*-  Else, let n equal (Opt Data Len - 3) / 6.  This is the number of
         addresses in the DSR Source Route option. */
        if(source->segments_left > dsr_source_get_address_count(source)) {
            /* QUESTION: If the value of the Segments Left field is greater than
             n, then send an ICMP Parameter Problem, Code 0, message [RFC792] to
             the IP Source Address, pointing to the Segments Left field, and
             discard the packet.  Do not process the DSR Source Route option
             further. */
            dessert_err("SOURCE[%"PRIi64"]: value of the source->segements_left is greater than the number of addresses in the SOURCE option [%i] > [%i]", id,
                source->segments_left,
                dsr_source_get_address_count(source));
            return DESSERT_MSG_DROP;
        }
        else {
            /*-  Else, decrement the value of the Segments Left field by 1.
             Let i equal n minus Segments Left.  This is the index of the next
             address to be visited in the Address vector. */

            source->segments_left--; //RFC_FIX:
            next_address_index = dsr_source_indicated_next_hop_index(source);

            dessert_debug("SOURCE[%"PRIi64"]: address count[%i] segments_left[%i] next_address_index[%i]", id,
                dsr_source_get_address_count(source), source->segments_left, next_address_index);

            ADDR_CPY(next_address, ADDR_IDX(source, next_address_index));

            //dessert_debug("SOURCE[%"PRIi64"]: next_address[%M]", id, next_address);

            /*- NOT POSSIBLE: If Address[i] or the IP Destination Address is a
             multicast address, then discard the packet.  Do not process the DSR
             Source Route option further. */

            /*-  If this node has more than one network interface and if
             Address[i] is the address of one this node's network interfaces,
             then this indicates a change in the network interface to use in
             forwarding the packet, as described in Section 8.4.  In this case,
             decrement the value of the Segments Left field by 1 to skip over
             this address (that indicated the change of network interface) and
             go to the first step above (checking the value of the Segments Left
             field) to continue processing this Source Route option; in further
             processing of this Source Route option, the indicated new network
             interface MUST be used in forwarding the packet. */

            if(ADDR_CMP(next_address, dessert_l25_defsrc) == 0) {
                //				dessert_debug("SOURCE[%"PRIi64"]: interface change detected. nextif == sys. segments_left[%i]", id, source->segments_left);

                source->segments_left--;
                goto check_segments_left;
            }

            // only one change is possible.
            if(network_interface_change == 0) {
                MESHIFLIST_ITERATOR_START(nextif) {
                    if(ADDR_CMP(next_address, nextif->hwaddr) == 0) {
                        network_interface_change = 1;
                        dessert_debug("SOURCE[%"PRIi64"]: interface change detected. next_address[" MAC "] is hwaddr of (%s) segments_left[%i]", id,
                            EXPLODE_ARRAY6(next_address),
                            nextif->if_name,
                            source->segments_left);
                        goto check_segments_left;
                    }
                }
                MESHIFLIST_ITERATOR_STOP;
            }

            ADDR_CPY(msg->l2h.ether_dhost, next_address);

            dessert_meshif_t* out_iface;

            if(network_interface_change == 1) {
                assert(iface !=  nextif);
                out_iface = nextif;
            }
            else {
                out_iface = iface;
            }

            dessert_debug("SOURCE[%"PRIi64"]: was [%s]; forwarding via [%s]", id, iface->if_name, out_iface->if_name);

            if(dsr_conf_get_routemaintenance_passive_ack() == 1) {
                if(source->segments_left > 2) {
                    dsr_msg_send_with_route_maintenance(msg, iface, out_iface, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_PASSIVE);
                }
                else {
                    if(dsr_conf_get_routemaintenance_network_ack() == 1) {
                        dsr_msg_send_with_route_maintenance(msg, iface, out_iface, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
                    }
                    else {
                        dsr_statistics_tx_msg(out_iface->hwaddr, msg->l2h.ether_dhost, msg);
                        int res = dessert_meshsend_fast(msg, out_iface);
                        assert(res == DESSERT_OK);
                    }
                }
            }
            else if(dsr_conf_get_routemaintenance_network_ack() == 1) {
                dsr_msg_send_with_route_maintenance(msg, iface, out_iface, DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK);
            }
            else {
                dsr_statistics_tx_msg(out_iface->hwaddr, msg->l2h.ether_dhost, msg);
                int res = dessert_meshsend_fast(msg, out_iface);
                assert(res == DESSERT_OK);
            }

            return DESSERT_MSG_DROP; //we are not the final destination
        }

    }

    return DESSERT_MSG_KEEP;
}

int dsr2sys_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {

    /* packet for myself AND NOT marked as DONT_FORWARD - forward to sys */
    if(proc->lflags & DESSERT_RX_FLAG_L25_DST || proc->lflags
       & DESSERT_RX_FLAG_L25_BROADCAST || proc->lflags
       & DESSERT_RX_FLAG_L25_MULTICAST) {

        if(proc->lreserved & DSR_DONOT_FORWARD_TO_NETWORK_LAYER) {
            dessert_debug("DSR2SYS[%"PRIi64"]: msg marked as DONOT_FORWARD. Dropping it...", id);
            return DESSERT_MSG_DROP;
        }

        dessert_debug("DSR2SYS[%"PRIi64"]: Yep. That one is for me", id);
        dessert_syssend_msg(msg);
    }

    return DESSERT_MSG_DROP;
}

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

/******************************************************************************
 *
 * main --
 *
 ******************************************************************************/
int main(int argc, char** argv) {
    dessert_info("\n     DES-DSR[%s] v%s - %s\n", VERSION_NAME, VERSION_STR, VERSION_DATE);

    FILE* cfg = dessert_cli_get_cfg(argc, argv);

    dsr_conf_initialize();

    /**************************************************************************
     * initialize dessert framework
     *************************************************************************/
    dessert_init(DESSERT_PROTO_STRING, 0x02, DESSERT_OPT_DAEMONIZE);

    /**************************************************************************
     * initialize logging
     *************************************************************************/
    dessert_logcfg(DESSERT_LOG_NOSTDERR | DESSERT_LOG_NOSYSLOG | DESSERT_LOG_RBUF | DESSERT_LOG_FILE);

    /**************************************************************************
     * initialize cli
     *************************************************************************/
    dessert_debug("initializing cli");
    cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set variable");
    cli_exec_info = cli_register_command(dessert_cli, NULL, "info", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "info");

    dsr_conf_register_cli_callbacks(cli_cfg_set, cli_exec_info);

    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys",
        dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG,
        "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh",
        dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG,
        "initialize dsr interface");
#if (LINKCACHE == 1)
    cli_register_command(dessert_cli, cli_exec_info, "linkcache",
        dessert_cli_cmd_showlinkcache, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
        "Print the linkcache data.");
#endif


#if (METRIC == ETX)
    cli_register_command(dessert_cli, cli_exec_info, "etx",
        dessert_cli_cmd_showetx, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
        "Print the ETX metric data.");
    cli_register_command(dessert_cli, cli_exec_info, "unicastetx",
        dessert_cli_cmd_showunicastetx, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
        "Print the unicast ETX metric data.");
#endif
    cli_register_command(dessert_cli, cli_exec_info, "statistics",
        dessert_cli_cmd_showstatistics, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
        "Print statistics data.");

    dessert_debug("initializing cli done");


    /**************************************************************************
     * initialize callbacks
     *************************************************************************/
    dessert_info("initializing callbacks");

    /**************************************************************************
     * register sysrxb callbacks
     *************************************************************************/
    dessert_sysrxcb_add(sys2dsr_cb, 100);

    /**************************************************************************
     * register meshrxb callbacks
     *************************************************************************/
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 15);

    //dessert_meshrxcb_add(debug_cb, 30);
    dessert_meshrxcb_add(loopcheck_cb, 50);
    dessert_meshrxcb_add(statistics_meshrx_cb, 60);
    dessert_meshrxcb_add(maintenance_buffer_passive_ack_meshrx_cb, 65);

#if (METRIC == ETX)
    dessert_meshrxcb_add(etx_meshrx_cb, 70);
    dessert_meshrxcb_add(unicast_etx_meshrx_cb, 75);
#endif

    dessert_meshrxcb_add(dessert_mesh_ipttl, 80);

    dessert_meshrxcb_add(rreq_meshrx_cb, 100);
    dessert_meshrxcb_add(repl_meshrx_cb, 110);
    dessert_meshrxcb_add(rerr_meshrx_cb, 120);
    dessert_meshrxcb_add(ackreq_meshrx_cb, 130);
    dessert_meshrxcb_add(ack_meshrx_cb, 140);
    dessert_meshrxcb_add(source_meshrx_cb, 150);
    dessert_meshrxcb_add(dsr2sys_meshrx_cb, 170);

    dessert_debug("initializing callbacks done");

    /**************************************************************************
     * apply config
     *************************************************************************/
    dessert_info("applying config");
    // we need no password - cli_allow_enable(dessert_cli, "gossip");
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_info("applying config done");

    /**************************************************************************
     * register periodic callbacks
     *************************************************************************/
    struct timeval blacklist_cleanup_interval;
    blacklist_cleanup_interval.tv_sec = DSR_CONFVAR_BLACKLIST_CLEANUP_INTERVAL_SECS;
    blacklist_cleanup_interval.tv_usec = 0;
    dessert_periodic_add(cleanup_blacklist, NULL, NULL, &blacklist_cleanup_interval);

    struct timeval rreqtable_cleanup_interval;
    rreqtable_cleanup_interval.tv_sec = DSR_CONFVAR_RREQTABLE_CLEANUP_INTERVAL_SECS;
    rreqtable_cleanup_interval.tv_usec = 0;
    dessert_periodic_add(cleanup_rreqtable, NULL, NULL, &rreqtable_cleanup_interval);

    struct timeval sendbuffer_cleanup_interval;
    sendbuffer_cleanup_interval.tv_sec = DSR_CONFVAR_SENDBUFFER_CLEANUP_INTERVAL_SECS;
    sendbuffer_cleanup_interval.tv_usec = 0;
    dessert_periodic_add(cleanup_sendbuffer, NULL, NULL, &sendbuffer_cleanup_interval);

#if (METRIC == ETX)
    struct timeval etx_probes_interval;
    etx_probes_interval.tv_sec = DSR_CONFVAR_ETX_PROBE_RATE_SECS;
    etx_probes_interval.tv_usec = 0;
    dessert_periodic_add(dsr_etx_send_probes, NULL, NULL, &etx_probes_interval);

    struct timeval unicastetx_probes_interval;
    unicastetx_probes_interval.tv_sec = DSR_CONFVAR_ETX_PROBE_RATE_SECS;
    unicastetx_probes_interval.tv_usec = 0;
    struct timeval unicastetx_probes_schedule;
    gettimeofday(&unicastetx_probes_schedule, NULL);
    TIMEVAL_ADD_SAFE(&unicastetx_probes_schedule, 0, 500000);
    dessert_periodic_add(dsr_unicastetx_send_probes, NULL, &unicastetx_probes_schedule, &etx_probes_interval);

    struct timeval etx_cleanup_interval;
    etx_cleanup_interval.tv_sec = DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS;
    etx_cleanup_interval.tv_usec = 0;
    dessert_periodic_add(dsr_etx_cleanup, NULL, NULL, &etx_cleanup_interval);
#endif

#if (LINKCACHE == 1)
    struct timeval dijkstra_interval;
    dijkstra_interval.tv_sec = DSR_CONFVAR_DIJKSTRA_SECS;
    dijkstra_interval.tv_usec = 0;
    dessert_periodic_add(dsr_linkcache_run_dijkstra_periodic, NULL, NULL, &dijkstra_interval);
#endif

#ifdef CACHE_STATISTICS
    struct timeval cache_statistics_interval;
    cache_statistics_interval.tv_sec = 10;
    cache_statistics_interval.tv_usec = 0;

    dessert_periodic_add(cache_statistics, NULL, NULL, &cache_statistics_interval);
#endif
    /**************************************************************************
     * initialize data structures
     *************************************************************************/
    dessert_info("initializing data structures");
#if (LINKCACHE == 1)
    dsr_linkcache_init(dessert_l25_defsrc);

    dessert_meshif_t* meshif;
    pthread_rwlock_rdlock(&dessert_cfglock);
    DL_FOREACH(dessert_meshiflist_get(), meshif) {
        dsr_linkcache_add_link(dessert_l25_defsrc, meshif->hwaddr, 0);
        dsr_linkcache_add_link(meshif->hwaddr, dessert_l25_defsrc, 0);
    }
    pthread_rwlock_unlock(&dessert_cfglock);
#endif
    dessert_debug("initializing data structures done");

    /**************************************************************************
     * main loop....
     *************************************************************************/
    dessert_cli_run();
    dessert_run();

    return (0);
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/




