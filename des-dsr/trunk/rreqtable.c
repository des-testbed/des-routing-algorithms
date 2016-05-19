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

/* RREQTABLE */
dsr_rreqtable_t* _dsr_rreqtable = NULL; // this is a MUST for uthash

pthread_rwlock_t _dsr_rreqtable_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _RREQTABLE_READLOCK pthread_rwlock_rdlock(&_dsr_rreqtable_rwlock)
#define _RREQTABLE_WRITELOCK pthread_rwlock_wrlock(&_dsr_rreqtable_rwlock)
#define _RREQTABLE_UNLOCK pthread_rwlock_unlock(&_dsr_rreqtable_rwlock)

#define _SAFE_RETURN(x) do {_RREQTABLE_UNLOCK; return(x);} while(0)

/* local forward declarations */
static inline dsr_rreqtable_t* _add_new_entry(const uint8_t address[ETHER_ADDR_LEN]);
static inline void _remove_entry(dsr_rreqtable_t* node);
static inline void _reset_entry(dsr_rreqtable_t* node);
static inline dsr_rreqtable_t* _get_entry_for_key(const uint8_t address[ETHER_ADDR_LEN]);
static inline void _mark_entry_used(dsr_rreqtable_t* node);

static inline int _is_routediscovery_ok_now(struct timeval* now, dsr_rreqtable_t* node);

static inline dsr_rreqcache_t* _add_rreqcache_entry(dsr_rreqtable_t* node, const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]);
static inline dsr_rreqcache_t* _get_rreqcache_entry_for_key(dsr_rreqtable_t* node, const dsr_rreqcache_lookup_key_t* lookup_key);
static inline dsr_rreqcache_t* _get_rreqcache_entry(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]);
static inline dsr_rreqcache_t* _get_rreqcache_entry_for_node(dsr_rreqtable_t* node, const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]);
static inline void _destroy_rreqcache_entry(dsr_rreqtable_t* node, dsr_rreqcache_t* cacheentry);

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
static inline dsr_smr_rreqcache_candidate_t* _new_dsr_smr_rreqcache_candidate(const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq);
static inline void _destroy_dsr_smr_rreq_candidate(dsr_smr_rreqcache_candidate_t* candidate);
static inline int _get_nodes_in_common(dsr_smr_rreqcache_candidate_t* sd, dsr_smr_rreqcache_candidate_t* c);
static inline int _get_links_in_common(dsr_smr_rreqcache_candidate_t* sd, dsr_smr_rreqcache_candidate_t* c);
#endif

inline int dsr_rreqtable_is_routediscovery_ok_now(const uint8_t dest[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node = NULL;
    struct timeval now;
    int res = 0;

    _RREQTABLE_WRITELOCK;
    node = _get_entry_for_key(dest);

    if(node == NULL) {
        node = _add_new_entry(dest);
    }

    gettimeofday(&now, NULL);
    res = _is_routediscovery_ok_now(&now, node);
    _RREQTABLE_UNLOCK;

    return res;
}

inline void dsr_rreqtable_got_repl(const uint8_t dest[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node = NULL;

    _RREQTABLE_WRITELOCK;
    node = _get_entry_for_key(dest);
    _reset_entry(node);
    _RREQTABLE_UNLOCK;
}

/** Add an entry to a rreqcache in the Route Request Table.
 * @arg *address
 * @arg identification
 * @arg *target
 * @return DSR_RREQTABLE_SUCCESS
 * @return DSR_RREQTABLE_ERROR_MEMORY_ALLOCATION */
inline int dsr_add_node_to_rreqtable_cache(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node;

    _RREQTABLE_WRITELOCK;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        node = _add_new_entry(address);
    }

    _add_rreqcache_entry(node, identification, target_address);

    _RREQTABLE_UNLOCK;
    return DSR_RREQTABLE_SUCCESS;
}

/** Tests if there is a rreqcache entry present in the Route Request Table.
 * @arg *address
 * @arg identification
 * @arg *target
 * @return DSR_RREQTABLE_RREQCACHE_ENTRY_NOT_PRESENT
 * @return DSR_RREQTABLE_RREQCACHE_ENTRY_PRESENT
 * @return DSR_RREQTABLE_ERROR_MEMORY_ALLOCATION */
inline int dsr_is_rreqcache_entry_present(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]) {
    dsr_rreqcache_t* cacheentry;

    _RREQTABLE_WRITELOCK;
    cacheentry = _get_rreqcache_entry(address, identification, target_address);
    _RREQTABLE_UNLOCK;

    return (cacheentry == NULL ? DSR_RREQTABLE_FORWARD_RREQ : DSR_RREQTABLE_DONT_FORWARD_RREQ);
}

#if (METRIC == ETX)
inline int dsr_is_rreqcache_entry_present_and_worse_than(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], uint32_t weight) {
    dsr_rreqtable_t* node;
    dsr_rreqcache_t* cacheentry;

    _RREQTABLE_WRITELOCK;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        node = _add_new_entry(address);
    }

    assert(node != NULL);

    cacheentry = _get_rreqcache_entry_for_node(node, identification, target_address);

    if(cacheentry == NULL) {
        cacheentry = _add_rreqcache_entry(node, identification, target_address);
        goto forward_rreq;
    }

    assert(cacheentry != NULL);

    /* test if weight is not worse than best so far */
    if(weight >= cacheentry->best_weight) {
        dessert_debug("rreqcache_entry->weight old[%u] new[%u]", cacheentry->best_weight, weight);
        _SAFE_RETURN(DSR_RREQTABLE_DONT_FORWARD_RREQ);
    }

forward_rreq:
    /* update best weight forwarded so far */
    cacheentry->best_weight = weight;

    _RREQTABLE_UNLOCK;

    return DSR_RREQTABLE_FORWARD_RREQ;
}
#endif

#if (PROTOCOL == MDSR_PROTOKOLL_1)
inline int dsr_mdsr_is_repl_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], dsr_path_t* path) {
    dsr_rreqtable_t* node;
    dsr_rreqcache_t* cacheentry;

    _RREQTABLE_WRITELOCK;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        node = _add_new_entry(address);
    }

    assert(node != NULL);

    cacheentry = _get_rreqcache_entry_for_node(node, identification, target_address);

    if(cacheentry == NULL) {
        cacheentry = _add_rreqcache_entry(node, identification, target_address);
    }

    assert(cacheentry != NULL);

    if(cacheentry->path_list == NULL) {
        DL_APPEND(cacheentry->path_list, path);
        _SAFE_RETURN(DSR_MDSR_REPLY_OK);
    }

    dsr_path_t* p;
    int common_link_found = 0;
    DL_FOREACH(cacheentry->path_list, p) {
        if(dsr_path_is_linkdisjoint(path, p) == DSR_PATH_LINK_FOUND) {
            common_link_found = 1;
            dessert_debug("RREQTABLE: common link found...");
            dsr_path_print_to_debug(path);
            dsr_path_print_to_debug(p);
            break;
        }
        else {
            dessert_debug("RREQTABLE: NO common link found...");
            dsr_path_print_to_debug(path);
            dsr_path_print_to_debug(p);
        }
    }

    if(common_link_found == 0) {
        DL_APPEND(cacheentry->path_list, path);
    }
    else {
        free(path);
    }

    _RREQTABLE_UNLOCK;

    return (common_link_found == 1 ? DSR_MDSR_REPLY_NOT_OK : DSR_MDSR_REPLY_OK);
}
#endif

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)

inline int dsr_smr_is_repl_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq) {
    dsr_rreqtable_t* node;
    dsr_rreqcache_t* cacheentry;

    _RREQTABLE_WRITELOCK;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        node = _add_new_entry(address);
    }

    assert(node != NULL);

    cacheentry = _get_rreqcache_entry_for_node(node, identification, target_address);

    if(cacheentry == NULL) {
        cacheentry = _add_rreqcache_entry(node, identification, target_address);
    }

    assert(cacheentry != NULL);

    dsr_smr_rreqcache_candidate_t* candidate;
    candidate = _new_dsr_smr_rreqcache_candidate(iface, rreq);

    if(cacheentry->shortest_delay == NULL) {
        cacheentry->shortest_delay = candidate;
        gettimeofday(&cacheentry->timeout, NULL);
        TIMEVAL_ADD_SAFE(&cacheentry->timeout, 0, DSR_CONFVAR_SMR_RREQCACHE_REPLY_TIMEOUT_MSECS);
        _SAFE_RETURN(DSR_SMR_REPLY_OK);
    }

    DL_APPEND(cacheentry->candidates, candidate);

    _RREQTABLE_UNLOCK;

    return DSR_SMR_REPLY_NOT_OK;
}

inline int dsr_smr_is_rreq_forward_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], uint32_t weight, uint8_t neighbor[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node;
    dsr_rreqcache_t* cacheentry;

    _RREQTABLE_WRITELOCK;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        node = _add_new_entry(address);
    }

    assert(node != NULL);

    cacheentry = _get_rreqcache_entry_for_node(node, identification, target_address);

    if(cacheentry == NULL) {
        cacheentry = _add_rreqcache_entry(node, identification, target_address);
        goto forward_rreq;
    }

    assert(cacheentry != NULL);

    /* test if neighbor is in list */
    int i;

    for(i = 0; i < cacheentry->neighbor_list_len; i++) {
        if(ADDR_CMP(ADDR_IDX(cacheentry, i), neighbor) == 0) {
            _SAFE_RETURN(DSR_RREQTABLE_DONT_FORWARD_RREQ);
        }
    }

    /* test if weight is not worse than best so far */
    if(weight > cacheentry->weight) {
        _SAFE_RETURN(DSR_RREQTABLE_DONT_FORWARD_RREQ);
    }


forward_rreq:
    /* add neighbor and update best weight forwarded so far */
    ADDR_CPY(ADDR_IDX(cacheentry, cacheentry->neighbor_list_len), neighbor);
    cacheentry->neighbor_list_len++;
    cacheentry->weight = weight;

    _RREQTABLE_UNLOCK;

    return DSR_RREQTABLE_FORWARD_RREQ;
}

static inline dsr_smr_rreqcache_candidate_t* _new_dsr_smr_rreqcache_candidate(const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq) {
    assert(iface != NULL);
    assert(rreq != NULL);

    dsr_smr_rreqcache_candidate_t* candidate = NULL;
    candidate = malloc(sizeof(dsr_smr_rreqcache_candidate_t));
    assert(candidate != NULL);

    candidate->next = NULL;
    candidate->prev = NULL;
    candidate->iface = iface;
    size_t rreq_lenght = DSR_RREQ_EXTENSION_HDRLEN + (DSR_RREQ_GET_HOPCOUNT(rreq) * sizeof(dsr_hop_data_t));
    candidate->rreq = malloc(rreq_lenght);
    assert(candidate->rreq != NULL);

    memcpy(candidate->rreq, rreq, rreq_lenght);

    return candidate;
}

static inline void _destroy_dsr_smr_rreq_candidate(dsr_smr_rreqcache_candidate_t* candidate) {
    assert(candidate != NULL);
    assert(candidate->rreq != NULL);

    free(candidate->rreq);
    free(candidate);
}

static inline int _get_nodes_in_common(dsr_smr_rreqcache_candidate_t* sd, dsr_smr_rreqcache_candidate_t* c) {
    assert(sd != NULL);
    assert(c != NULL);
    assert(sd->rreq != NULL);
    assert(c->rreq != NULL);

    dsr_rreq_ext_t* s; /* the shorter rreq w.r.t. hopcount */
    dsr_rreq_ext_t* l; /* the longer rreq w.r.t. hopcount */
    int nodes_in_common = 0;

    if(DSR_RREQ_GET_HOPCOUNT(sd->rreq) <= DSR_RREQ_GET_HOPCOUNT(c->rreq)) {
        s = sd->rreq;
        l = c->rreq;
    }
    else {
        s = c->rreq;
        l = sd->rreq;
    }

    int i;

    for(i = 0; i < DSR_RREQ_GET_HOPCOUNT(s); i++) {
        int j;

        for(j = 0; j < DSR_RREQ_GET_HOPCOUNT(l); j++) {
            if(ADDR_CMP(s->data[i].address, l->data[j].address) == 0) {
                nodes_in_common++;
            }
        }
    }

    return nodes_in_common;
}

static inline int _get_links_in_common(dsr_smr_rreqcache_candidate_t* sd, dsr_smr_rreqcache_candidate_t* c) {
    assert(sd != NULL);
    assert(c != NULL);
    assert(sd->rreq != NULL);
    assert(c->rreq != NULL);

    dsr_rreq_ext_t* s; /* the shorter rreq w.r.t. hopcount */
    dsr_rreq_ext_t* l; /* the longer rreq w.r.t. hopcount */
    int links_in_common = 0;

    if(DSR_RREQ_GET_HOPCOUNT(sd->rreq) <= DSR_RREQ_GET_HOPCOUNT(c->rreq)) {
        s = sd->rreq;
        l = c->rreq;
    }
    else {
        s = c->rreq;
        l = sd->rreq;
    }

    int i;

    for(i = 1; i < DSR_RREQ_GET_HOPCOUNT(s); i++) {
        int j;

        for(j = 1; j < DSR_RREQ_GET_HOPCOUNT(l); j++) {
            if(ADDR_CMP(s->data[i-1].address, l->data[i-1].address) == 0
               &&
               ADDR_CMP(s->data[i].address, l->data[i].address) == 0) {
                links_in_common++;
            }
        }
    }

    return links_in_common;
}

#endif

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t run_rreqtable(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_rreqtable_t* iter_node = NULL;
    struct timeval now;
    int is_ok = 0;

    _RREQTABLE_WRITELOCK;

    gettimeofday(&now, NULL);

    iter_node = _dsr_rreqtable;

    while(iter_node) {
        if(iter_node->timeout.tv_sec != 0) {
            /* otherwise this node is in reset state */
            is_ok = _is_routediscovery_ok_now(&now, iter_node);

            if(is_ok > 0) {
                int ttl = is_ok;
                /* send another RREQ */
                dsr_send_rreq(iter_node->address,
                              dsr_new_rreq_identification(), ttl);

            }
            else if(is_ok == DSR_RREQTABLE_DISCOVERY_MAX_TIMEOUT_REACHED) {
                dessert_debug("DSR_RREQTABLE_DISCOVERY_MAX_TIMEOUT_REACHED dest[" MAC "]", EXPLODE_ARRAY6(iter_node->address));
                _reset_entry(iter_node);
            }
            else if(is_ok == DSR_RREQTABLE_DISCOVERY_WAIT) {
                /* NOOP */
            }
        }

        iter_node = iter_node->hh.next;
    }

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
    dsr_rreqcache_t* cacheentry = NULL;

    iter_node = _dsr_rreqtable;

    while(iter_node) {
        HASH_FOREACH(hh, iter_node->rreqcache, cacheentry) {

            if(cacheentry->complete == 0 && TIMEVAL_COMPARE(&now, &cacheentry->timeout) >= 0) {
                /* time is up: choose one of the candidate paths */
                dsr_smr_rreqcache_candidate_t* the_chosen_one = NULL;
                dsr_smr_rreqcache_candidate_t* candidate = NULL;

                if(cacheentry->candidates != NULL) {
#    if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_2)
                    /* choose the candidate path that is maximally disjoint to the shortest delay path */
                    int nodes_in_common = 0;
                    int links_in_common = 0;
                    uint32_t weight = 0;
                    int hopcount = 0;

                    int cand_nodes_in_common = 0;
                    int cand_links_in_common = 0;
                    uint32_t cand_weight = 0;
                    int cand_hopcount = 0;

                    nodes_in_common = _get_nodes_in_common(cacheentry->shortest_delay, cacheentry->candidates);
                    links_in_common = _get_links_in_common(cacheentry->shortest_delay, cacheentry->candidates);
                    weight = dsr_rreq_get_weight_incl_hop_to_self((cacheentry->candidates)->iface, (cacheentry->candidates)->rreq);
                    hopcount = DSR_RREQ_GET_HOPCOUNT((cacheentry->candidates)->rreq);
                    the_chosen_one = cacheentry->candidates;

                    DL_FOREACH((cacheentry->candidates)->next, candidate) {
                        cand_nodes_in_common = _get_nodes_in_common(cacheentry->shortest_delay, candidate);
                        cand_links_in_common = _get_links_in_common(cacheentry->shortest_delay, candidate);
                        cand_weight = dsr_rreq_get_weight_incl_hop_to_self(candidate->iface, candidate->rreq);
                        cand_hopcount = DSR_RREQ_GET_HOPCOUNT(candidate->rreq);

                        if(cand_nodes_in_common < nodes_in_common)	{
                            goto new_chosen_one;
                        }
                        else if(cand_nodes_in_common == nodes_in_common) {
                            goto test_links_in_common;
                        }
                        else {
                            continue;
                        }

                    test_links_in_common:

                        if(cand_links_in_common < links_in_common) {
                            goto new_chosen_one;
                        }
                        else if(cand_links_in_common == links_in_common) {
                            goto test_weight;
                        }
                        else {
                            continue;
                        }

                    test_weight:

                        if(cand_weight < weight) {
                            goto new_chosen_one;
                        }
                        else if(cand_weight == weight) {
                            goto test_hopcount;
                        }
                        else {
                            continue;
                        }

                    test_hopcount:

                        if(cand_hopcount < hopcount) {
                            goto new_chosen_one;
                        }
                        else {
                            continue;
                        }

                    new_chosen_one:
                        nodes_in_common = cand_nodes_in_common;
                        links_in_common = cand_links_in_common;
                        weight = cand_weight;
                        hopcount = cand_weight;
                        the_chosen_one = candidate;
                    }
#    elif (PROTOCOL == BACKUPPATH_VARIANT_1)
                    /* choose the candidate path that has minimal weight */
                    uint32_t weight = 0;
                    int hopcount = 0;
                    uint32_t cand_weight = 0;
                    int cand_hopcount = 0;

                    weight = dsr_rreq_get_weight_incl_hop_to_self((cacheentry->candidates)->iface, (cacheentry->candidates)->rreq);
                    hopcount = DSR_RREQ_GET_HOPCOUNT((cacheentry->candidates)->rreq);
                    the_chosen_one = cacheentry->candidates;

                    DL_FOREACH((cacheentry->candidates)->next, candidate) {
                        cand_weight = dsr_rreq_get_weight_incl_hop_to_self(candidate->iface, candidate->rreq);
                        cand_hopcount = DSR_RREQ_GET_HOPCOUNT(candidate->rreq);

                        if(cand_weight < weight) {
                            goto new_chosen_one;
                        }
                        else if(cand_weight == weight) {
                            goto test_hopcount;
                        }
                        else {
                            continue;
                        }

                    test_hopcount:

                        if(cand_hopcount < hopcount) {
                            goto new_chosen_one;
                        }
                        else {
                            continue;
                        }

                    new_chosen_one:
                        weight = cand_weight;
                        hopcount = cand_weight;
                        the_chosen_one = candidate;
                    }

#    endif
                }
                else {
                    /* NOOP: no candidate paths to choose from */
                    dessert_debug("RREQTABLE: there are no candidate paths we could choose from!");
                }

                if(the_chosen_one != NULL) {
                    dsr_send_repl(the_chosen_one->iface, the_chosen_one->rreq);
                    dessert_debug("RREQTABLE: replying the maximal-disjoint path to source[" MAC "] for id[%i].", EXPLODE_ARRAY6(the_chosen_one->rreq->data[0].address), ntohs(the_chosen_one->rreq->identification));
                }

                cacheentry->complete = 1;
            }
        }
        iter_node = iter_node->hh.next;
    }

#endif

    _RREQTABLE_UNLOCK;

    return DESSERT_PER_KEEP;
}

dessert_per_result_t cleanup_rreqtable(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_rreqtable_t* iter_node = NULL;
    dsr_rreqtable_t* next_node = NULL;
    struct timeval timeout;

    _RREQTABLE_WRITELOCK;

    gettimeofday(&timeout, NULL);
    timeout.tv_sec -= DSR_CONFVAR_RREQTABLE_CLEANUP_INTERVAL_SECS;

    iter_node = _dsr_rreqtable;

    while(iter_node) {
        next_node = iter_node->hh.next;

        if(TIMEVAL_COMPARE(&timeout, &iter_node->last_used) >= 0) {
            dessert_info("RREQTABLE: removing entry dest[" MAC "], not used for %usecs", EXPLODE_ARRAY6(iter_node->address), DSR_CONFVAR_RREQTABLE_CLEANUP_INTERVAL_SECS);
            _remove_entry(iter_node);
        }

        iter_node = next_node;
    }

    _RREQTABLE_UNLOCK;

    return DESSERT_PER_KEEP;
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

/* RREQTABLE */

static inline dsr_rreqtable_t* _add_new_entry(const uint8_t address[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node = NULL;

    node = malloc(sizeof(dsr_rreqtable_t));
    assert(node != NULL);

    ADDR_CPY(node->address, address);
    _reset_entry(node);
    node->rreqcache = NULL; // this is a MUST for uthash
    node->last_used.tv_sec = 0;
    node->last_used.tv_usec = 0;
    HASH_ADD(hh, _dsr_rreqtable, address, ETHER_ADDR_LEN, node);

    return node;
}

static inline void _remove_entry(dsr_rreqtable_t* node) {
    assert(node != NULL);

    HASH_DELETE(hh, _dsr_rreqtable, node);

    while(node->rreqcache) {
        _destroy_rreqcache_entry(node, node->rreqcache);
    }

    free(node);
    node = NULL;
}

static inline void _reset_entry(dsr_rreqtable_t* node) {
    assert(node != NULL);
    node->rreqs_since_repl = 0;
    node->ttl = 0;
    node->last_rreq.tv_sec = node->last_rreq.tv_usec = 0;
    node->timeout.tv_sec = node->timeout.tv_usec = 0;
}

static inline dsr_rreqtable_t* _get_entry_for_key(const uint8_t address[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node = NULL;

    HASH_FIND(hh, _dsr_rreqtable, address, ETHER_ADDR_LEN, node);

    return node;
}

static inline int _is_routediscovery_ok_now(struct timeval* now, dsr_rreqtable_t* node) {
    int is_ok = 0;
    uint16_t timeout_factor = 0;

    _mark_entry_used(node);

    if(TIMEVAL_COMPARE(now, &node->timeout) >= 0) {

        if(node->rreqs_since_repl - 1
           == dsr_conf_get_routediscovery_maximum_retries()) {
            return DSR_RREQTABLE_DISCOVERY_MAX_TIMEOUT_REACHED;
        }

        if(dsr_conf_get_routediscovery_expanding_ring_search() == 1) {
            switch(node->ttl) {
                case 0:
                    node->ttl = 1; // 255 == expanding ring search globally disabled!!
                    break;
                case 128:
                    node->ttl = 255;
                    break;
                case 255:
                    break;
                default:
                    node->ttl = 2 * node->ttl;
                    break;
            }

            //timeout_factor = (uint16_t) (1 << (node->rreqs_since_repl));

        }
        else {
            node->ttl = 255;
            //timeout_factor = 1;
        }

        timeout_factor = (uint16_t)(1 << (node->rreqs_since_repl));

        node->last_rreq.tv_sec = node->timeout.tv_sec = now->tv_sec;
        node->last_rreq.tv_usec = node->timeout.tv_usec = now->tv_usec;
        __suseconds_t initial_timeout = dsr_conf_get_routediscovery_timeout();
        __suseconds_t timeout = timeout_factor * initial_timeout;
        TIMEVAL_ADD_SAFE(&node->timeout, 0, timeout);
        node->rreqs_since_repl++;

        is_ok = 1;
    }

    return (is_ok ? (int) node->ttl : DSR_RREQTABLE_DISCOVERY_WAIT);
}

/* RREQCACHE */

static inline dsr_rreqcache_t* _add_rreqcache_entry(dsr_rreqtable_t* node, const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]) {
    dsr_rreqcache_t* cacheentry = NULL;

    _mark_entry_used(node);

    cacheentry = malloc(sizeof(dsr_rreqcache_t));
    assert(cacheentry != NULL);

    cacheentry->identification = identification;
    ADDR_CPY(cacheentry->target_address, target_address);

#if (PROTOCOL == MDSR_PROTOKOLL_1)
    cacheentry->path_list = NULL;
#endif

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
    cacheentry->neighbor_list_len = 0;
    cacheentry->shortest_delay = NULL;
    cacheentry->candidates = NULL;
    cacheentry->complete = 0;
#endif

#if (METRIC == ETX)
    cacheentry->best_weight = 65535; /* best weight for this id so far */
#endif

    if(HASH_CNT(hh, node->rreqcache) == DSR_CONFVAR_RREQTABLE_REQUESTTABLEIDS) {
        _destroy_rreqcache_entry(node, node->rreqcache); /* FIFO */
    }

    HASH_ADD(hh, node->rreqcache, identification, DSR_RREQTABLE_RREQCACHE_KEYLEN, cacheentry);

    return cacheentry;
}

static inline dsr_rreqcache_t* _get_rreqcache_entry_for_node(dsr_rreqtable_t* node, const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]) {
    dsr_rreqcache_t* cacheentry;
    dsr_rreqcache_lookup_key_t lookupkey;

    lookupkey.identification = identification;
    ADDR_CPY(lookupkey.target_address, target_address);

    cacheentry = _get_rreqcache_entry_for_key(node, &lookupkey);

    return cacheentry;
}

static inline dsr_rreqcache_t* _get_rreqcache_entry(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]) {
    dsr_rreqtable_t* node = NULL;
    dsr_rreqcache_t* cacheentry = NULL;
    dsr_rreqcache_lookup_key_t lookupkey;

    node = _get_entry_for_key(address);

    if(node == NULL) {
        return NULL;
    }

    assert(node != NULL);

    lookupkey.identification = identification;
    ADDR_CPY(lookupkey.target_address, target_address);

    cacheentry = _get_rreqcache_entry_for_key(node, &lookupkey);

    return cacheentry;
}

static inline dsr_rreqcache_t* _get_rreqcache_entry_for_key(dsr_rreqtable_t* node, const dsr_rreqcache_lookup_key_t* lookup_key) {
    dsr_rreqcache_t* cacheentry;

    _mark_entry_used(node);

    HASH_FIND(hh, node->rreqcache, lookup_key, sizeof(dsr_rreqcache_lookup_key_t), cacheentry);

    return cacheentry;
}

static inline void _mark_entry_used(dsr_rreqtable_t* node) {
    struct timeval now;
    gettimeofday(&now, NULL);

    (node->last_used).tv_sec = now.tv_sec;
    (node->last_used).tv_usec = now.tv_usec;
}

static inline void _destroy_rreqcache_entry(dsr_rreqtable_t* node, dsr_rreqcache_t* cacheentry) {
    HASH_DELETE(hh, node->rreqcache, cacheentry);

#if (PROTOCOL == MDSR_PROTOKOLL_1)
    dsr_path_t* path;

    while(cacheentry->path_list) {
        path = cacheentry->path_list;
        DL_DELETE(cacheentry->path_list, path);
        free(path);
    }

#endif

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
    free(cacheentry->shortest_delay);

    dsr_smr_rreqcache_candidate_t* candidate;

    while(cacheentry->candidates) {
        candidate = cacheentry->candidates;
        DL_DELETE(cacheentry->candidates, candidate);
        free(candidate);
    }

#endif

    free(cacheentry);
}
