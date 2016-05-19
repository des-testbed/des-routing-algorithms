/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
 Extended and debugged by Bastian Blywis, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by Philipp Schmidt
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
*******************************************************************************/

#ifndef ARA_H
#define ARA_H

#include <dessert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <uthash.h>

// default values for CLI parameters
#define ARA_INITIAL_PHEROMONE 0.6   ///< initial pheromone value used in cubic mode
#define ARA_PHEROMONE_INC 0.2       ///< additive increase of pheromone value used in cubic mode
#define ARA_PHEROMONE_DEC 0.1       ///< cubic decrease of pheromone value used in cubic mode
#define ARA_TICK_INTERVAL 5         ///< tick interval [s] to decrease pheromone trails
#define ARA_NODE_DISJOINT 0         ///< enable/disable node disjoint route discovery
#define ARA_PANT_INTERVAL 0         ///< PANT interval [s], 0 = disabled
#define ARA_TTL 64                  ///< time-to-live for generated packets
#define ARA_TRACE_MIN_LENGTH 255    ///< maximum length of broadcast/multicast packets to add a trace extension
#define ARA_TRAP_SLOTCHUNKSIZE 32   ///< maximum number of trapped packets per destination
#define ARA_RETRY_MAX 3             ///< maximum number of times a packets can be trapped
#define ARA_RETRY_DELAY_MS 250      ///< maximum delay in milli-seconds for trapped packets to remain in packet trap
#define ARA_PRUNE_LENGTH 0          ///< do not accept alternative routes that are this many hops longer; 0 disables pruning
#define ARA_RTPROB_BANTS 0          ///< enable/disable route problem BANTs (0 = disabled)
#define ARA_ACK_WAIT_MS 500         ///< maximum time to wait for an acknowledgement [ms]
#define ARA_BACKWARDS_INC 1         ///< increment pheromone trails also in backwards direction
#define ARA_ACK_MISS_CREDIT 5.0     ///< how many times we may not receive an ack before a route is dropped
#define ARA_ACK_CREDIT_INC 0.5      ///< increase credit by this value when incrementing the pheromone value
#define ARA_ADAP_EVAPORATION 0      ///< enable/disable adaptive evaporation mode
#define ARA_ANT_SIZE 0            
// end default values

#define ARA_ACK_CREDIT_MAX 255      ///< maximum number of times an ack may be missed
#define ARA_ACK_CREDIT_MIN 0        ///< depleted credit -> invalidate route
#define ARA_TOO_MANY_NEIGHBORS 255
#define ARA_PRINT_RT_INTERVAL 0     ///< disabled periodic printing of routing table
#define ARA_PRINT_CL_INTERVAL 0     ///< disabled periodic printing of path classification table

/* flags in the DES-SERT message (in msg->u8) */
enum ara_msg_flags {
    ARA_ANT             = 0x01,
    ARA_LOOPING_PACKET  = 0x02,
    ARA_ROUTEFAIL       = 0x04,
    ARA_ROUTEPROBLEM    = 0x08,
    ARA_ACK_REQUEST     = 0x10,
    ARA_ACK_RESPONSE    = 0x20
};

/* local flags for ara_proc_t */
enum ara_proc_flags {
    ARA_DELIVERABLE   = 0x0001, ///< nexthop available or packet destined for this host
    ARA_DUPLICATE     = 0x0002, ///< duplicate of an already received packet
    ARA_FLOOD         = 0x0004, ///< forward by broadcast
    ARA_FORWARD       = 0x0008, ///< forward to nexthop
    ARA_LOCAL         = 0x0010, ///< deliver to local host
    ARA_LOOPING       = 0x0020, ///< packet is looping
    ARA_ORIG_LOCAL    = 0x0040, ///< packet has been created by this host
    //#define ARA_VALID = 0x0080, ///<
    ARA_RT_UPDATE_IGN = 0x0100, ///< do not update routing table
    ARA_RT_SEND_BANT  = 0x0200, ///< send BANT before forwarding packet
    ARA_FANT_LOCAL    = 0x0400, ///< packet is a FANT for this host
    ARA_BANT_LOCAL    = 0x0800, ///< packet is a BANT for this host
    ARA_LAST_HOP      = 0x1000  ///< last hop to destination
};

/**
 * Types used for ARA's DES-SERT extensions
 */
enum _ARA_EXT_TYPES {
    ARA_EXT_FANT = DESSERT_EXT_USER,    ///< forward ANT
    ARA_EXT_BANT,                       ///< backward ANT
    // ARA_EXT_ROUTEFAIL                   ///< route fail message
};

/**
* ARA forwarding modes
*/
enum _ara_forw_modes {
    ARA_FORW_B = 0x00,///< forward over best link
    ARA_FORW_W,       ///< random weighted
    ARA_FORW_R,       ///< random
    ARA_FORW_RR,      ///< round robin
};

/**
* ARA pheromone trail modes
*/
enum _ara_ptrail_modes {
    ARA_PTRAIL_CLASSIC = 0x00,///< use TTL as pheromone value
    ARA_PTRAIL_CUBIC,         ///< use cubic function with values from [0,1]
    ARA_PTRAIL_LINEAR
};

/**
 * Acknowledgement modes
 */
enum _ara_ack_modes {
    ARA_ACK_LINK = 0x00,
    ARA_ACK_PASSIVE,
    ARA_ACK_NETWORK,
    ARA_ACK_DISABLED
};

/**
 * Return values of the route update functions
 */
typedef enum _ara_route_updated_results {
    ARA_RT_FAILED = 0x00,
    ARA_RT_NEW,
    ARA_RT_UPDATED,
    ARA_RT_DISCARDED,
    ARA_RT_CREDIT_DECREASED,
    ARA_RT_CREDIT_DEPLETED,
    ARA_RT_CREDIT_INCREASED,
    ARA_RT_CREDIT_MAX,
    ARA_RT_DELETED,
} ara_rt_update_res_t;

extern uint8_t ara_defttl;      ///< default ttl for generated packets

/*** CLI PARAMETERS ***/
extern uint16_t ara_retry_delay_ms; ///< delay for trapped package retransmits [ms]
extern uint8_t ara_retry_max;   ///< maximum number of retransmits for trapped packets

extern uint8_t ara_forw_mode;   ///< forwarding mode (selection of next hop)
extern uint8_t ara_ptrail_mode; ///< pheromone trail mode
extern uint8_t ara_ack_mode;    ///< acknowledgement mode

extern uint8_t ara_backwards_inc; ///< increment pheromone trails also backwards?
extern uint8_t ara_ndisjoint;   ///< generate only node disjoint paths if enabled
extern double ara_prune_routes; ///< alternative routes that are this many times longer than the shortest one are discarded
extern uint8_t ara_rtprob_bants;///< send a BANT if route problems are detected
extern uint8_t ara_pant_interval;///< send PANT after x seconds when no other packets have been sent
extern uint16_t ara_ack_wait_ms;///< time until an acknowledgement should arrive
extern double ara_ack_miss_credit; ///< how many times we may not receive an ack before a route is dropped in PASSIVE mode
extern double ara_ack_credit_inc; ///< increment credit by this value when a ack is received
extern uint8_t ara_adap_evaporation; ///< enable/disable adaptive evaporation mode

extern double rt_min_pheromone; ///< minimum pheromone value for entries in routing table
extern double rt_initial;       ///< initial pheromone value for CUBIC mode
extern double rt_inc;           ///< additive increase of pheromone trail
extern double rt_delta_q;       ///< multiplicative aging value for routing table entries
extern uint8_t rt_tick_interval;///< aging interval for routing table entries

extern size_t ara_trace_broadcastlen; ///< append trace ext. if message is <= this length [bytes]

extern uint8_t ara_print_rt_interval_s;
extern uint8_t ara_print_cl_interval_s;

extern uint8_t ara_clsf_lossmin;     ///< min packet loss in percent (effective path if less) 
extern uint8_t ara_clsf_lossmax;     ///< max packet loss in percent (acceptable path if less)
extern uint8_t ara_clsf_tick_interval_s; ///< tick interval for path classification in seconds 
extern uint8_t ara_clsf_skiptimes;       ///< number of times to skip the classification (start phase)
extern uint8_t ara_clsf_sw_size;         ///< sliding window size for remembering classification results
extern uint16_t ara_clsf_sender_rate;    ///< sendrate of the sending node in packets per second
extern uint8_t ara_classify;             ///< classify paths via adding extensions if enabled

extern size_t ara_ant_size;    ///< Packet Size of Fant Bant and Pant>
/*** end of CLI PARAMETERS ***/


typedef u_char ara_address_t[ETHER_ADDR_LEN];
typedef uint16_t ara_seq_t;
#define ara_seq_overflow(x, y) ((x>y)&&((x-y)>(((ara_seq_t)-1)/2)))


extern struct cli_command* cli_cfg_set;

int cli_showpackettrap(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showroutingtable(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_flushroutingtable(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_flushclassifictable(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showloopprotect_table(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showclassifictable(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_showloopprotect_statistics(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_flush_ack_monitor(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_ack_monitor(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_flush_loopprotec_table(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_flush_rmnt(struct cli_def* cli, char* command, char* argv[], int argc);

ara_seq_t ara_seq_next();
void ara_addseq(dessert_msg_t* msg);
double get_pheromone(dessert_msg_t* msg);
int8_t ara_rt_route_nhopnumber(ara_address_t dst);
#define ara_proc_get(x) ((ara_proc_t *) &((x)->lbuf))

void ara_rt_init();
ara_rt_update_res_t ara_rt_update(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double delta_pheromone, ara_seq_t seq, uint8_t ttl);
ara_rt_update_res_t ara_rt_delete(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double delta_pheromone, ara_seq_t seq, uint8_t ttl);
inline double ara_ptrailmode2_pheromone(uint8_t ttl);
ara_rt_update_res_t ara_rt_inc_pkt_count(ara_address_t dst, uint32_t len);
dessert_per_result_t ara_print_rt_periodic(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t ara_print_cl_periodic(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t ara_rt_tick(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t ara_loopprotect_tick(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t ara_classification_tick(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t ara_ack_tick(void* data, struct timeval* scheduled, struct timeval* interval);

void ara_loopprotect_init();
void ara_classification_init();

void ara_maintainroute_stamp(ara_address_t dst);

int8_t ara_ack_waitfor(ara_address_t src, ara_seq_t seq, ara_address_t nexthop, ara_address_t dst, dessert_meshif_t* iface);
int8_t ara_ack_eval_packet(ara_address_t src, ara_seq_t seq, ara_address_t nexthop);
void ara_ack_init();
ara_rt_update_res_t ara_rt_modify_credit(ara_address_t dst, ara_address_t nexthop, dessert_meshif_t* iface, double amount);

/** local processing buffer */
typedef struct ara_proc {
    uint16_t flags;                     ///< processing flags
    ara_address_t src;                  ///< source
    ara_address_t prevhop;              ///< previous hop
    dessert_meshif_t* iface_in;         ///< input interface
    dessert_meshif_t* iface_out;        ///< output interface
    ara_address_t nexthop;              ///< next hop
    ara_address_t dst;                  ///< destination
    ara_seq_t seq;                      ///< sequence number
    int trapcount;                      ///< trap count
    //ara_address_t routefail_src;        ///< routefail src
    //ara_seq_t routefail_seq;            ///< routefail seq
} ara_proc_t;

/* ara nexthop structure used for the routing table */
typedef struct ara_nexthop {
    ara_address_t nexthop;          ///< nexthop for this route
    const dessert_meshif_t* iface;  ///< interface to reach nexthop
    double pheromone;               ///< pheromone value
    double credit;                  ///< how many times we may not receive an ack before the route is dropped
    uint8_t ttl;                    ///< TTL for this entry
    struct ara_nexthop* prev;       ///< needed for doubly-linked list
    struct ara_nexthop* next;       ///< needed for doubly-linked list
} ara_nexthop_t;

/** routefail extension */
// typedef struct ara_ext_routefail {
//     ara_address_t src;
//     ara_seq_t seq;
// } ara_ext_routefail_t;

void ara_init_cli();

int trap_packet(ara_address_t dst, dessert_msg_t* pkg, size_t len, dessert_msg_proc_t* proc, dessert_frameid_t id);

int untrap_packets(ara_address_t dst, dessert_meshrxcb_t* c);

dessert_cb_result ara_tun2ara(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* tunif, dessert_frameid_t id);

dessert_cb_result ara_sendfant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_ara2tun(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

dessert_cb_result ara_checkl2dst(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_checkl2dst_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_makeproc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_makeproc_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_addext_classification(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_updateext_classification(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_sendbant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_forward_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_noroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_handle_fant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_handle_ack_request(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_per_result_t ara_routefail_untrap_packets(void* data, struct timeval* scheduled, struct timeval* interval);

dessert_cb_result ara_retrypacket(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_handle_loops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_dropdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

dessert_cb_result ara_tagroutefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);


dessert_cb_result ara_routeupdate_data(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_routeupdate_ant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_handle_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_getroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_getroute_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_checkloop(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_checkdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_maintainroute_pant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

dessert_cb_result dessert_msg_ifaceflags_cb_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_maintainroute_timestamp(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id);

dessert_cb_result ara_handle_routeproblem(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface_in, dessert_frameid_t id);

#endif
