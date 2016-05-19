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

#ifndef AODV_CONFIG
#define AODV_CONFIG

#include <dessert.h>

#define RREQ_RETRIES				5 /* ferhat=5 rfc=2 */
#define RREQ_RATELIMIT				10 /* rfc=10 */
#define TTL_START					1 /* rfc=1 */
#define TTL_INCREMENT				2 /* rfc=2 */
#define TTL_THRESHOLD				7 /* rfc=7 */
#define TTL_MAX						UINT8_MAX

#define ACTIVE_ROUTE_TIMEOUT		3000 /* ms rfc=3000 */
#define ALLOWED_HELLO_LOST			4 /* christian=4 rfc=2 */
#define NODE_TRAVERSAL_TIME			20 /* ms christian=2 rfc=40 */
#define NET_DIAMETER				16 /* christian=8 rfc=35 */
#define NET_TRAVERSAL_TIME			(2 * NODE_TRAVERSAL_TIME * NET_DIAMETER) /* rfc */
#define BLACKLIST_TIMEOUT			(RREQ_RETRIES * NET_TRAVERSAL_TIME) /* rfc */
#define MY_ROUTE_TIMEOUT			(2 * ACTIVE_ROUTE_TIMEOUT) /* rfc */
#define PATH_DESCOVERY_TIME			(2 * NET_TRAVERSAL_TIME) /* rfc */
#define RERR_RATELIMIT				10 /* rfc=10 */

#define RREQ_EXT_TYPE				DESSERT_EXT_USER
#define RREP_EXT_TYPE				(DESSERT_EXT_USER + 1)
#define RERR_EXT_TYPE				(DESSERT_EXT_USER + 2)
#define RERRDL_EXT_TYPE				(DESSERT_EXT_USER + 3)
#define HELLO_EXT_TYPE				(DESSERT_EXT_USER + 4)
#define BROADCAST_EXT_TYPE			(DESSERT_EXT_USER + 5)

#define FIFO_BUFFER_MAX_ENTRY_SIZE	UINT32_MAX /* maximal packet count that can be stored in FIFO for one destination */
#define DB_CLEANUP_INTERVAL			NET_TRAVERSAL_TIME /* not in rfc */
#define SCHEDULE_CHECK_INTERVAL		20 /* ms not in rfc */

#define HELLO_INTERVAL				1000 /* ms rfc=1000 */

#define HELLO_SIZE					128 /* bytes */
#define RREQ_SIZE					128 /* bytes */

#define GOSSIP_P					1 /* flooding */
#define DEST_ONLY					false /* only destination answer a RRequest */
#define RING_SEARCH			 		true /* use expanding ring search */

typedef enum aodv_gossip {
    GOSSIP_NONE,
    GOSSIP_0,
    GOSSIP_1,
    GOSSIP_3,
    PISSOG_0,
    PISSOG_3
} aodv_gossip_t;

typedef enum aodv_metric {
    AODV_METRIC_RFC = 0,
    AODV_METRIC_HOP_COUNT,
    AODV_METRIC_RSSI,
    AODV_METRIC_ETX_ADD,
    AODV_METRIC_ETX_MUL,
    AODV_METRIC_PDR
} aodv_metric_t;

typedef uint16_t metric_t;
#define AODV_PRI_METRIC				PRIu16
#define AODV_MAX_METRIC				UINT16_MAX /* the type of the variable in the packets -> u16 it is the maximum value of a metric */
#define AODV_METRIC_STARTVAL		0

#define PDR_TRACKING_FACTOR			10 /* length of pdr tracking interval for a nb := nb_hello_interval * PDR_TRACKING_FACTOR */
#define PDR_TRACKING_PURGE_FACTOR	2  /* timeout for nb entry in pdr tracker := nb_hello_interval * PDR_TRACKING_FACTOR * PDR_TRACKING_PURGE_FACTOR */
#define PDR_MIN_TRACKING_INTERVAL	500 /* minimum tracking interval in ms */

#define REPORT_RT_STR_LEN			150 /* default: 150 (should not be switched, needed for string inits)*/
#define RREQ_INTERVAL				0 /* off */

#define AODV_DATA_SEQ_TIMEOUT		MY_ROUTE_TIMEOUT /* wait MY_ROUTE_TIMEOUT for dropping data seq information -> this is the time a route is valid */

/**
 * Schedule type = repeat RREQ
 */
#define AODV_SC_REPEAT_RREQ			2

/**
 * Schedule type = send out route error for given next hop
 */
#define AODV_SC_SEND_OUT_RERR		3
#define AODV_SC_SEND_OUT_RWARN		4
#define AODV_SC_UPDATE_RSSI			5

#define AODV_SIGNAL_STRENGTH_THRESHOLD	0 /* dbm (off)*/
#define AODV_SIGNAL_STRENGTH_INIT		-120

// --- Database Flags
#define AODV_FLAGS_UNUSED				0
#define AODV_FLAGS_ROUTE_INVALID 		1
#define AODV_FLAGS_NEXT_HOP_UNKNOWN		(1 << 1)
#define AODV_FLAGS_ROUTE_WARN			(1 << 2)
#define AODV_FLAGS_ROUTE_LOCAL_USED		(1 << 3)
#define AODV_FLAGS_ROUTE_NEW	    	(1 << 4)

#define MAX_MESH_IFACES_COUNT			8

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

extern dessert_periodic_t* 			send_hello_periodic;

extern dessert_periodic_t* 			send_rreq_periodic;
extern uint16_t 					rreq_interval;

extern uint16_t 					hello_size;
extern uint16_t 					hello_interval;
extern uint16_t 					rreq_size;
extern uint16_t						tracking_factor;
extern double 						gossip_p;
extern bool							dest_only;
extern bool 						ring_search;
extern aodv_gossip_t				gossip_type;
extern aodv_metric_t				metric_type;
extern uint16_t 					metric_startvalue;
extern int8_t						signal_strength_threshold;

typedef struct aodv_link_break_element {
    mac_addr host;
    uint32_t sequence_number;
    struct aodv_link_break_element* next;
    struct aodv_link_break_element* prev;
} aodv_link_break_element_t;

typedef struct aodv_mac_seq {
    mac_addr host;
    uint32_t sequence_number;
} __attribute__((__packed__)) aodv_mac_seq_t;

#define MAX_MAC_SEQ_PER_EXT (DESSERT_MAXEXTDATALEN / sizeof(aodv_mac_seq_t))

#endif
