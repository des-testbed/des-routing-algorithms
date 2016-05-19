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
#include <math.h>
#include <utlist.h>

#define RT_FLAGS_SEND_BANT 0x01

typedef struct ara_flow_analysis {
    uint32_t pkt_count;
    uint32_t byte_count;
    double evaporation_factor;
} ara_flow_analysis_t;

/** routing table entry */
typedef struct ara_newrte {
    ara_address_t dst;              ///< destination address
    ara_nexthop_t* nexthops;        ///< list of all potential next hops
    uint8_t flags;                  ///< general purpose flags
    uint8_t num_nexthops;           ///< number of potential next hops
    uint8_t max_ttl;                ///< metric for the length of the shortest path
    ara_flow_analysis_t flow;       ///< everything we need to adapt the evaporation rate
    UT_hash_handle hh;              ///< used by uthash - only head of list is in hashtable
} ara_newrte_t;

ara_newrte_t* newrt = NULL; ///< routing table
pthread_rwlock_t rtlock = PTHREAD_RWLOCK_INITIALIZER; ///< routing table lock

/** PARAMETER - forwarding mode */
uint8_t ara_forw_mode = ARA_FORW_B;

/** PARAMETER - pheromone trail mode */
uint8_t ara_ptrail_mode = ARA_PTRAIL_CUBIC;

/** PARAMETER - plateau factor in CUBIC mode
 *
 * The higher the value, the longer is the plateau for the pheromone
 * value to remain on.
 *
 * Although it is called the plateau factor, it is actually the exponent of the
 * cubic function.
 *
 * Please note: rt_plateau_factor must be an odd value!!!
 *
 * Examples:
 *  - 1 = linear decrease
 *  - 3 = cubic decrease, default value
 *  - 101 = very long plateau with quite vertical edges
 **/
uint8_t rt_plateau_factor = 3.0;

/** Remove pheromone trail if value drops below rt_min_pheromone */
double rt_min_pheromone = 0.1;

/** Aging parameter
 *
 * The aging parameter is dependent on the pheromone trail mode.
 * - ARA_PTRAIL_CLASSIC: multiply current pheromone trail value with rt_delta_q
 * - ARA_PTRAIL_CUBIC: use rt_delta_q as parameter for the cubic aging function
 * - ARA_PTRAIL_LINEAR: decrease pheromone value by rt_delta_q every tick
 */
double rt_delta_q = ARA_PHEROMONE_DEC;

/** When creating trails use this value as initial pheromone level */
double rt_initial = ARA_INITIAL_PHEROMONE;

/** When receiving packets use this value to increase pheromone level */
double rt_inc = ARA_PHEROMONE_INC;

/** Age pheromone values every rt_tick_interval seconds */
uint8_t rt_tick_interval = ARA_TICK_INTERVAL;

/** PARAMETER - generate only node disjoint paths */
uint8_t ara_ndisjoint = ARA_NODE_DISJOINT;

double ara_prune_routes = ARA_PRUNE_LENGTH;

uint8_t ara_backwards_inc = ARA_BACKWARDS_INC;

double ara_ack_miss_credit = ARA_ACK_MISS_CREDIT;

double ara_ack_credit_inc = ARA_ACK_CREDIT_INC;

uint8_t ara_adap_evaporation = ARA_ADAP_EVAPORATION;

uint8_t ara_print_rt_interval_s = ARA_PRINT_RT_INTERVAL;

uint8_t ara_print_cl_interval_s = ARA_PRINT_CL_INTERVAL;

/** Compare routing table entry
  *
  * Compare an ARA route table entry with (dst, nexthop, iface) tripel.
  * Every entry set to NULL is assumed to be already known as equal
  * @arg rte route table entry
  * @arg nexthop next hop
  * @arg iface interface
  * @return 0 if equal, something else otherwise
  */
inline int8_t ara_rte_compare(const ara_nexthop_t* rte, const ara_address_t nexthop, const dessert_meshif_t* iface) {

    if(nexthop != NULL
       && memcmp(&(rte->nexthop), nexthop, sizeof(ara_address_t)) != 0) {
        return -2;
    }

    if(iface != NULL
       && rte->iface != iface) {
        return -4;
    }

    return 0;
}

/** Get best next hop
 *
 * Searches the routing table to find the next hop for a specific destination.
 * The pheromone value is used as metric: the highest value wins.
 *
 * @param dst destination of the packet
 * @param rrte will point to a copy of the found routing table entry
 * @return -1 if route not found, 1 if route found but a BANT should be sent first,
 * and 0 else
 */
int8_t ara_rt_get_best(ara_address_t dst, ara_nexthop_t* next) {
    ara_newrte_t* entry = NULL;
    ara_nexthop_t* best = NULL;
    int ret = -1;

    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    if (entry != NULL) {
    	best = entry->nexthops;
    }

    if(best == NULL) {
        dessert_debug("no routes found for dst=" MAC, EXPLODE_ARRAY6(dst));
    }
    else {
        if(best->pheromone >= rt_min_pheromone) {
            dessert_debug("route found for dst=" MAC " nexthop=" MAC , EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(best->nexthop));

            memcpy(next, best, sizeof(ara_nexthop_t));

            if(entry->flags & RT_FLAGS_SEND_BANT) {
                ret = 1;
                entry->flags &= ~RT_FLAGS_SEND_BANT;
            }
            else {
                ret = 0;
            }
        }
        else {
            dessert_debug("no route found for dst=" MAC, EXPLODE_ARRAY6(dst));
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return ret;
}

/** Check if a route to the destination exists.
 *
 * Searches the routing table to find the next hop for a specific destination.
 * Used for checking for route availability in the node disjoint mode.
 *
 * @param dst destination of the packet
 * @param rrte will point to a copy of the found routing table entry
 * @return -1 if route not found, and 0 else
 */
int8_t ara_rt_route_exists(ara_address_t dst, ara_nexthop_t* next) {
    ara_newrte_t* entry = NULL;
    ara_nexthop_t* best = NULL;
    int ret = -1;

    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    if (entry != NULL) {
        best = entry->nexthops;
    }

    if(best == NULL) {
        dessert_debug("routeexist check: there is no route for dst=" MAC, EXPLODE_ARRAY6(dst));
    }
    else {
        if(best->pheromone >= rt_min_pheromone) {
            dessert_debug("routeexist check: there is a route for dst=" MAC " nexthop=" MAC , EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(best->nexthop));

            memcpy(next, best, sizeof(ara_nexthop_t));
            ret = 0;
        }
        else {
            dessert_debug("routeexist check: there is no route for dst=" MAC, EXPLODE_ARRAY6(dst));
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return ret;
}


/** Get the number of known routes to the destination
 *
 * Returns the number of existing nexthops to the provided destination
 *
 * @param dst destination of the packet
 * @return -1 if route not found, and the number of nexthops else
 */
int8_t ara_rt_route_nhopnumber(ara_address_t dst) {
    ara_newrte_t* entry = NULL;
    int ret = -1;

    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    if (entry != NULL) {
        ret = entry->num_nexthops;
    } else {
        dessert_debug("There is no route found while looking for nhop number for dst=" MAC, EXPLODE_ARRAY6(dst));
    } 

    pthread_rwlock_unlock(&rtlock);
    return ret;
}


/** Get random weighted next hop
 *
 * Searches the routing table to find the next hop for a specific destination.
 * The pheromone value is used as metric: the higher the value, the higher the
 * chance of the next hop to be selected.
 *
 * @param dst destination of the packet
 * @param rrte will point to a copy of the found routing table entry
 * @return -1 if route not found, 1 if route found but a BANT should be sent first,
 * and 0 else
 *
 */
int8_t ara_rt_get_weighted(ara_address_t dst, ara_nexthop_t* rrte) {
    ara_newrte_t* entry = NULL;
    ara_nexthop_t* cur = NULL;
    double psum = 0;
    double target = 0;
    int ret = -1;

    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    if(entry == NULL) {
        dessert_debug("no routes found for dst=" MAC, EXPLODE_ARRAY6(dst));
        goto ara_rtget_weighted_out;
    }

    /* get pheromone sum */
    for(cur = entry->nexthops; cur != NULL; cur = cur->next) {
        if(cur->pheromone < rt_min_pheromone) { // do not count invalidated routes
            continue;
        }

        target += cur->pheromone;
    }

    /* roll dice weighted dice how to forward */
    target *= ((long double) random()) / ((long double) RAND_MAX);

    for(cur = entry->nexthops; cur != NULL; cur = cur->next) {
        psum += cur->pheromone;

        if(psum >= target && cur->pheromone >= rt_min_pheromone) {
            dessert_debug("route found for dst=" MAC " nexthop=" MAC, EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(cur->nexthop));
            goto ara_rtget_weighted_out;
        }
    }

    dessert_err("did not find route due to some strange error (maybe only invalid nexthops in routing tables?)");

ara_rtget_weighted_out:

    if(cur != NULL) {
        memcpy(rrte, cur, sizeof(ara_nexthop_t));

        if(entry->flags & RT_FLAGS_SEND_BANT) {
            ret = 1;
            entry->flags &= ~RT_FLAGS_SEND_BANT;
        }
        else {
            ret = 0;
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return(ret);
}

/** Get random next hop
 *
 * Searches the routing table to find the next hop for a specific destination.
 *
 * @param dst destination of the packet
 * @param rrte will point to a copy of the found routing table entry
 * @return -1 if route not found, 1 if route found but a BANT should be sent first,
 * and 0 else
 *
 */
int8_t ara_rt_get_random(ara_address_t dst, ara_nexthop_t* rrte) {
    ara_newrte_t* entry = NULL;
    ara_nexthop_t* cur = NULL;
    int ret = -1;

    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    if(entry == NULL) {
        dessert_debug("no routes found for dst=" MAC, EXPLODE_ARRAY6(dst));
        goto ara_rtget_weighted_out;
    }
    if (entry->num_nexthops == 0){
        dessert_debug("Error: num_nexthops = 0  when searching random routes found for dst=" MAC, EXPLODE_ARRAY6(dst));
        return ret;
    }
    uint16_t rand_next_hop = random() % entry->num_nexthops;

    uint16_t i = 0;

    for(cur = entry->nexthops; cur != NULL; cur = cur->next) {
        if(i == rand_next_hop && cur->pheromone >= rt_min_pheromone) {
            dessert_debug("route found for dst=" MAC " nexthop=" MAC, EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(cur->nexthop));
            goto ara_rtget_weighted_out;
        }

        i++;
    }

    dessert_err("did not find route due to some strange error: rand_next_hop=%d, entry->num_nexthops=%d", rand_next_hop, entry->num_nexthops);

ara_rtget_weighted_out:

    if(cur != NULL) {
        memcpy(rrte, cur, sizeof(ara_nexthop_t));

        if(entry->flags & RT_FLAGS_SEND_BANT) {
            ret = 1;
            entry->flags &= ~RT_FLAGS_SEND_BANT;
        }
        else {
            ret = 0;
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return ret;
}

int ara_getroute_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    return ara_getroute(msg, len, proc, (dessert_meshif_t*) iface_in, id);
}

/** Get next hop
 *
 * Queries the routing table to get the next hop for the packet.
 * The routing table entry is copied to the processing buffer.
 */
dessert_cb_result ara_getroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    int res;
    ara_nexthop_t rte;
    ara_proc_t* ap = ara_proc_get(proc);

    assert(proc != NULL);

    /* we are only interested in certain packets... */
    if(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
       || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
        /* flood multicast and broadcast */
        ap->flags |= ARA_DELIVERABLE;
        ap->flags |= ARA_FLOOD;
        ap->flags |= ARA_LOCAL;

        /* fake route table entry */
        memcpy(ap->nexthop, ether_broadcast, sizeof(ara_address_t));
        ap->iface_out = NULL;
        dessert_debug("route found for dst=" MAC " - will flood packet", EXPLODE_ARRAY6(ap->dst));
    }
    else if(proc->lflags & DESSERT_RX_FLAG_L25_DST) {
        ap->flags |= ARA_DELIVERABLE;
        ap->flags |= ARA_LOCAL;
        dessert_debug("route found for dst=" MAC " - packet is for myself", EXPLODE_ARRAY6(ap->dst));
    }
    else {
        /* deliver other things */
        ap->flags |= ARA_FORWARD;

        switch(ara_forw_mode) {
            case ARA_FORW_B: {
                res = ara_rt_get_best(ap->dst, &rte);
                break;
            }
            case ARA_FORW_W: {
                res = ara_rt_get_weighted(ap->dst, &rte);
                break;
            }
            case ARA_FORW_R: {
                res = ara_rt_get_random(ap->dst, &rte);
                break;
            }
            default: {
                dessert_err("ara_forw_mode has invalid type %d - not forwarding packets!", ara_forw_mode);
                res = -1;
                break;
            }
        }

        /* we have an entry */
        if(res >= 0) {
            ap->flags |= ARA_DELIVERABLE;
            memcpy(ap->nexthop, rte.nexthop, sizeof(ara_address_t));
            ap->iface_out = rte.iface;

            if(res == 1) {
                dessert_debug("never sent ANT to dst=" MAC, EXPLODE_ARRAY6(ap->dst));
                ap->flags |= ARA_RT_SEND_BANT;
            }

            if(rte.ttl == ara_defttl) {
                dessert_debug(MAC " is last hop to dst=" MAC, EXPLODE_ARRAY6(rte.nexthop), EXPLODE_ARRAY6(ap->dst));
                ap->flags |= ARA_LAST_HOP;
            }
        }
    }

    // handle routefail flagged messages that became deliverable
    if(ap->flags & ARA_DELIVERABLE
       && ap->flags & ARA_ROUTEFAIL) {
        dessert_debug("alternative route found for packet:\n\tsrc=" MAC " seq=%d dst=" MAC " nexthop=" MAC " ",
                      EXPLODE_ARRAY6(ap->src), ap->seq, EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->nexthop));
        msg->u8 &= ~ARA_ROUTEFAIL;   // unset flag because we have an alternative route
        msg->u8 |= ARA_ROUTEPROBLEM; // this signals the destination that it should send a BANT
    }

    return DESSERT_MSG_KEEP;
}

/** Compare to nexthop entries
 *
 * Compare the pheromone values of two nexthop functions.
 * Will always return -1 or 1 and never 0 to signal that both entries are equal.
 */
int _ara_nexthop_cmp(ara_nexthop_t* a, ara_nexthop_t* b) {
    return (a->pheromone >= b->pheromone) ? -1 : 1;
}


/** Modify credit of a routing table entry
  *
  * Increase or decrease the credit for a particular next hop in the routing
  * table.
  *
  * @arg dst destination address
  * @arg nexthop next hop
  * @arg iface interface
  * @arg credit to increase or decrease
  * @return ARA_RT_FAILED if route is not found
  * @return ARA_RT_CREDIT_DECREASED if decreased credit is > ARA_ACK_CREDIT_MIN
  * @return ARA_RT_CREDIT_DEPLETED if decreased credit is <= ARA_ACK_CREDIT_MIN
  * @return ARA_RT_CREDIT_INCREASED if increased credit is > 0 and < ARA_RT_CREDIT_MAX
  * @return ARA_RT_CREDIT_MAX if increased credit is >= ARA_RT_CREDIT_MAX
  */
ara_rt_update_res_t ara_rt_modify_credit(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double amount) {
    ara_rt_update_res_t res = ARA_RT_FAILED;

    pthread_rwlock_wrlock(&rtlock);

    if(newrt != NULL) {
        ara_newrte_t* entry = NULL;
        HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry); // find first entry in hash map

        if(entry != NULL) {
            ara_nexthop_t* cur = entry->nexthops;

            while(cur && cur->next != NULL && ara_rte_compare(cur, nexthop, iface) != 0) {
                cur = cur->next;
            }

            if(cur) { // found a matching entry
                cur->credit += amount;

                if(amount >= 0) {
                    if(cur->credit >= ARA_ACK_CREDIT_MAX) {
                        res = ARA_RT_CREDIT_MAX;
                    }
                    else {
                        res = ARA_RT_CREDIT_INCREASED;
                    }
                }
                else {
                    if(cur->credit <= ARA_ACK_CREDIT_MIN) {
                        res = ARA_RT_CREDIT_DEPLETED;
                    }
                    else {
                        res = ARA_RT_CREDIT_DECREASED;
                    }
                }
            }
        }
    }

    pthread_rwlock_unlock(&rtlock);
    return res;
}

/** Increase packet count for a routing entry
 *
 * @arg dst destination address
 * @return ARA_RT_UPDATED if the counter was increased,
 *         ARA_RT_FAILED otherwise
 */
ara_rt_update_res_t ara_rt_inc_pkt_count(ara_address_t dst, uint32_t bytes) {
    int res = ARA_RT_FAILED;

    pthread_rwlock_wrlock(&rtlock);

    ara_newrte_t* entry = NULL;
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry); // find first entry in hash map

    if(entry != NULL) {
        entry->flow.pkt_count++;
        entry->flow.byte_count += bytes;
        res = ARA_RT_UPDATED;
    }
    else {
        dessert_warn("cannot count packets without routing table entry:\n\tdst=" MAC , EXPLODE_ARRAY6(dst));
    }

    pthread_rwlock_unlock(&rtlock);
    return res;
}

/** update or add the seq and pheromone for a dst/nexthop/iface tripel
 * the table is only updated if seq is newer or has overflown
 * @arg dst destination address
 * @arg nexthop next hop
 * @arg iface interface
 * @arg delta_pheromone pheromone change to apply
 * @arg seq sequence number of the packet triggering the update
 * @return ARA_RT_NEW if the route is new,
 *         ARA_RT_UPDATED if the route is updated,
 *         ARA_RT_FAILED otherwise
 */
ara_rt_update_res_t ara_rt_update(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double delta_pheromone, ara_seq_t seq, uint8_t ttl) {
    int ret = ARA_RT_FAILED;
    ara_newrte_t* entry = NULL;  // routing table entry for the destination
    ara_nexthop_t* cur = NULL;   // nexthop entry currently worked on
    dessert_debug("Updating RT Entry:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                  EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);

    pthread_rwlock_wrlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    /* create an entry for this destination if needed (has empty nexthop list!) */
    if(entry == NULL) {
        if(delta_pheromone < 0) {  /* do not add routes with (delta_pheromone < 0) */
            ret = ARA_RT_FAILED;
            dessert_warn("tried to remove unknown route:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);
            goto ara_rtupdate_out;
        }

        entry = malloc(sizeof(ara_newrte_t));

        if(entry == NULL) {
            dessert_err("failed to allocate new routing table entry");
            goto ara_rtupdate_out;
        }

        memcpy(&(entry->dst), dst, sizeof(ara_address_t));
        entry->nexthops = NULL;
        entry->flags = RT_FLAGS_SEND_BANT;
        entry->flow.pkt_count = 0;
        entry->flow.byte_count = 0;
        entry->flow.evaporation_factor = 1.0;
        entry->num_nexthops = 0;
        entry->max_ttl = 0;

        HASH_ADD_KEYPTR(hh, newrt, &(entry->dst), sizeof(ara_address_t), entry);
        dessert_debug("new routing table entry:\n\tdst=" MAC, EXPLODE_ARRAY6(dst));
        ret = ARA_RT_NEW;
    }

    assert(entry != NULL);

    /* find matching entry in linked list; also find max. TTL of all entries */
    if(entry->nexthops != NULL) {
        cur = entry->nexthops;

        while(cur != NULL && ara_rte_compare(cur, nexthop, iface) != 0) {
            cur = cur->next;

            if(cur == entry->nexthops) {
                dessert_crit("infinite loop in routing table entry - time to use gdb");
            }
        }
    }

    /// \todo do we want to prune all routes or only for routes were we just received an ANT?
    if(ara_prune_routes
       && delta_pheromone > 0  // do not discard if the route shall be deleted
       && ttl > 0              // ignore pruning if we are increasing in forward direction
       && max(entry->max_ttl - ttl + 1, 0) > ((ara_defttl - entry->max_ttl + 1)*ara_prune_routes)) {
        dessert_info("ignoring ptrail inc. due to pruning:\n\t(max_ttl=%d - ttl=%d + 1) > (ara_defttl=%d - max_ttl=%d + 1)*ara_prune_routes=%04.02lf", entry->max_ttl, ttl, ara_defttl, entry->max_ttl, ara_prune_routes);
        ret = ARA_RT_DISCARDED;
        goto ara_rtupdate_out;
    }

    /* no entry not found? create a new one! */
    if(cur == NULL) {
        dessert_debug("new next hop:\n\tdst=" MAC " nexthop=" MAC " iface=%s",
                      EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name);

        if(delta_pheromone < 0) { /* do not add routes with (delta_pheromone < 0) */
            ret = ARA_RT_FAILED;
            dessert_warn("tried to remove unknown route:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);
            goto ara_rtupdate_out;
        }

        cur = malloc(sizeof(ara_nexthop_t));

        if(cur == NULL) {
            dessert_crit("failed to allocate new routing table entry");
            goto ara_rtupdate_out;
        }

        memcpy(&(cur->nexthop), nexthop, sizeof(ara_address_t));

        switch(ara_ptrail_mode) {
            case ARA_PTRAIL_CLASSIC:
                cur->pheromone = (delta_pheromone >= 0) ? delta_pheromone : 0;
                break;
            case ARA_PTRAIL_LINEAR:
            case ARA_PTRAIL_CUBIC:
                cur->pheromone = (delta_pheromone >= 0) ? ARA_INITIAL_PHEROMONE : 0;
                break;
            default:
                assert(0); // should never happen
                break;
        }

        cur->iface = iface;
        cur->ttl = ttl;
        entry->max_ttl = max(ttl, entry->max_ttl);
        cur->credit = ara_ack_miss_credit;
        cur->next = 0;
        cur->prev = 0;

        DL_APPEND(entry->nexthops, cur);
        entry->num_nexthops++;
    }
    /* found nexthop entry */
    else {
        dessert_debug("updating next hop:\n\tdst=" MAC " next=" MAC " iface=%s", EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name);

        switch(ara_ptrail_mode) {
            case ARA_PTRAIL_CLASSIC:
                cur->pheromone = (delta_pheromone >= 0) ? ((cur->pheromone) + delta_pheromone) : 0;
                break;
            case ARA_PTRAIL_LINEAR:
            case ARA_PTRAIL_CUBIC:
                cur->pheromone = (delta_pheromone >= 0) ? min(max(cur->pheromone + delta_pheromone, 0), 1.0) : 0;
                break;
            default:
                assert(0); // should never happen
                break;
        }

        cur->ttl = max(ttl, cur->ttl);
        entry->max_ttl = max(cur->ttl, entry->max_ttl);
        cur->credit += ara_ack_credit_inc;
        if (cur->credit > ARA_ACK_CREDIT_MAX) {
	    cur->credit = ARA_ACK_CREDIT_MAX;
        }
    }

    /* we updated something so far */
    ret = ARA_RT_UPDATED;
    DL_SORT(entry->nexthops, _ara_nexthop_cmp);

ara_rtupdate_out:

    pthread_rwlock_unlock(&rtlock);

    // if this is a new route, all trapped packet for this destination should be sent
    if(ret == ARA_RT_NEW) {
        untrap_packets(dst, ara_retrypacket);
    }

    return ret;
}

/** Update routing entry for an ANT
 *
 * Update routing table entry for a received ANT.
 */
dessert_cb_result ara_routeupdate_ant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    double delta_p = 0;
    ara_nexthop_t rte;

    ara_proc_t* ap = ara_proc_get(proc);

    // do not update...
    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC        // ...for overheard packets sent by this node
       || proc->lflags & DESSERT_RX_FLAG_L2_SRC// ...for overheard packets sent by this node
       || ap->flags & ARA_RT_UPDATE_IGN            // ...if we are told so
       || !(msg->u8 & ARA_ANT)) {                  // ...for data packets; there is another callback for that
        return DESSERT_MSG_KEEP;
    }

    // do not learn routes or increase the pheromone from duplicates in node disjoint mode
    // exception: the packet is an ANT destined for this node
    if(ap->flags & ARA_DUPLICATE
       && ara_ndisjoint
       && !(ap->flags & (ARA_FANT_LOCAL | ARA_BANT_LOCAL))) {
        dessert_info("ignoring duplicate ANT due to node disjoint mode:\n\tdst=" MAC " src=" MAC " seq=%d", EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq);
        return DESSERT_MSG_KEEP;
    }
 
    // do not learn new routes in node disjoint mode if we already have a route for this destination
    // exception: the packet is an ANT destined for this node
    if(ara_ndisjoint
       && !(ap->flags & (ARA_FANT_LOCAL | ARA_BANT_LOCAL))) {

        if(ara_rt_route_exists(ap->src, &rte) != -1) {
            dessert_info("ignoring ANT in node disjoint mode due to existing route:\n\tdst=" MAC " src=" MAC " seq=%d", EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq);
            return DESSERT_MSG_KEEP;
            
        }
    }

    switch(ara_ptrail_mode) {
        case ARA_PTRAIL_CLASSIC:
            delta_p = (double)(msg->ttl);
            break;
        case ARA_PTRAIL_LINEAR:
        case ARA_PTRAIL_CUBIC:
            delta_p = rt_inc;
            break;
        default:
            assert(0); // should never happen
            break;
    }

    int res = ara_rt_update(ap->src, ap->prevhop, iface_in, delta_p, ap->seq, msg->ttl);

    if(res == ARA_RT_DISCARDED) {
        int x = dessert_msg_getext(msg, NULL, DESSERT_EXT_TRACE_REQ, 0);
        int i;

        for(i = 0; i < x; i++) {
            dessert_ext_t*  ext;
            dessert_msg_getext(msg, &ext, DESSERT_EXT_TRACE_REQ, i);
            dessert_debug("\t" MAC, EXPLODE_ARRAY6(ext->data));
        }
    }
    // After Updating of the routingtable this fant or bant should be deleted, if it is for myself.
    if (ap->flags & (ARA_FANT_LOCAL | ARA_BANT_LOCAL)) {
        return (DESSERT_MSG_DROP);
    }
    return (DESSERT_MSG_KEEP);
}

/** Update routing entry for a data packet
 *
 * Update routing table entry for a received data packet.
 */
dessert_cb_result ara_routeupdate_data(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    double delta_p;
    ara_nexthop_t rte;
    int ret;

    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != NULL);

    // do not update...
    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC        // ...for overheard packets sent by this node
       || proc->lflags & DESSERT_RX_FLAG_L2_SRC// ...for overheard packets sent by this node
       || ap->flags & ARA_RT_UPDATE_IGN            // ...if we are told so
       || msg->u8 & ARA_ANT) {                     // ...for ANTs; there is another callback for that
        return DESSERT_MSG_KEEP;
    }

    // increase the packet count if the packet has been sent to this node
    if((proc->lflags & DESSERT_RX_FLAG_L2_DST
        && !(proc->lflags & DESSERT_RX_FLAG_L25_DST))// do not update if this node is the destination
       && !(ap->flags & ARA_DUPLICATE)) {          // do not update for duplicates
        ara_rt_inc_pkt_count(ap->dst, msg->plen);
    }

    // do not learn routes or increase the pheromone from duplicates in node disjoint mode
    // exception: the packet is destined for this node
    if(ap->flags & ARA_DUPLICATE
       && ara_ndisjoint
       && !(proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
        return DESSERT_MSG_KEEP;
    }

    // prevent adding new routes via backward learning in node disjoint mode
    // exception: the route we already know should be increased; the packet is destined to this node
    if(ara_ndisjoint 
       && ara_backwards_inc
       && !(proc->lflags & DESSERT_RX_FLAG_L25_DST)) {
        ret = ara_rt_route_exists(ap->src, &rte);
	if(ret != -1 &&
	   ara_rte_compare(&rte, ap->prevhop, iface_in) != 0) {
	    // the route to increase is not the one we have
            dessert_info("ignoring backward inc in ndisjoint mode due to another existing route:\n\tdst=" MAC " src=" MAC " seq=%d", EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->src), ap->seq);
            return DESSERT_MSG_KEEP;
	} 
    }

    /* This callback handles only packet that are received and have to beforwarded.
     * The original ARA increases the pheromone trail in backwards direction. This might
     * be a bad idea in real networks with asymmetric link. Thus there is a particular
     * mode.
     */
    if(ara_backwards_inc) {
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

        int res = ara_rt_update(ap->src, ap->prevhop, iface_in, delta_p, ap->seq, msg->ttl);

        if(res == ARA_RT_DISCARDED) {
            int x = dessert_msg_getext(msg, NULL, DESSERT_EXT_TRACE_REQ, 0);
            int i;

            for(i = 0; i < x; i++) {
                dessert_ext_t*  ext;
                dessert_msg_getext(msg, &ext, DESSERT_EXT_TRACE_REQ, i);
                dessert_debug("\t" MAC, EXPLODE_ARRAY6(ext->data));
            }
        }
    }

    return DESSERT_MSG_KEEP;
}

/** Delete route because of route error
 *
 * Delete a routing table entry because a packet with set ARA_ROUTEFAIL
 * flag arrived.
 */
dessert_cb_result ara_handle_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    if(proc->lflags & DESSERT_RX_FLAG_L25_DST
       || (msg->u8 & ARA_ROUTEFAIL) == 0) {
        return DESSERT_MSG_KEEP;
    }

    dessert_warn("removing route to " MAC " via " MAC " due to route fail", EXPLODE_ARRAY6(ap->dst), EXPLODE_ARRAY6(ap->prevhop));
//  ara_rt_update(ap->dst, ap->prevhop, iface_in, -1, ap->seq, 0);
    ara_rt_delete(ap->dst, ap->prevhop, iface_in, -1, ap->seq, 0);
    ap->flags |= ARA_RT_UPDATE_IGN;

    return DESSERT_MSG_KEEP;
}


inline double _ara_ttop(double t) {
    double r = 0.5 * (-pow(2 * t - 1, rt_plateau_factor) + 1);
    return r;
}

inline int8_t _ara_signum(double d) {
    return (d >= 0) ? 1 : -1;
}

double _ara_cubic_decrease(double p, double s) {
    double curr_t = 0.5 * (_ara_signum(-2 * p + 1) * pow(fabs(-2 * p + 1), (1.0 / rt_plateau_factor))) + 0.5;
    double r = _ara_ttop(curr_t + s);
    return min(max(r, 0.0), 1.0);
}

/** Simulate pheromone evaporation
 *
 * Decrease the pheromone values of all routing table entries.
 * All entries with a value lower than rt_min_pheromone are removed.
 */
dessert_per_result_t ara_rt_tick(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_newrte_t* entry = NULL;

    /* re-add task */
//    TIMEVAL_ADD(scheduled, rt_tick_interval, 0);
    TIMEVAL_ADD(scheduled, 0, rt_tick_interval * 1000);
    dessert_periodic_add(ara_rt_tick, NULL, scheduled, NULL);

    pthread_rwlock_wrlock(&rtlock);

    /* iterate over route table entries for each destination */
    for(entry = newrt; entry != NULL; /* must be done in if/else */) {
        ara_nexthop_t* cur = NULL;
        ara_nexthop_t* tmp = NULL;

        /* calculate adaptive evaporation factor */
        double adap_slow_down = 1.0;
        double adap_decrease = 0.0;

        if(ara_adap_evaporation) {
            if(entry->num_nexthops < 3) { // slow down if few left
                adap_slow_down = 0.5;
            }
            else {
                double pktsPerNextHop = entry->flow.byte_count / (double) entry->num_nexthops;
                double avrIncPerNextHop = min(pktsPerNextHop * rt_inc, 1.0);
            }
        }

        /* iterate over nexthop entries of the current destination (forwards) */
        uint16_t a = 0;
        DL_FOREACH(entry->nexthops, cur) {
            if(a++ > ARA_TOO_MANY_NEIGHBORS) {
                dessert_warn("have infinite loop or very large neighborhood!");
                a = 0;
            }

            switch(ara_ptrail_mode) {
                case ARA_PTRAIL_CLASSIC:
                    cur->pheromone *= rt_delta_q;
                    break;
                case ARA_PTRAIL_LINEAR:
                    cur->pheromone = cur->pheromone - rt_delta_q * adap_slow_down;
                    break;
                case ARA_PTRAIL_CUBIC:
                    cur->pheromone = _ara_cubic_decrease(cur->pheromone, rt_delta_q * adap_slow_down);
                    break;
                default:
                    assert(0); // should never happen
                    break;
            }
        }

        entry->flow.pkt_count = 0;
        entry->flow.byte_count = 0;

        DL_FOREACH_SAFE(entry->nexthops, cur, tmp) {
            if (cur->pheromone < rt_min_pheromone) {
		entry->num_nexthops--;
		dessert_debug("dropping nexthop " MAC " to dst=" MAC ", num_nexthops=%2d", EXPLODE_ARRAY6(cur->nexthop), EXPLODE_ARRAY6(entry->dst), entry->num_nexthops);
		DL_DELETE(entry->nexthops, cur);
                free(cur);
	    }
	}
    rt_tick_rteremove_done:

        /* are there still nexthops for this destination? */
        if(entry->num_nexthops <= 0) { // remove current entry
	    dessert_debug("dropping whole routing table entry " MAC ", num_nexthops=%2d", EXPLODE_ARRAY6(entry->dst), entry->num_nexthops);
            if(entry->hh.next == NULL) { // this is the last entry in the hashmap
                HASH_DELETE(hh, newrt, entry);
                free(entry);
                entry = NULL; // will end the loop and ara_rt_tick()
            }
            else {
                ara_newrte_t* del = entry;
                entry = entry->hh.next;
                HASH_DELETE(hh, newrt, del);
                free(del);
            }
        }
        else { // update max_ttl and go to the next entry in the hashmap
            uint8_t max_ttl = 0;
            DL_FOREACH(entry->nexthops, cur) {
                max_ttl = max(max_ttl, cur->ttl);
            }
            entry->max_ttl = max_ttl;
            entry = entry->hh.next;
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return DESSERT_PER_KEEP;
}

/** Initialize routing table processing
 *
 * Registers a function to periodically decrease the pheromone
 * values in the routing table entries.
 */
void ara_rt_init() {
    dessert_debug("initalizing routing table");
    dessert_periodic_add(ara_rt_tick, NULL, NULL, NULL);
    dessert_debug("routing table initialized");
}

dessert_per_result_t ara_print_rt_periodic(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_newrte_t* entry = NULL;
    char* end = "\n*END*";
    size_t size_left = 4096 - sizeof(end);
    char buf[size_left];
    uint16_t offset = 0;

    if(newrt) {
        pthread_rwlock_rdlock(&rtlock);

        for(entry = newrt; entry != NULL; entry = entry->hh.next) {
            size_t written = snprintf(buf + offset, size_left, "\n\nd=" MAC " nhops=%ld", EXPLODE_ARRAY6(entry->dst), entry->num_nexthops);

            if(written < 0 || written >= size_left) {
                goto out_of_buffer;
            }

            size_left -= written;
            offset += written;

            ara_nexthop_t* cur;
            DL_FOREACH(entry->nexthops, cur) {
                written = snprintf(buf + offset, size_left, "\n\tn=" MAC " i=%s p=%04.02lf, c=%04.02lf", EXPLODE_ARRAY6(cur->nexthop), cur->iface->if_name, cur->pheromone, cur->credit);

                if(written < 0 || written >= size_left) {
                    goto out_of_buffer;
                }

                size_left -= written;
                offset += written;
            }
        }

        snprintf(buf + offset, size_left, "%s", end);
    out_of_buffer:
        dessert_info("%s", buf);
        pthread_rwlock_unlock(&rtlock);
    }

    if(ara_print_rt_interval_s > 0) {
        scheduled->tv_sec += ara_print_rt_interval_s;
        dessert_periodic_add(ara_print_rt_periodic, NULL, scheduled, NULL);
    }

    return DESSERT_PER_KEEP;
}

/** Print routing table
 *
 * Print all routing table entries to the CLI.
 */
int cli_showroutingtable(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_newrte_t* entry = NULL;

    pthread_rwlock_rdlock(&rtlock);

    for(entry = newrt; entry != NULL; entry = entry->hh.next) {
        cli_print(cli, "\ndst=" MAC ", pkts=%ld, bytes=%ld", EXPLODE_ARRAY6(entry->dst), newrt->flow.pkt_count, newrt->flow.byte_count);

        const char* best = "(best)";
        ara_nexthop_t* cur;
        DL_FOREACH(entry->nexthops, cur) {
            cli_print(cli, "\tnexthop=" MAC " iface=%10s pheromone=%04.02lf hops=%2d credit=%04.02lf %s",
                EXPLODE_ARRAY6(cur->nexthop), cur->iface->if_name, cur->pheromone, ara_defttl - cur->ttl + 1, cur->credit, best);
            best = "";
        }
    }

    pthread_rwlock_unlock(&rtlock);

    return CLI_OK;
}

/** Flush routing table
 *
 * Flush all routing table entries.
 */
int cli_flushroutingtable(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_newrte_t* entry = NULL;

    pthread_rwlock_wrlock(&rtlock);

    while(newrt) {
        ara_nexthop_t* cur;
        entry = newrt;
        DL_FOREACH(entry->nexthops, cur) {
            DL_DELETE(entry->nexthops, cur);
            free(cur);
        }
        HASH_DEL(newrt, entry);
        free(entry);
    }

    pthread_rwlock_unlock(&rtlock);

    cli_print(cli, "flushed routing table");
    dessert_warn("flushed routing table");

    return CLI_OK;
}

/** Delete routing table entry immediately
 * @arg dst destination address
 * @arg nexthop next hop
 * @arg iface interface
 * @arg delta_pheromone pheromone change to apply
 * @arg seq sequence number of the packet triggering the update
 * @return ARA_RT_NEW if the route is new,
 *         ARA_RT_UPDATED if the route is updated,
 *         ARA_RT_FAILED otherwise
 */
ara_rt_update_res_t ara_rt_delete(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double delta_pheromone, ara_seq_t seq, uint8_t ttl) {
    int ret = ARA_RT_FAILED;
    ara_newrte_t* entry = NULL;  // routing table entry for the destination
    ara_nexthop_t* cur = NULL;   // nexthop entry currently worked on
    dessert_debug("Trying to remove RT entry:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                  EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);

    pthread_rwlock_wrlock(&rtlock);
    HASH_FIND(hh, newrt, dst, sizeof(ara_address_t), entry);

    /* entry was not found */
    if(entry == NULL) {
            dessert_warn("tried to remove unknown route entry:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);
            goto ara_rtdelete_out;
    } 

    assert(entry != NULL);

    /* find matching entry in linked list; */
    if(entry->nexthops != NULL) {
        cur = entry->nexthops;

        while(cur != NULL && ara_rte_compare(cur, nexthop, iface) != 0) {
            cur = cur->next;

            if(cur == entry->nexthops) {
                dessert_crit("infinite loop in routing table entry - time to use gdb");
            }
        }
    }

    /* was the nexthop found? */
    if(cur == NULL) {
        dessert_warn("tried to remove unknown next hop:\n\tdst=" MAC " nexthop=" MAC " iface=%s dp=%lf ttl=%d",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name, delta_pheromone, ttl);
        goto ara_rtdelete_out;
    }   
    /* found nexthop entry */
    else {
        dessert_debug("found next hop to delete:\n\tdst=" MAC " next=" MAC " iface=%s", EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(nexthop), iface->if_name);
	
	//may not happen
	assert(entry->num_nexthops != 0);
	entry->num_nexthops--;

        if (entry->num_nexthops <= 0) {
		dessert_debug("dropping whole routing table entry, num_nexthops=%ld", entry->num_nexthops);
                free(cur);
		if(entry->hh.next == NULL) { // this is the last entry in the hashmap
               	    HASH_DELETE(hh, newrt, entry);
                    free(entry);
                }
                else {
                    ara_newrte_t* del = entry;
                    entry = entry->hh.next;
                    HASH_DELETE(hh, newrt, del);
                    free(del);
                }

	}
        else { // nexthops still there
		DL_DELETE(entry->nexthops, cur)
                free(cur);
                dessert_debug("dropping nexthop, not the first/best, num_nexthops=%2d", entry->num_nexthops);

	        uint8_t max_ttl = 0;
		ara_nexthop_t* c = NULL;
                DL_FOREACH(entry->nexthops, c) {
                   max_ttl = max(max_ttl, c->ttl);
                }
                entry->max_ttl = max_ttl;
        }

	// deleted something so far
	ret = ARA_RT_DELETED;
    } 

ara_rtdelete_out:
    pthread_rwlock_unlock(&rtlock);
    return ret;
}


