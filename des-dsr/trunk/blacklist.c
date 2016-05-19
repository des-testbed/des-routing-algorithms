/******************************************************************************
 Copyright 2009, David Gutzmann, Freie Universitaet Berlin (FUB).
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

/* Blacklist*/
dsr_blacklist_t* _dsr_blacklist = NULL; // this is a MUST for uthash
pthread_rwlock_t _dsr_blacklist_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _BLACKLIST_READLOCK pthread_rwlock_rdlock(&_dsr_blacklist_rwlock)
#define _BLACKLIST_WRITELOCK pthread_rwlock_wrlock(&_dsr_blacklist_rwlock)
#define _BLACKLIST_UNLOCK pthread_rwlock_unlock(&_dsr_blacklist_rwlock)

#define _SAFE_RETURN(x) _BLACKLIST_UNLOCK; return(x)

/** Add a node to the blacklist */
int dsr_blacklist_add_node(uint8_t address[ETHER_ADDR_LEN]) {

    dsr_blacklist_t* node;
    int state = dsr_blacklist_get_state(address);

    if(state == DSR_BLACKLIST_FLAG_PROBABLE) {

        return DSR_BLACKLIST_ERROR_NODE_ALREADY_IN_LIST;
    }
    else if(state == DSR_BLACKLIST_FLAG_QUESTIONABLE) {
        dsr_blacklist_set_state(address, DSR_BLACKLIST_FLAG_PROBABLE);

        return DSR_BLACKLIST_SUCCESS;
    }

    assert(state == DSR_BLACKLIST_ERROR_NO_SUCH_NODE);

    node = malloc(sizeof(dsr_blacklist_t));
    assert(node != NULL);

    memcpy(node->neighbor, address, ETHER_ADDR_LEN);
    node->state = DSR_BLACKLIST_FLAG_PROBABLE;
    gettimeofday(&(node->last_updated), NULL);

    _BLACKLIST_WRITELOCK;

    HASH_ADD(hh, _dsr_blacklist, neighbor, ETHER_ADDR_LEN, node);
    dessert_debug("BLACKLIST: added " MAC , EXPLODE_ARRAY6(address));

    _BLACKLIST_UNLOCK;

    return DSR_BLACKLIST_SUCCESS;

}

/** Remove a node from the blacklist */
int dsr_blacklist_remove_node(uint8_t address[ETHER_ADDR_LEN]) {
    _BLACKLIST_WRITELOCK;
    
    dsr_blacklist_t* node = NULL;
    HASH_FIND(hh, _dsr_blacklist, address, ETHER_ADDR_LEN, node);

    if(node != NULL) {
        HASH_DELETE(hh, _dsr_blacklist, node);
        free(node);
        dessert_debug("BLACKLIST: removed " MAC , EXPLODE_ARRAY6(address));
        _SAFE_RETURN(DSR_BLACKLIST_SUCCESS);

    }
    else {
        _SAFE_RETURN(DSR_BLACKLIST_ERROR_NO_SUCH_NODE);
    }
}

/** Determine state of a node in the blacklist*/
int dsr_blacklist_get_state(uint8_t address[ETHER_ADDR_LEN]) {
    dsr_blacklist_t* node = NULL;

    _BLACKLIST_READLOCK;
    HASH_FIND(hh, _dsr_blacklist, address, ETHER_ADDR_LEN, node);

    if(node != NULL) {
        _SAFE_RETURN(node->state);

    }
    else {
        _SAFE_RETURN(DSR_BLACKLIST_ERROR_NO_SUCH_NODE);
    }
}

/** Set state of a node in the blacklist */
int dsr_blacklist_set_state(uint8_t address[ETHER_ADDR_LEN], int new_state) {
    assert(new_state == DSR_BLACKLIST_FLAG_PROBABLE || new_state == DSR_BLACKLIST_FLAG_QUESTIONABLE);

    _BLACKLIST_WRITELOCK;

    dsr_blacklist_t* node = NULL;
    HASH_FIND(hh, _dsr_blacklist, address, ETHER_ADDR_LEN, node);

    if(node != NULL) {
        node->state = new_state;
        gettimeofday(&(node->last_updated), NULL);
        dessert_debug("BLACKLIST: updated " MAC " new state=%d", EXPLODE_ARRAY6(address), new_state);

        _SAFE_RETURN(DSR_BLACKLIST_SUCCESS);

    }
    else {
        _SAFE_RETURN(DSR_BLACKLIST_ERROR_NO_SUCH_NODE);
    }
}

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t cleanup_blacklist(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_blacklist_t* node = NULL;
    dsr_blacklist_t* tbd = NULL;

    struct timeval now;
    time_t expiration_threshold;
    time_t revert_to_questionable_threshold;

    gettimeofday(&now, NULL);
    expiration_threshold = now.tv_sec - (time_t) DSR_CONFVAR_BLACKLIST_EXPIRATION_SECS;
    revert_to_questionable_threshold = now.tv_sec - (time_t) DSR_CONFVAR_BLACKLIST_REVERT_TO_QUESTIONABLE_SECS;

    _BLACKLIST_WRITELOCK;

    /* iterating the hash table as linked list */
    node = _dsr_blacklist;

    while(node != NULL) {
        dessert_debug("BLACKLIST: cleanup [" MAC " state=%d]", EXPLODE_ARRAY6(node->neighbor), node->state);

        if((node->state == DSR_BLACKLIST_FLAG_PROBABLE)
           && (node->last_updated.tv_sec < revert_to_questionable_threshold)) {
            dessert_debug("BLACKLIST: cleanup, reverting state to QUESTIONABLE [" MAC " state=%d]", EXPLODE_ARRAY6(node->neighbor), node->state);

            node->state = DSR_BLACKLIST_FLAG_QUESTIONABLE;
            gettimeofday(&(node->last_updated), NULL);

        }
        else if((node->state == DSR_BLACKLIST_FLAG_QUESTIONABLE)
                && (node->last_updated.tv_sec < expiration_threshold)) {
            dessert_debug("BLACKLIST: cleanup, removing node [" MAC " state=%d]", EXPLODE_ARRAY6(node->neighbor), node->state);
            // deletion is even safe while iterating over the list! :)
            HASH_DELETE(hh, _dsr_blacklist, node);
            tbd = node;
        }

        if(tbd != NULL) {
            node = node->hh.next;
            free(tbd);
            tbd = NULL;
        }
        else {
            node = node->hh.next;
        }
    }

    _BLACKLIST_UNLOCK;

    return DESSERT_PER_KEEP;
}

