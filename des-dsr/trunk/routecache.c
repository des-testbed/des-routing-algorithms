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

pthread_rwlock_t _dsr_routecache_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _ROUTECACHE_READLOCK pthread_rwlock_rdlock(&_dsr_routecache_rwlock)
#define _ROUTECACHE_WRITELOCK pthread_rwlock_wrlock(&_dsr_routecache_rwlock)
#define _ROUTECACHE_UNLOCK pthread_rwlock_unlock(&_dsr_routecache_rwlock)
#define _SAFE_RETURN(x) _ROUTECACHE_UNLOCK; return(x);

dsr_routecache_t* dsr_routecache = NULL;

/* local forward declarations */
static inline void _delete_all_paths_with_link(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
static inline dsr_routecache_t* _get_entry_by_key(const uint8_t dest[ETHER_ADDR_LEN]);
static inline dsr_routecache_t* _add_new_entry(const uint8_t dest[ETHER_ADDR_LEN]);
static inline void _remove_entry(dsr_routecache_t* dest_rc_el);
static inline void _add_to_pathlist(dsr_routecache_t* rc_el, dsr_path_t* path);
static inline void _remove_from_pathlist(dsr_routecache_t* rc_el, dsr_path_t* path);

/**
 *
 * @param dest the mac address of the destination
 * @param path
 * @return
 */
int dsr_routecache_get_first(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path) {
    assert(path != NULL);
    
    dsr_routecache_t* dest_rc_el = NULL;

    _ROUTECACHE_WRITELOCK;

    //	dessert_debug("[ROUTECACHE]  1: Querying RC for a route to [%M]...", dest);
    dest_rc_el = _get_entry_by_key(dest);

    if(dest_rc_el == NULL) {
        //		dessert_debug("[ROUTECACHE] 2a: No Path found in RC.");
#if (LINKCACHE == 1)
        dsr_path_t* p = NULL;

        if(dsr_linkcache_get_shortest_path(dessert_l25_defsrc, dest, &p) < 0) {
            //			dessert_debug("[ROUTECACHE] 3a: ... No Path found in LC.");
            _SAFE_RETURN(DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION);
        }
        else {
            //			dessert_debug("[ROUTECACHE] 3b: ... Path found in LC.");

            dest_rc_el = _add_new_entry(dest);
            _add_to_pathlist(dest_rc_el, p);
        }

#else /* NO LINKCACHE */
        _ROUTECACHE_UNLOCK;

        return DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION;
#endif /* LINKCACHE == 1*/

    }
    else {

        //		dessert_debug("[ROUTECACHE] 2b: ... Path found in RC.");
    }

    /* paths in path list are sorted, so the first path has least weight */
    memcpy(path, dest_rc_el->paths, sizeof(dsr_path_t));
    path->next = NULL;
    path->prev = NULL;
    //	dessert_debug("[ROUTECACHE]  4: ... Copied path with length[%d]", path->len);

    _ROUTECACHE_UNLOCK;

    //	dsr_routecache_print_routecache_to_debug();
    return DSR_ROUTECACHE_SUCCESS;
}

int dsr_routecache_get_next_round_robin(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path) {
    assert(path != NULL);

    dsr_routecache_t* dest_rc_el = NULL;

    _ROUTECACHE_WRITELOCK;

    //	dessert_debug("[ROUTECACHE]  1: Querying RC for a route to [%M]...", dest);
    dest_rc_el = _get_entry_by_key(dest);

    if(dest_rc_el == NULL) {
        //		dessert_debug("[ROUTECACHE] 2a: No Path found in RC.");
        _SAFE_RETURN(DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION);
    }
    else {
        //		dessert_debug("[ROUTECACHE] 2b: ... Path found in RC.");
    }

    memcpy(path, dest_rc_el->paths, sizeof(dsr_path_t));
    path->next = NULL;
    path->prev = NULL;
    //	dessert_debug("[ROUTECACHE]  4: ... Copied path with length[%d]... now round-robin", path->len);

    /* load balancing (round-robin) - the first will be the last... */
    if((dest_rc_el->paths)->next != NULL) {
        dsr_path_t* first = dest_rc_el->paths;
        DL_DELETE(dest_rc_el->paths, first);
        DL_APPEND(dest_rc_el->paths, first);

    }
    else {
        //		dessert_debug("only one path in the routecache");
    }

    _ROUTECACHE_UNLOCK;
    //	dsr_routecache_print_routecache_to_debug();
    return DSR_ROUTECACHE_SUCCESS;
}

void dsr_routecache_process_link_error(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    _ROUTECACHE_WRITELOCK;
    _delete_all_paths_with_link(u, v);
    _ROUTECACHE_UNLOCK;

#if (LINKCACHE == 1)

    if(dsr_linkcache_remove_link(u, v) == DSR_LINKCACHE_SUCCESS) {
        /* the link was removed, run Dijkstra again */
        dsr_linkcache_run_dijkstra(dessert_l25_defsrc);
    }

#endif /* LINKCACHE */
}

void dsr_routecache_add_path(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path) {
    assert(path != NULL);

    _ROUTECACHE_WRITELOCK;

    dsr_routecache_t* dest_rc_el = NULL;

    dest_rc_el = _get_entry_by_key(dest);

    if(dest_rc_el == NULL) {
        dest_rc_el = _add_new_entry(dest);
    }

    _add_to_pathlist(dest_rc_el, path);

    _ROUTECACHE_UNLOCK;
}

void dsr_routecache_print_routecache_to_debug() {
    dsr_path_t* iter_path = NULL;
    dsr_routecache_t* rc_el = NULL;

    _ROUTECACHE_READLOCK;

    HASH_FOREACH(hh, dsr_routecache, rc_el) {
        dessert_debug("ROUTECACHE: dest[" MAC "] routes[%u]", EXPLODE_ARRAY6(rc_el->address), rc_el->route_count);
        DL_FOREACH(rc_el->paths, iter_path) {
            dsr_path_print_to_debug(iter_path);
        }
    }

    _ROUTECACHE_UNLOCK;
}

/******************************************************************************
 *
 * C L I --
 *
 ******************************************************************************/



/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

static inline void _delete_all_paths_with_link(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]) {
    dsr_routecache_t* rc_el = NULL;
    dsr_routecache_t* next_rc_el = NULL;
    dsr_path_t* iter_path = NULL;
    dsr_path_t* tbd_path = NULL;

    rc_el = dsr_routecache;

    while(rc_el) {
        next_rc_el = rc_el->hh.next;

        iter_path = rc_el->paths;

        while(iter_path) {
            if(dsr_path_contains_link(iter_path, u, v) == DSR_PATH_LINK_FOUND) {
                tbd_path = iter_path;
                _remove_from_pathlist(rc_el, iter_path);
                iter_path = iter_path->next;
                free(tbd_path);
            }
            else {
                iter_path = iter_path->next;
            }
        }

        if(rc_el->route_count == 0) {
            _remove_entry(rc_el);
            free(rc_el);
        }

        rc_el = next_rc_el;
    }
}

static inline dsr_routecache_t* _get_entry_by_key(const uint8_t dest[ETHER_ADDR_LEN]) {
    dsr_routecache_t* dest_rc_el = NULL;

    //dessert_debug("Looking up rc_el for dest[%M]", dest);

    /* test if dest exists as dest_rc_el in linkcache */
    HASH_FIND(hh, dsr_routecache, dest, ETHER_ADDR_LEN, dest_rc_el);

    return dest_rc_el;
}

static inline dsr_routecache_t* _add_new_entry(const uint8_t dest[ETHER_ADDR_LEN]) {
    dsr_routecache_t* dest_rc_el = NULL;

    dest_rc_el = malloc(sizeof(dsr_routecache_t)); /* TODO: add dsr_routecache_t to alloc_cache*/
    assert(dest_rc_el != NULL);

    memcpy(dest_rc_el->address, dest, ETHER_ADDR_LEN);
    dest_rc_el->route_count = 0;
    dest_rc_el->paths = NULL;

    HASH_ADD(hh, dsr_routecache, address, ETHER_ADDR_LEN, dest_rc_el);
    dessert_debug("Added rc_el for dest[" MAC "]", EXPLODE_ARRAY6(dest));
    return dest_rc_el;
}

static inline void _remove_entry(dsr_routecache_t* dest_rc_el) {
    assert(dest_rc_el != NULL);

    HASH_DELETE(hh, dsr_routecache, dest_rc_el);
}

static inline void _add_to_pathlist(dsr_routecache_t* rc_el, dsr_path_t* path) {
    assert(path != NULL);
    assert(rc_el != NULL);
    assert(path->len > 0);

    /* only add if path isn't already in the cache */
    dsr_path_t* check_path;
    DL_FOREACH(rc_el->paths, check_path) {
        if(dsr_path_cmp(path, check_path) == 0) {
            /* dsr_path_cmp only compares len and weight, so check if the hops match in detail */
            if(dsr_path_hops_ident(path, check_path) == 0) {
                /* paths are identical */
                free(path);
                return;
            }
        }
    }

#if (PROTOCOL == MDSR_PROTOKOLL_1)
    /* up to DSR_CONFVAR_ROUTECACHE_KEEP_PATHS paths are used, keep them in insertion order */
    DL_APPEND(rc_el->paths, path);
    rc_el->route_count++;
#elif  (PROTOCOL == SMR)

    /* only two paths are used, keep them in insertion order */
    if(rc_el->route_count <= 1) {
        DL_APPEND(rc_el->paths, path);
        rc_el->route_count++;
    }
    else {
        dessert_err("Destination replied with more than two(2) REPLs!");
        free(path);
    }

#elif (PROTOCOL == BACKUPPATH_VARIANT_1)

    /* only two paths are used, but the primary path arrives second */
    if(rc_el->route_count == 0) {
        DL_APPEND(rc_el->paths, path);
        rc_el->route_count++;
    }
    else if(rc_el->route_count == 1) {
        DL_PREPEND(rc_el->paths , path);
        rc_el->route_count++;
    }
    else {
        dessert_err("Destination replied with more than two(2) REPLs!");
        free(path);
    }

#elif (PROTOCOL == BACKUPPATH_VARIANT_2)

    /* only two paths are used, keep them in insertion order */
    if(rc_el->route_count <= 1) {
        DL_APPEND(rc_el->paths, path);
        rc_el->route_count++;
    }
    else {
        dessert_err("Destination replied with more than two(2) REPLs!");
        free(path);
    }

#else
    /* add every new path, keep them sorted by weight */
    DL_APPEND(rc_el->paths, path);
    rc_el->route_count++;
    DL_SORT(rc_el->paths, dsr_path_cmp);
#endif

#if (DSR_CONFVAR_ROUTECACHE_KEEP_PATHS > 0)
    /* keep only the first DSR_CONFVAR_ROUTECACHE_KEEP_PATHS paths to dest */
    dsr_path_t* tbd;

    if(rc_el->route_count > DSR_CONFVAR_ROUTECACHE_KEEP_PATHS) {
        dessert_debug("Deleting %i needless backup paths...", rc_el->route_count - DSR_CONFVAR_ROUTECACHE_KEEP_PATHS);

        while(rc_el->route_count > DSR_CONFVAR_ROUTECACHE_KEEP_PATHS) {
            tbd = (rc_el->paths)->prev;
            DL_DELETE(rc_el->paths, tbd);
            rc_el->route_count--;
            free(tbd);
        }
    }

#endif
}

static inline void _remove_from_pathlist(dsr_routecache_t* rc_el, dsr_path_t* path) {
    assert(path != NULL);
    assert(rc_el != NULL);

    DL_DELETE(rc_el->paths, path);
    rc_el->route_count--;
}
