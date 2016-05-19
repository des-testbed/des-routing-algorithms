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

#include <dessert.h>
#include "../config.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

#ifndef BATMAN_PIPELINE_H
#define BATMAN_PIPELINE_H

#define BATMAN_OGM_UFLAG 128
#define BATMAN_OGM_DFLAG 64
#define BATMAN_OGM_RFLAG 32

/** OGM - Originator Message */
struct batman_msg_ogm {
    /** OGM Version */
    uint8_t version;
    /** Flags : U D R 0 0 0 0 0
     *  U - unidirectional flag. Indicates whether the neighboring node is bidirectional or not.
     *  D - Is-direct-link flag. Indicates whether a Node is a direct neighbor or not.
     *  R - reset flag. Indicates whether the destination router is new. Old entrys for this
     *  				destination must be deleted.
     */
    uint8_t flags;
    /** Sequence number */
    uint16_t sequence_num;
    /** the number of output interface */
    uint8_t output_iface_num;
    /** ethernet address of next hop interface */
    uint8_t next_hop[ETH_ALEN];
    /**
     * Precursor list. Used to save last OGM_PRECURSOR_LIST_SIZE
     * addresses of nodes that processed this OGM
     * to prevent multiple processing of same OGM.
     * HINT: The approach to process only first incoming OGM with
     * given sequence number have numerous issues.
     */
    uint8_t precursor_list[OGM_PREC_LIST_SIZE* ETH_ALEN];
    uint8_t precursors_count;
}  __attribute__((__packed__));

/** Inverted routing table */
struct batman_ogm_invrt {
    uint8_t source_addr[ETH_ALEN];
    uint8_t output_iface_num;
    uint8_t next_hop[ETH_ALEN];
} __attribute__((__packed__));

struct batman_msg_brc {
    uint16_t		id;
} __attribute__((__packed__));

/**
 * Struct for routing log sequence number
 */
struct rl_seq {
    uint16_t 	seq_num;
    uint8_t	hop_count;
    uint8_t	precursor_if_list[OGM_PREC_LIST_SIZE* ETH_ALEN];
    uint8_t	prec_iface_count;
} __attribute__((__packed__));


// ------------- pipeline -----------------------------------------------------

/**
 * Encapsulate packets as dessert_msg,
 * set NEXT HOP if known and send via B.A.T.M.A.N routing protocol
 */
int batman_sys2rp(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc,
                  dessert_sysif_t* sysif, dessert_frameid_t id);

/** forward packets received via B.A.T.M.A.N to tun interface **/
int rp2sys(dessert_msg_t* msg, size_t len,
           dessert_msg_proc_t* proc, const dessert_meshif_t* iface, dessert_frameid_t id);

int namtab_msg_check_cb(dessert_msg_t* msg, size_t len,
                        dessert_msg_proc_t* proc, const dessert_meshif_t* iface,
                        dessert_frameid_t id);

/** drop errors (drop corrupt packets, packets from myself ... )*/
int batman_drop_errors(dessert_msg_t* msg, size_t len,
                       dessert_msg_proc_t* proc, const dessert_meshif_t* iface, dessert_frameid_t id);

/** forward packets received via B.A.T.M.A.N to destination **/
int batman_fwd2dest(dessert_msg_t* msg, size_t len,
                    dessert_msg_proc_t* proc, const dessert_meshif_t* iface, dessert_frameid_t id);

/** handle OGM message */
int batman_handle_ogm(dessert_msg_t* msg, size_t len,
                      dessert_msg_proc_t* proc, const dessert_meshif_t* iface, dessert_frameid_t id);

// ------------------------------ periodic ----------------------------------------------------

/** periodic send OGM message */
int batman_periodic_send_ogm(void* data, struct timeval* scheduled, struct timeval* interval);

/** periodic print routing table into logging file */
int batman_periodic_log_rt(void* data, struct timeval* scheduled, struct timeval* interval);

/** clean up database from old entrys */
int batman_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval);

/**
 *  Register send_ogm callback to periodic pipelien and set/change
 * 	ist interval betwen to OGMs to omg_int
 */
int batman_periodic_register_send_ogm(time_t ogm_int);


#endif
