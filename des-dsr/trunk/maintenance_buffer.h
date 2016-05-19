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

#ifndef MAINTENANCE_BUFFER_H_
#define MAINTENANCE_BUFFER_H_

#include "dsr.h"

#define DSR_MAINTENANCE_BUFFER_SUCCESS                             0
#define DSR_MAINTENANCE_BUFFER_ERROR_MEMORY_ALLOCATION            -1
#define DSR_MAINTENANCE_BUFFER_ERROR_NO_SUCH_MESSAGE              -2
#define DSR_MAINTENANCE_BUFFER_ERROR_MESSAGE_ALREADY_IN_BUFFER    -3

#define DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_PASSIVE        0
#define DSR_MAINTENANCE_BUFFER_NEXTHOP_REACHABILITY_ACKREQ_ACK     1

typedef struct __attribute__((__packed__)) dsr_maintenance_buffer {
    uint16_t identification; /* key for uthash */
    dessert_msg_t* msg;
    uint8_t in_iface_address[ETHER_ADDR_LEN];
    uint8_t out_iface_address[ETHER_ADDR_LEN];
    int retransmission_count;
    struct timeval error_threshold;
    UT_hash_handle hh;
} dsr_maintenance_buffer_t;

int maintenance_buffer_passive_ack_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

dessert_per_result_t cleanup_maintenance_buffer(void* data, struct timeval* scheduled, struct timeval* interval);

inline int dsr_maintenance_buffer_add_msg(
    const uint16_t id,
    dessert_msg_t* msg,
    const uint8_t in_iface_address[ETHER_ADDR_LEN],
    const uint8_t out_iface_address[ETHER_ADDR_LEN]);

inline int dsr_maintenance_buffer_add_msg_delay(const uint16_t id, dessert_msg_t* msg, const uint8_t in_iface_address[ETHER_ADDR_LEN], const uint8_t out_iface_address[ETHER_ADDR_LEN], __suseconds_t delay);

inline int dsr_maintenance_buffer_delete_msg(const uint16_t id);

#endif /* MAINTENANCE_BUFFER_H_ */
