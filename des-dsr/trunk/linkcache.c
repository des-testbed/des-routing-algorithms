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

 *******************************************************************************/

#include "dsr.h"

#if (LINKCACHE == 1)

/* LINKCACHE */
dsr_linkcache_t* dsr_linkcache = NULL; // this is a MUST for uthash
pthread_rwlock_t _dsr_linkcache_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _LINKCACHE_READLOCK pthread_rwlock_rdlock(&_dsr_linkcache_rwlock)
#define _LINKCACHE_WRITELOCK pthread_rwlock_wrlock(&_dsr_linkcache_rwlock)
#define _LINKCACHE_UNLOCK pthread_rwlock_unlock(&_dsr_linkcache_rwlock)

#define _SAFE_RETURN(x) _LINKCACHE_UNLOCK; return(x)

/* local forward declarations */
static inline uint16_t _dsr_linkcache_get_weight_XXX(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN]);
static inline dsr_linkcache_t* _add_new_node(const uint8_t u[ETHER_ADDR_LEN]);
static inline void _remove_node(dsr_linkcache_t* u);
static inline void _remove_node_if_detached(dsr_linkcache_t* u);
static inline dsr_linkcache_t* _get_node_by_key(const uint8_t u[ETHER_ADDR_LEN]);
static inline int _is_detached_node(const dsr_linkcache_t* u);
static inline int _ADJ_COUNT(const dsr_linkcache_t* u);
static inline int _has_adjacent_nodes(const dsr_linkcache_t* u);
static inline int _is_adjacent_to_nodes(const dsr_linkcache_t* u);
static inline dsr_adjacency_list_t* _add_new_adjentry(dsr_linkcache_t* u, dsr_linkcache_t* v, const uint16_t weight);
static inline void _remove_adjentry(dsr_linkcache_t* u, dsr_adjacency_list_t* v);
static inline dsr_adjacency_list_t* _get_adjentry_by_keys(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
static inline dsr_adjacency_list_t* _get_adjentry_by_key(dsr_linkcache_t* u, const uint8_t v[ETHER_ADDR_LEN]);

int dsr_linkcache_add_link(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t weight) {
    assert(ADDR_CMP(u, v) != 0);
    dessert_debug("LINKCACHE: caching [" MAC "]->[" MAC "] weight[%u]", EXPLODE_ARRAY6(u), EXPLODE_ARRAY6(v), weight);

    dsr_linkcache_t* u_lc_el = NULL;
    dsr_linkcache_t* v_lc_el = NULL;
    dsr_adjacency_list_t* v_adj_el = NULL;

    _LINKCACHE_WRITELOCK;

    /* test if u exists in link cache as u_lc_el */
    u_lc_el = _get_node_by_key(u);

    if(u_lc_el == NULL) {
        /* no: add u as u_lc_el to link cache */;
        u_lc_el = _add_new_node(u);
    }

    /* test if v exists in link cache as v_lc_el */
    v_lc_el = _get_node_by_key(v);

    if(v_lc_el == NULL) {
        /* no: add v as v_lc_el to link cache */;
        v_lc_el = _add_new_node(v);
    }

#ifndef NDEBUG
    int u_adj_cnt = _ADJ_COUNT(u_lc_el);
    int v_in_use  = v_lc_el->in_use;
#endif

    /*test if v exists in u_lc_el:adj */
    v_adj_el = _get_adjentry_by_key(u_lc_el, v);

    if(v_adj_el == NULL) {
        assert(v_adj_el == NULL);
        /* add v to u_lc_el:adj as v_adj_el */
        v_adj_el = _add_new_adjentry(u_lc_el, v_lc_el, weight);
        assert(v_adj_el != NULL);
    }
    else {
        assert(v_adj_el != NULL);
#if (METRIC != HC)
        v_adj_el->weight = weight;
        _SAFE_RETURN(DSR_LINKCACHE_SUCCESS);
#else
        _SAFE_RETURN(DSR_LINKCACHE_ERROR_LINK_ALREADY_IN_CACHE);
#endif
    }

#ifndef NDEBUG
    assert(_ADJ_COUNT(u_lc_el) == u_adj_cnt + 1);
    assert(v_lc_el->in_use == v_in_use + 1);
    assert(_get_adjentry_by_key(u_lc_el, v) != NULL);
#endif

    _LINKCACHE_UNLOCK;

    return DSR_LINKCACHE_SUCCESS;
}

int dsr_linkcache_remove_link(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    assert(ADDR_CMP(u, v) != 0);

    dsr_linkcache_t* u_lc_el;
    dsr_adjacency_list_t* v_adj_el;
    dsr_linkcache_t* v_lc_el;

    _LINKCACHE_WRITELOCK;

    /* test if u exists as u_lc_el in link cache */
    u_lc_el = _get_node_by_key(u);

    if(u_lc_el == NULL) {
        goto no_such_link_error;
    }

    /* test if v exists as v_adj_el in u_lc_el:adj */
    v_adj_el = _get_adjentry_by_key(u_lc_el, v);

    if(v_adj_el == NULL) {
        goto no_such_link_error;
    }

    /* remove v_adj_el from u_lc_el'a adjacency list, however backup v_adj_el->hop as
     * we need it later on */
    v_lc_el = v_adj_el->hop;


#ifndef NDEBUG
    int u_adj_cnt = _ADJ_COUNT(u_lc_el);
    int v_in_use  = v_lc_el->in_use;
#endif

    _remove_adjentry(u_lc_el, v_adj_el);

#ifndef NDEBUG
    assert(_ADJ_COUNT(u_lc_el) == u_adj_cnt - 1);
    assert(v_lc_el->in_use == v_in_use - 1);
    assert(_get_adjentry_by_key(u_lc_el, v) == NULL);
#endif

    /* if u_lc_el:adj is now empty and if u_lc_el is not in any adjacency lists,
     * remove u_lc_el from link cache as it is safe now to do so */
    _remove_node_if_detached(u_lc_el);

    /* if v_lc_el is now not in any adjacency lists anymore and if v_lc_el:adj is empty ,
    	 * remove v_lc_el from link cache as it is safe now to do so */
    _remove_node_if_detached(v_lc_el);

    _LINKCACHE_UNLOCK;
    return DSR_LINKCACHE_SUCCESS;

no_such_link_error:

    _LINKCACHE_UNLOCK;
    return DSR_LINKCACHE_ERROR_NO_SUCH_LINK;
}

static inline uint16_t _dsr_linkcache_get_weight_XXX(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN]) {
    dsr_adjacency_list_t* v_adj_el;

    /* test if v exists as v_adj_el in u's adjacency list*/

    v_adj_el = _get_adjentry_by_keys(u, v);

    if(v_adj_el == NULL) {
        return 0;
    }

    return v_adj_el->weight;
}

int dsr_linkcache_get_weight(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t* weight) {
    dsr_adjacency_list_t* v_adj_el;

    _LINKCACHE_READLOCK;

    /* test if v exists as v_adj_el in u's adjacency list*/
    v_adj_el = _get_adjentry_by_keys(u, v);

    if(v_adj_el == NULL) {
        _SAFE_RETURN(DSR_LINKCACHE_ERROR_NO_SUCH_LINK);
    }

    *weight = v_adj_el->weight;

    _LINKCACHE_UNLOCK;

    return DSR_LINKCACHE_SUCCESS;
}

int dsr_linkcache_set_weight(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t* weight) {
    dsr_adjacency_list_t* v_adj_el;

    _LINKCACHE_WRITELOCK;

    v_adj_el = _get_adjentry_by_keys(u, v);

    if(v_adj_el == NULL) {
        _SAFE_RETURN(DSR_LINKCACHE_ERROR_NO_SUCH_LINK);
    }

    v_adj_el->weight = *weight;

    _LINKCACHE_UNLOCK;

    return DSR_LINKCACHE_SUCCESS;
}

int dsr_linkcache_get_shortest_path(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN], dsr_path_t** path) {
    *path = NULL;
    uint8_t path_d_to_s[DSR_SOURCE_MAX_ADDRESSES_IN_OPTION * ETHER_ADDR_LEN];
    dsr_linkcache_t* s;
    dsr_linkcache_t* d;

    dsr_linkcache_t* hop;
    int hop_index = -1;
    int path_index;

    _LINKCACHE_READLOCK;

    s = _get_node_by_key(u);

    if(s == NULL) {
#ifdef DEBUG_LINKCACHE
        dessert_err("[LINKCACHE]: Link cache not initialized!");
#endif
        _SAFE_RETURN(DSR_ROUTECACHE_ERROR_LINKCACHE_NOT_INITIALIZED);
    }

    d = _get_node_by_key(v);

    if(d == NULL) {
#ifdef DEBUG_LINKCACHE
        dessert_debug("[LINKCACHE]: Destination node not in link cache!");
#endif
        _SAFE_RETURN(DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION);
    }

#ifdef DEBUG_LINKCACHE
    dessert_debug("[LINKCACHE]: Destination node found.");
#endif

    *path = malloc(sizeof(dsr_path_t));
    (*path)->weight = 0;

    dsr_linkcache_t* prev_hop = NULL;

    for(hop = d; hop != NULL; hop = hop->p) {
        memcpy(path_d_to_s + (++hop_index * ETHER_ADDR_LEN), hop->address, ETHER_ADDR_LEN);

        if(prev_hop != NULL) {
            (*path)->weight += _dsr_linkcache_get_weight_XXX(hop->address, prev_hop->address);
        }

        prev_hop = hop;
#ifdef DEBUG_LINKCACHE
        dessert_debug("[LINKCACHE]:    --> Copying hop [" MAC "] to path_d_to_s[%d] ",
            EXPLODE_ARRAY6((path_d_to_s + (hop_index * ETHER_ADDR_LEN))),
            hop_index * ETHER_ADDR_LEN);
#endif
    }

    if(memcmp(path_d_to_s + (hop_index * ETHER_ADDR_LEN), s->address, ETHER_ADDR_LEN) != 0) {
#ifdef DEBUG_LINKCACHE
        dessert_debug("[LINKCACHE]: Path not ending in src but in [" MAC "]!",
            EXPLODE_ARRAY6((path_d_to_s + (hop_index * ETHER_ADDR_LEN))));
#endif
        free(*path);
        *path = NULL;

        _SAFE_RETURN(DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION);
    }

    for(path_index = 0; hop_index >= 0;) {
#ifdef DEBUG_LINKCACHE
        dessert_debug("[LINKCACHE]:    --> Copying [%d] -> [%d]", path_index, hop_index);
#endif
        memcpy((*path)->address + (path_index * ETHER_ADDR_LEN), path_d_to_s + (hop_index * ETHER_ADDR_LEN), ETHER_ADDR_LEN);
        path_index++;
        hop_index--;
    }

#ifdef DEBUG_LINKCACHE
    dessert_debug("[LINKCACHE]: Copied path with hop-length [%d]. ", path_index);
#endif


    _LINKCACHE_UNLOCK;

    if(path_index == 0) {
        return DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION;
    }


    (*path)->len = path_index;
    dessert_debug("[LINKCACHE]: path length[%d]", (*path)->len);

    return (path_index);
}

int dsr_linkcache_run_dijkstra(uint8_t u[ETHER_ADDR_LEN]) {
    dsr_linkcache_t* s;

    _LINKCACHE_WRITELOCK;

    s = _get_node_by_key(u);

    if(s == NULL) {
        dessert_err("[LINKCACHE]: Link cache not initialized!");
        _SAFE_RETURN(DSR_ROUTECACHE_ERROR_LINKCACHE_NOT_INITIALIZED);
    }

    dessert_info("[LINKCACHE]: Running DIJKSTRA for source[" MAC "]", EXPLODE_ARRAY6(s->address));

    DIJKSTRA(dsr_linkcache, WEIGHT, s);

#ifdef DEBUG_LINKCACHE
    dessert_debug("[LINKCACHE]: Dijkstra done.");
#endif

    _LINKCACHE_UNLOCK;
    return DSR_ROUTECACHE_SUCCESS;
}

void dsr_linkcache_print() {
    dsr_linkcache_t* u;
    dsr_linkcache_t* v;
    dsr_adjacency_list_t* adj;

    _LINKCACHE_READLOCK;

    dessert_debug("######################################################");

    for(u = dsr_linkcache; u != NULL; u = u->hh.next) {
        dessert_debug("[LINKCACHE]:   | [" MAC "] _in_use(%u) adjs(%u)", EXPLODE_ARRAY6(u->address), u->in_use, _ADJ_COUNT(u));

        for(adj = u->adj; adj != NULL; adj = adj->hh.next) {
            v = adj->hop;
            dessert_debug("[LINKCACHE]:      ->[" MAC "]  weight[%u]", EXPLODE_ARRAY6(v->address), adj->weight);
        }

    }

    dessert_debug("######################################################");

    _LINKCACHE_UNLOCK;
}



int dsr_linkcache_init(uint8_t default_src[ETHER_ADDR_LEN]) {
    dsr_linkcache_t* s;

    _LINKCACHE_WRITELOCK;

    s = _add_new_node(default_src);
    s->in_use = 1; /* never delete from link cache */
    dessert_info("initializing LINKCACHE with " MAC , EXPLODE_ARRAY6(s->address));

    _LINKCACHE_UNLOCK;

    return DSR_LINKCACHE_SUCCESS;
}

/******************************************************************************
 *
 * C L I --
 *
 ******************************************************************************/

int dessert_cli_cmd_showlinkcache(struct cli_def* cli, char* command, char* argv[], int argc) {
    dsr_linkcache_t* u;
    dsr_linkcache_t* v;
    dsr_adjacency_list_t* adj;

    _LINKCACHE_READLOCK;

    for(u = dsr_linkcache; u != NULL; u = u->hh.next) {
        cli_print(cli, "|   [" MAC "] _in_use(%3u) adjs(%3u)", EXPLODE_ARRAY6(u->address), u->in_use, _ADJ_COUNT(u));

        for(adj = u->adj; adj != NULL; adj = adj->hh.next) {
            v = adj->hop;
            cli_print(cli, "|-->[" MAC "]-->[" MAC "]  weight[%06u]", EXPLODE_ARRAY6(u->address), EXPLODE_ARRAY6(v->address), adj->weight);
        }

    }

    _LINKCACHE_UNLOCK;

    return CLI_OK;
}


/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t dsr_linkcache_run_dijkstra_periodic(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_linkcache_run_dijkstra(dessert_l25_defsrc);
    return DESSERT_PER_KEEP;
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

/******************************************************************************
 * nodes
 ******************************************************************************/

static inline dsr_linkcache_t* _add_new_node(const uint8_t u[ETHER_ADDR_LEN]) {
    dsr_linkcache_t* u_lc_el;

    assert(_get_node_by_key(u) == NULL);

    u_lc_el = malloc(sizeof(dsr_linkcache_t));
    assert(u_lc_el != NULL);

    memcpy(u_lc_el->address, u, ETHER_ADDR_LEN);
    u_lc_el->in_use = 0;
    u_lc_el->adj = NULL; /* MUST for uthash*/
    HASH_ADD(hh, dsr_linkcache, address, ETHER_ADDR_LEN, u_lc_el);

    assert(_get_node_by_key(u) != NULL);

    return u_lc_el;
}

/** Removes a node @a *u from the link cache.
 *
 * @warning This function is inherently unsafe if used on its own, that is it
 * does NOT handle any adjacency issues. Always use _remove_node_if_detached().
 *
 */
static inline void _remove_node(dsr_linkcache_t* u) {
    assert(u != NULL);
#ifndef NDEBUG
    uint8_t u_address[ETHER_ADDR_LEN];
    ADDR_CPY(u_address, u->address);
    assert(_get_node_by_key(u->address) != NULL);
#endif

    HASH_DELETE(hh, dsr_linkcache, u);
    free(u);
    u = NULL;

#ifndef NDEBUG
    assert(_get_node_by_key(u_address) == NULL);
#endif

}

/** Removes @a *u if it is a detached node.
 *
 * @param *u the node to remove
 *
 * @see _is_detached_node()
 */
static inline void _remove_node_if_detached(dsr_linkcache_t* u) {
    assert(u != NULL);

    if(_is_detached_node(u)) {
        _remove_node(u);
    }
}

static inline dsr_linkcache_t* _get_node_by_key(const uint8_t u[ETHER_ADDR_LEN]) {
    dsr_linkcache_t* u_lc_el;

    /* test if u exists as u_lc_el in link cache */
    HASH_FIND(hh, dsr_linkcache, u, ETHER_ADDR_LEN, u_lc_el);

    return u_lc_el;
}

/** Tests whether @a *u is a detached node, that is @a *u is neither adjacent to
 *  any other node, nor is any other node adjacent to @a *u.
 *
 * @param *u the node to test
 *
 * @return 1 if @a *u is detached, 0 otherwise
 */
static inline int _is_detached_node(const dsr_linkcache_t* u) {
    assert(u != NULL);
    return (!_is_adjacent_to_nodes(u) && !_has_adjacent_nodes(u)) ? 1 : 0;
}

/** Tests whether @a *u has adjacent nodes, that is no other node is adjacent to
 *  @a *u.
 *
 * @param *u the node to test
 *
 * @return 1 if there is some node which is adjacent to @a *u, 0 otherwise
 */
static inline int _has_adjacent_nodes(const dsr_linkcache_t* u) {
    assert(u != NULL);
    return (_ADJ_COUNT(u) > 0) ? 1 : 0;
}

static inline int _ADJ_COUNT(const dsr_linkcache_t* u) {
    return HASH_CNT(hh, u->adj);
}

/** Tests whether @a *u is adjacent to any other nodes.
 *
 * @param *u the node to test
 *
 * @return 1 if @a *u is adjacent to some other node, 0 otherwise
 */
static inline int _is_adjacent_to_nodes(const dsr_linkcache_t* u) {
    assert(u != NULL);
    return (u->in_use > 0) ? 1 : 0;
}

/******************************************************************************
 * adjacency entries
 ******************************************************************************/

static inline dsr_adjacency_list_t* _add_new_adjentry(dsr_linkcache_t* u, dsr_linkcache_t* v, uint16_t weight) {
    dsr_adjacency_list_t* v_adj_el = NULL;

    assert(u != NULL);
    assert(v != NULL);

#ifndef NDEBUG
    int u_adj_cnt = _ADJ_COUNT(u);
    int v_in_use  = v->in_use;
#endif

    v_adj_el = malloc(sizeof(dsr_adjacency_list_t));
    assert(v_adj_el != NULL);

    memcpy(v_adj_el->address, v->address, ETHER_ADDR_LEN);
    v_adj_el->weight = weight;
    v_adj_el->hop = v;
    (v_adj_el->hop)->in_use++;

    HASH_ADD(hh, u->adj, address, ETHER_ADDR_LEN, v_adj_el);

#ifndef NDEBUG
    assert(_ADJ_COUNT(u) == u_adj_cnt + 1);
    assert(v->in_use == v_in_use + 1);
#endif

    return v_adj_el;
}

/** Removes the adjacency entry @a *v from @a *u's adjacency list
 *
 * @param *u the adjacency list to delete from
 * @param *v the entry to remove

 * @return pointer to the node which was removed from @a u's adjacency list
 */
static inline void _remove_adjentry(dsr_linkcache_t* u, dsr_adjacency_list_t* v) {
    assert(u != NULL);
    assert(v != NULL);
    assert(v->hop != NULL);
#ifndef NDEBUG
    int u_adj_cnt = _ADJ_COUNT(u);
    int v_in_use  = (v->hop)->in_use;
#endif

    HASH_DELETE(hh, u->adj, v);
    (v->hop)->in_use--;

#ifndef NDEBUG
    assert(_ADJ_COUNT(u) == u_adj_cnt - 1);
    assert((v->hop)->in_use == v_in_use - 1);
#endif

    free(v);
    v = NULL;
}

static inline dsr_adjacency_list_t* _get_adjentry_by_keys(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    dsr_linkcache_t* u_lc_el;
    dsr_adjacency_list_t* v_adj_el;

    /* test if u exists as u_lc_el in link cache */
    u_lc_el = _get_node_by_key(u);

    if(u_lc_el == NULL) {
        return NULL;
    }

    assert(u_lc_el != NULL);

    /* test if v exists as v_adj_el in u_lc_el:adj */
    v_adj_el = _get_adjentry_by_key(u_lc_el, v);

    return v_adj_el;
}

static inline dsr_adjacency_list_t* _get_adjentry_by_key(dsr_linkcache_t* u, const uint8_t v[ETHER_ADDR_LEN]) {
    dsr_adjacency_list_t* v_adj_el;

    assert(u != NULL);

    /* find v as v_adj_el in u's adjacency list */
    HASH_FIND(hh, u->adj, v, ETHER_ADDR_LEN, v_adj_el);

    return v_adj_el;
}

#endif /* LINKCACHE */
