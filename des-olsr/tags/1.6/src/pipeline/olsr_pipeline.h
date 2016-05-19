/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
    http://www.des-testbed.net
*******************************************************************************/

#ifndef OLSR_PIPELINE
#define OLSR_PIPELINE

#include <dessert.h>
#include "../android.h"


// ------------- message formats ----------------------------------------------

// ---- hello -----
// all HELLO messages consists of hello_header and optional specification
// of local interface following by specification of all neighbor
// interfaces connected to current local interface.

struct olsr_msg_hello_hdr {
    uint16_t   seq_num;        ///< Sequence number of HELLO message. Needed to compute link quality
    /*uint8_t  hold_time;      ///< Hold time of information in this message*/
    uint8_t    hello_interval; ///< Interval between two HELLO messages
    uint8_t    willingness;    ///< Willingness of router to retransmit broadcast messages
    uint8_t    n_iface_count;  ///< Number of neighbor MANET interfaces  introduced in this HELLO message
} __attribute__((__packed__));

struct olsr_msg_hello_niface {
    /**
    * Link code format:
    * +----+----+----+----+----+----+---------+
    * | 0  | 0  | 0  | 0  | 0  | 0  | link_t  |
    * +----+----+----+----+----+----+---------+
    * where
    * link_t = UNSPEC_LINK | ASYM_LINK | SYM_LINK | LOST_LINK
    */
    uint8_t    link_code;
    /**
    * Ethernet address neighbor interface connected to previous introduced
    * local interface of host that have originated this HELLO message.
    */
    uint8_t    n_iface_addr[ETH_ALEN];
    uint8_t    quality_from_neighbor;  ///< Link quality by sending data from neighbor to HELLO originator
} __attribute__((__packed__));

/**
* Neighbor description
*/
struct olsr_msg_hello_ndescr {
    /**
    * Link code format:
    * +----+----+----+----+----+----+---------+
    * | 0  | 0  | 0  | 0  | 0  | 0  | neigh_t |
    * +----+----+----+----+----+----+---------+
    * where
    * neigh_t = SYM_NEIGH | MPR_NEIGH | NOT_NEIGH
    */
    uint8_t    neigh_code;
    uint8_t    n_main_addr[ETH_ALEN];  ///< Main address of host
    uint8_t    link_quality;           ///< Link quality to neighbor in %
} __attribute__((__packed__));

// ---- TC -----

struct olsr_msg_tc_hdr {
    uint16_t    seq_num;    ///< Sequence number of this TC message to avoid multiple re-sending
    //uint8_t  hold_time;  ///< Hold time of information in this message
    uint8_t     tc_interval;///< Interval between two HELLO messages
    uint8_t     neighbor_count; ///< Number of 1hop neighbors of TC originator introduced in this TC
} __attribute__((__packed__));

/**
* Neighbor description
*/
struct olsr_msg_tc_ndescr {
    uint8_t     link_quality;           ///< quality of link between originator and this neighbor
    uint8_t     n_main_addr[ETH_ALEN];  ///< main address of host
} __attribute__((__packed__));

/**
* Struct for routing log sequence number
*/
struct rl_seq {
    uint32_t   seq_num;
    uint8_t    hop_count;
} __attribute__((__packed__));

// ----------

extern uint8_t pending_rtc; // pending recalculation of routing table
extern pthread_rwlock_t pp_rwlock;

// -------------------- broadcast id ------------------------------------------

struct olsr_msg_brc {
    uint32_t   id;
} __attribute__((__packed__));

// ------------- pipeline -----------------------------------------------------

dessert_cb_result olsr_handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);
dessert_cb_result olsr_handle_tc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);
dessert_cb_result olsr_fwd2dest(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

/**
* Encapsulate packets as dessert_msg,
* sets NEXT HOP if known and send via OLSR routing protocol
*/
dessert_cb_result olsr_sys2rp(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* tunif, dessert_frameid_t id);

/** forward packets received via OLSR to tun interface */
dessert_cb_result rp2sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc,  dessert_meshif_t* iface, dessert_frameid_t id);

/** drop errors (drop corrupt packets, packets from myself and etc.)*/
dessert_cb_result olsr_drop_errors(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

// ----- Pipeline callbacks ---------- //


// ------------------------------ periodic ----------------------------------------------------

dessert_per_result_t olsr_periodic_send_hello(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t olsr_periodic_send_tc(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t olsr_periodic_build_routingtable(void* data, struct timeval* scheduled, struct timeval* interval);

/** clean up database from old entrys */
dessert_per_result_t olsr_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval);

#endif
