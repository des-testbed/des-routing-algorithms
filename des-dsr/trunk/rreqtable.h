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

#ifndef RREQTABLE_H_
#define RREQTABLE_H_

#include "dsr.h"

/******************************************************************************
 *
 * Route Request Table
 *
 * "The Route Request Table of a node implementing DSR records
 *  information about Route Requests that have been recently originated
 *  or forwarded by this node." RFC4728 p30
 ******************************************************************************/

#define DSR_RREQTABLE_FORWARD_RREQ                        0
#define DSR_RREQTABLE_DONT_FORWARD_RREQ                   1
#define DSR_RREQTABLE_RREQCACHE_ENTRY_PRESENT_AND_WORSE   2

#define DSR_RREQTABLE_SUCCESS                             0
#define DSR_RREQTABLE_ERROR_MEMORY_ALLOCATION            -1
#define DSR_RREQTABLE_ERROR_NO_SUCH_NODE                 -2
#define DSR_RREQTABLE_ERROR_NODE_ALREADY_IN_LIST         -3

#define DSR_RREQTABLE_DISCOVERY_WAIT                     -1
#define DSR_RREQTABLE_DISCOVERY_MAX_TIMEOUT_REACHED      -2

/** Used to signal via dsr_rreqtable_t->ttl. If set, only the cache is
 *  valid (we never originated a RREQ for this address)  */
#define DSR_RREQTABLE_TTL_ONLY_CACHE_VALID               -1

inline int dsr_rreqtable_is_routediscovery_ok_now(const uint8_t dest[ETHER_ADDR_LEN]);
inline void dsr_rreqtable_got_repl(const uint8_t dest[ETHER_ADDR_LEN]);

/** Tests if there is a rreqcache entry present in the Route Request Table. */
inline int dsr_is_rreqcache_entry_present(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]);
/** Add an entry to a rreqcache in the Route Request Table.*/
inline int dsr_add_node_to_rreqtable_cache(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN]);

#if (METRIC == ETX)
inline int dsr_is_rreqcache_entry_present_and_worse_than(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], uint32_t weight);
#endif

#if (PROTOCOL == MDSR_PROTOKOLL_1)
#define DSR_MDSR_REPLY_OK                                 0
#define DSR_MDSR_REPLY_NOT_OK                             1

inline int dsr_mdsr_is_repl_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], dsr_path_t* path);
#endif

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
#define DSR_SMR_REPLY_OK                                 0
#define DSR_SMR_REPLY_NOT_OK                             1

typedef struct __attribute__((__packed__)) dsr_smr_rreqcache_candidate {
    struct dsr_smr_rreqcache_candidate* prev;
    struct dsr_smr_rreqcache_candidate* next;
    dsr_rreq_ext_t* rreq;
    dessert_meshif_t* iface;
} dsr_smr_rreqcache_candidate_t;

inline int dsr_smr_is_repl_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], const dessert_meshif_t* iface, dsr_rreq_ext_t* rreq);

inline int dsr_smr_is_rreq_forward_ok(const uint8_t address[ETHER_ADDR_LEN], const uint16_t identification, const uint8_t target_address[ETHER_ADDR_LEN], uint32_t weight, uint8_t neighbor[ETHER_ADDR_LEN]);

#endif

dessert_per_result_t run_rreqtable(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t cleanup_rreqtable(void* data, struct timeval* scheduled, struct timeval* interval);

#define DSR_RREQTABLE_RREQCACHE_KEYLEN (sizeof(uint16_t) + ETHER_ADDR_LEN)

typedef struct __attribute__((__packed__)) dsr_rreqcache {
    uint16_t identification; /* key: part 1 -- in network byte order! */
    uint8_t target_address[ETHER_ADDR_LEN]; /* key: part 2 */
#if (METRIC == ETX)
    uint32_t best_weight; /* best weight for this id so far */
#endif
#if (PROTOCOL == MDSR_PROTOKOLL_1)
    dsr_path_t* path_list; /* List with paths we replied to source */
#endif
#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
    /* data about RREQs we forwarded*/
    uint32_t weight; /* best weight so far */
    int neighbor_list_len;
    uint8_t address[DSR_CONFVAR_SMR_RREQCACHE_NEIGHBOR_LIST_MAX_LEN* ETHER_ADDR_LEN];

    /* data about RREQs we received as target */
    dsr_smr_rreqcache_candidate_t* shortest_delay;
    struct timeval timeout; /* time at which we reply to source  */
    dsr_smr_rreqcache_candidate_t* candidates;
    int complete; /* did we sent the second reply yet? */
#endif
    UT_hash_handle hh;
} dsr_rreqcache_t;

typedef struct __attribute__((__packed__)) dsr_rreqcache_lookup_key {
    uint16_t identification; /* key: part 1 */
    uint8_t target_address[ETHER_ADDR_LEN]; /* key: part 2 */
} dsr_rreqcache_lookup_key_t;

typedef struct __attribute__((__packed__)) dsr_rreqtable {
    uint8_t address[ETHER_ADDR_LEN]; /* key */
    /* RREQs originated by this node to node with hwaddr address*/
    uint16_t rreqs_since_repl;
    uint8_t ttl;
    struct timeval last_rreq; /* only timeval.tv_sec is relevant */
    struct timeval timeout;
    /* RREQs originated by node with hwaddr address */
    dsr_rreqcache_t* rreqcache;
    struct timeval last_used;
    UT_hash_handle hh;
} dsr_rreqtable_t;

#endif /* RREQTABLE_H_ */
