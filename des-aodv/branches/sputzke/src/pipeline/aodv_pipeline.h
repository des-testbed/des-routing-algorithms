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

#ifndef AODV_PIPELINE
#define AODV_PIPELINE

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

#include <dessert.h>
#include "../config.h"

extern pthread_rwlock_t pp_rwlock;

/**
 * Unknown sequence number
 */
#define AODV_FLAGS_RREQ_U 			(1 << 11)

/**
 * Destination only flag
 */
#define AODV_FLAGS_RREQ_D			(1 << 12)
/**
 * Aknowledgement required
 */
#define AODV_FLAGS_RREP_A			(1 << 6)

/**
 * Not delete flag of RERR
 */
#define AODV_FLAGS_RERR_N			(1 << 7)

/** RREQ - Route Request Message */
struct aodv_msg_rreq {
    /**
     * flags format: J R G D U 0 0 0   0 0 0 0 0 0 0 0
     * J - Join flag; reserved for multicast //outdated
     * R - Repair flag; reserved for multicast //outdated
     * G - Gratuitous RREP flag; indicates whether a gratuitous
     * 	   RREP should be unicast to the node specified in the ether_dhost
     * D - Destination only flag; indicates only the destiantion may respond to this RREQ
     * U - Unknown sequence number; indicates the destination sequence number is unknown
     */
    uint16_t		flags;

    uint32_t		destination_sequence_number;

    uint32_t		originator_sequence_number;
} __attribute__((__packed__));

/** RREP - Route Reply Message */
struct aodv_msg_rrep {
    /**
     * flags format: R A 0 0 0 0 0 0
     * R - repair flag;
     * A - acknowledgement required;
     */
    uint8_t		flags;

    uint32_t		destination_sequence_number;
    /**
     * LifeTime:
     * The time in millisecond for which nodes receiving the RREP consider the
     * route to be valid.
     */
    time_t			lifetime;

    uint8_t qos_metric;
    double qos_constraint;
} __attribute__((__packed__));

/** RERR - Route Error Message */
struct aodv_msg_rerr {
    /**
     * flags format: N 0 0 0 0 0 0 0
     * N - No delete flag; set when a node has performed a local repair of a link
     */
    uint8_t		flags;
    /** The number of interfaces of the RERR last hop */
    uint8_t 		iface_addr_count;
    /** all of mesh interfaces of current host listed i this message */
    mac_addr ifaces[MAX_MESH_IFACES_COUNT];
} __attribute__((__packed__));

/** HELLO - Hello Message */
struct aodv_msg_hello {
    /** rcvd hellos in last interval */
    uint8_t 		hello_rcvd_count;
    /** Hello Interval in ms */
    uint16_t		hello_interval;
} __attribute__((__packed__));

typedef struct aodv_rreq_series aodv_rreq_series_t;

// ------------- pipeline -----------------------------------------------------
int aodv_handle_hello(dessert_msg_t* msg, uint32_t len,
                      dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_handle_rreq(dessert_msg_t* msg, uint32_t len,
                     dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_handle_rerr(dessert_msg_t* msg, uint32_t len,
                     dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_handle_rrep(dessert_msg_t* msg, uint32_t len,
                     dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_forward_broadcast(dessert_msg_t* msg, uint32_t len,
                           dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_forward_multicast(dessert_msg_t* msg, uint32_t len,
                           dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int aodv_forward(dessert_msg_t* msg, uint32_t len,
                 dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

/**
 * Encapsulate packets as dessert_msg,
 * set NEXT HOP if known and send via AODV routing protocol
 */
int aodv_sys2rp(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc,
                dessert_sysif_t* sysif, dessert_frameid_t id);

int aodv_sys_drop_multicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t* proc,
                            dessert_sysif_t* sysif, dessert_frameid_t id);
/** forward packets received via AODV to tun interface */
int aodv_local_unicast(dessert_msg_t* msg, uint32_t len,
                       dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

/** drop errors (drop corrupt packets, packets from myself and etc...)*/
int aodv_drop_errors(dessert_msg_t* msg, uint32_t len,
                     dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

void aodv_send_packets_from_buffer(mac_addr ether_dhost, mac_addr next_hop, dessert_meshif_t* iface);

// ------------------------------ periodic ----------------------------------------------------

dessert_per_result_t aodv_periodic_send_hello(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t aodv_periodic_send_rreq(void* data, struct timeval* scheduled, struct timeval* interval);

/** clean up database from old entries */
dessert_per_result_t aodv_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval);

dessert_msg_t* aodv_create_rerr(aodv_link_break_element_t** destlist);

dessert_per_result_t aodv_periodic_scexecute(void* data, struct timeval* scheduled, struct timeval* interval);

// ------------------------------ metric ----------------------------------------------------

int aodv_metric_do(metric_t* metric, mac_addr last_hop, dessert_meshif_t* iface, struct timeval* timestamp);

// ------------------------------ gossip ----------------------------------------------------

int aodv_gossip(dessert_msg_t* msg);
int aodv_gossip_0();
void aodv_gossip_capt_rreq(dessert_msg_t *msg);

// ------------------------------ helper ------------------------------------------------------

void aodv_pipeline_delete_series_ether(mac_addr addr);
void aodv_send_rreq(mac_addr dhost_ether, struct timeval* ts);
void aodv_send_rreq_repeat(struct timeval* ts, aodv_rreq_series_t* series);

#endif
