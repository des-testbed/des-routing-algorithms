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
#ifndef BLACKLIST_H_
#define BLACKLIST_H_

#include "dsr.h"

/******************************************************************************
 *
 * Blacklist
 *
 * "When a node using the DSR protocol is connected through a network
 *  interface that requires physically bidirectional links for unicast
 *  transmission, the node MUST maintain a blacklist.  The blacklist is a
 *  table, indexed by neighbor node address, that indicates that the link
 *  between this node and the specified neighbor node may not be
 *  bidirectional." RFC4728 p33
 ******************************************************************************/

#define DSR_BLACKLIST_FLAG_PROBABLE                    0x02
#define DSR_BLACKLIST_FLAG_QUESTIONABLE                0x04

#define DSR_BLACKLIST_SUCCESS                             0
#define DSR_BLACKLIST_ERROR_MEMORY_ALLOCATION            -1
#define DSR_BLACKLIST_ERROR_NO_SUCH_NODE                 -2
#define DSR_BLACKLIST_ERROR_NODE_ALREADY_IN_LIST         -3

/** Add a node to the blacklist */
int dsr_blacklist_add_node(uint8_t address[ETHER_ADDR_LEN]);
/** Remove a node from the blacklist */
int dsr_blacklist_remove_node(uint8_t address[ETHER_ADDR_LEN]);
/** Determine the state of a node in the Blacklist*/
int dsr_blacklist_get_state(uint8_t address[ETHER_ADDR_LEN]);
/** Set the state of a node in the Blacklist */
int dsr_blacklist_set_state(uint8_t address[ETHER_ADDR_LEN], int new_state);

dessert_per_result_t cleanup_blacklist(void* data, struct timeval* scheduled, struct timeval* interval);

typedef struct __attribute__((__packed__)) dsr_blacklist {
    uint8_t neighbor[ETHER_ADDR_LEN]; /* key */
    uint8_t state;
    struct timeval last_updated; /* only timeval.tv_sec is relevant */
    UT_hash_handle hh;
} dsr_blacklist_t;

#endif /* BLACKLIST_H_ */
