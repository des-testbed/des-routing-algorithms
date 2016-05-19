/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
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

#include <dessert.h>
#include <unistd.h>
#include "./contrib/uthash-1.5/uthash.h"

/* debugging switches */
#if 0
#define ARA_ANT_DEBUG
#define ARA_LOOPPROTECT_DEBUG
#define ARA_LOOPHANLDERS_DEBUG
#define ARA_RT_GET_DEBUG
#define ARA_RT_UPDATE_DEBUG
#define PACKETTRAP_DEBUG
#endif

/* constants */
#define ARA_EXT_FANT DESSERT_EXT_USER
#define ARA_EXT_BANT (DESSERT_EXT_USER+1)
#define ARA_EXT_PANT (DESSERT_EXT_USER+2)
#define ARA_EXT_ROUTEFAIL (DESSERT_EXT_USER+3)
#define ARA_PING (DESSERT_EXT_USER+4)
#define ARA_PONG (DESSERT_EXT_USER+5)


/** forward over best link */
#define ARA_FORW_B 0x1
/** fowrard weighted */
#define ARA_FORW_W 0x2

/** PARAMETER - delay for unconditional trapped package retransmit */
extern int trapretry_delay;
/** PARAMETER - maximum trapped package retransmit */
extern int trapretry_max;
/** PARAMETER - forwarding mode */
extern char ara_forw_mode;
/** PARAMETER - default ttl */
extern uint8_t ara_defttl;
/** PARAMETER - minimum pheromone value to keep rte in table */
extern double rt_min_pheromone;
/** PARAMETER - multiplicative ageing value for routing table*/
extern double rt_delta_q;
/** PARAMETER - ageing interval for routing table */
extern int rt_tick_interval;
/** PARAMETER - send pant after x seconds without own transmit */
extern int ara_pant_interval;
/** PARAMETER - append trace header to every broadcast/multicast packet
                smaller than  ara_trace_broadcastlen abusing them to find 
                or maintain alternate pathes */
extern size_t ara_trace_broadcastlen;

/* types */
typedef u_char ara_address_t[ETHER_ADDR_LEN];
#define ARA_ADDR_LEN ETHER_ADDR_LEN
typedef uint16_t ara_seq_t;
#define ara_seq_overflow(x, y) ((x>y)&&((x-y)>(((ara_seq_t)-1)/2)))


/* cli variables */
struct cli_def *cons;
extern struct cli_command *cli_cfg_set;

/* cli commands */
int cli_addmeshif(struct cli_def *cli, char *command, char *argv[], int argc); 
int cli_cfgtapif(struct cli_def *cli, char *command, char *argv[], int argc); 
int cli_packetdump(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_nopacketdump(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_showpackettrap(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_showroutingtable(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_showloopprotect_table(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_showloopprotect_statistics(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_ping(struct cli_def *cli, char *command, char *argv[], int argc); 
int cli_traceroute(struct cli_def *cli, char *command, char *argv[], int argc); 




/* utility functions */
ara_seq_t ara_seq_next();
void ara_addseq(dessert_msg_t *msg);
// ara_seq_t ara_getseq(dessert_msg_t *msg);
double get_pheromone(dessert_msg_t *msg);
#define ara_proc_get(x) ((ara_proc_t *) &((x)->lbuf))

/* routing table functions */
void ara_rt_init();
int ara_rt_update(ara_address_t dst, ara_address_t nexthop, const dessert_meshif_t *iface, double delta_pheromone, ara_seq_t seq );
#define ARA_RT_FAILED 0x0
#define ARA_RT_NEW 0x1
#define ARA_RT_UPDATED 0x2

/* loop protect functions */
void ara_loopprotect_init();

/** local processing buffer */
typedef struct ara_proc {
    /** processing flags */
    uint16_t flags;
    /* source */
    ara_address_t src;
    /** previous hop */
    ara_address_t prevhop;
    /** input interface */
    const dessert_meshif_t *iface_in;
    /** output interface */
    const dessert_meshif_t *iface_out;
    /** next hop */
    ara_address_t nexthop;
    /** destination */
    ara_address_t dst;
    /**  sequence number */
    ara_seq_t seq;
    /** trap count */
    int trapcount;
    /** routefail src */
    ara_address_t routefail_src;
    /** routefail seq */
    ara_seq_t routefail_seq;
} ara_proc_t;

/* remote flags (in msg->u8) */
#define ARA_ANT             0x01
#define ARA_LOOPING_PACKET  0x02
#define ARA_ROUTEFAIL       0x04
#define ARA_ROUTEPROBLEM    0x08

/* local flags */
#define ARA_DELIVERABLE   0x0001
#define ARA_DUPLICATE     0x0002
#define ARA_FLOOD         0x0004
#define ARA_FORWARD       0x0008
#define ARA_LOCAL         0x0010
#define ARA_LOOPING       0x0020
#define ARA_ORIG_LOCAL    0x0040
#define ARA_VALID         0x0080
#define ARA_RT_UPDATE_IGN 0x0100

/** routefail extension */
typedef struct ara_ext_routefail {
    ara_address_t src;
    ara_seq_t seq;
} ara_ext_routefail_t;




/* packettrap functions */
int trap_packet(ara_address_t dst, dessert_msg_t *pkg, size_t len, dessert_msg_proc_t *proc, dessert_frameid_t id);
int untrap_packets(ara_address_t dst, dessert_meshrxcb_t* c);


/* workers from ara_packethandlers.c */
int ara_tun2ara (struct ether_header *eth, size_t len, dessert_msg_proc_t *proc, dessert_tunif_t *tunif, dessert_frameid_t id);
int ara_ara2tun(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
int ara_checkl2dst(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_makeproc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_handle_fant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_routefail_untrap_packets(void *data, struct timeval *scheduled, struct timeval *interval);
int ara_retrypacket(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_handle_loops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_dropdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);

/* workers from ara_rt.c */
int ara_routeupdate_rflow(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_routeupdate_ant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_routeupdate_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);
int ara_getroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);

/* workers from ara_loopprotect.c */
int ara_checkloop(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);

/* workers from ara_rmnt.c */
int ara_maintainroute_pant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id);

/* workers from ara_trace.c */
int ara_pingpong(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);


