/******************************************************************************
 Copyright 2009, Alexander Ende, Freie Universitaet Berlin
 (FUB).
 All rights reserved.

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

#ifndef GOSSIP_NHDP
#define GOSSIP_NHDP

#define USE_MPRS    true

/* L_status values
   Used for all occurences of LINK_STATUS values. */
//#define LOST        0    ///< a lost link, due to low quality
//#define SYMMETRIC   1    ///< a symmetric link between two neighbors
//#define HEARD       2    ///< a link that is considered heard
//#define PENDING     3    ///< a pending link, not yet established
enum link_types {
    LOST,
    SYMMETRIC,
    HEARD,
    PENDING
};

/* Address block TYPES. */
//#define LOCAL_IF     0    ///< this ab contains information about the local iface. (neighbor's or mine!)
//#define LINK_STATUS  1    ///< this ab contains information about the link status.
//#define OTHER_NEIGHB 2    ///< this ab contains information about other neigbors, not yet added.
//#define MPR          3    ///< this ab contains information about the mpr status
enum address_block_types {
    LOCAL_IF,
    LINK_STATUS,
    OTHER_NEIGHB,
    MPR
};

/* Address block type LOCAL_IF values. */
//#define THIS_IF     0 ///< the address belongs to the sending interface
//#define OTHER_IF    1 ///< the address belongs to another interface on the sending router
enum local_if_values {
    THIS_IF,
    OTHER_IF
};
/* Address block type MPR values. */
//#define UNDEFINED   0
//#define FLOODING    1
//#define ROUTING     2
//#define FLOOD_ROUTE 3
enum mpr_ab_values {
    UNDEFINED,
    FLOODING,
    ROUTING,
    FLOOD_ROUTE
};

/* Timeval types needed for _check_expired() */
//#define L_SYM_TIME      0
//#define L_HEARD_TIME    1
enum timeval_types {
    L_SYM_TIME,
    L_HEARD_TIME
};

/* Initial parameters */
#define INITIAL_QUALITY 1
#define INITIAL_PENDING false

/* MPR related constants */
#define WILL_NEVER      0     ///< A value of 0 means this router does not want to be selected as MPR.
#define WILL_DEFAULT    7     ///< A value of 7 describes the default willingness level.
#define WILL_ALWAYS     15    ///< A value of 15 means this always wants to be selected as MPR.
#define WILLINGNESS     WILL_DEFAULT

#define PDR_WINDOW_SIZE 10      ///< PDR_WINDOW_SIZE * HELLO_INTERVAL = PDR_WINDOW_INTERVAL

extern struct timeval N_HOLD_TIME;
extern struct timeval L_HOLD_TIME;
extern struct timeval H_HOLD_TIME;
extern struct timeval NHDP_HELLO_INTERVAL;
extern struct timeval NHDP_CLEANUP_INTERVAL;
extern double mpr_minpdr;

/* ========= Header and packet definitions used for Hello packet creation. ============*/
/*
 * The header for hello messages send by this NHDP implementation.
 */
struct nhdp_hello_hdr {
    uint16_t         validity_time;      ///< time this hello is valid in milliseconds.
    uint8_t         num_address_blocks; ///< number of address blocks this hello contains
    uint8_t         willingness;        ///< the senders willingness value
} __attribute__((packed));

/* An address block TLV contains an address associated with this block, the type
 * of the block (LOCAL_IF or ?) and a description of that address (THIS_IF or
 * OTHER_IF), whether this address is from the sending interface or another one.
 */
struct nhdp_address_block {
    uint8_t address[ETH_ALEN];  ///< The address described by this block.
    uint8_t flags;               ///< Type of this address block.
} __attribute__((packed));

/* Possible dessert extensions used in this implementation. */
//enum nhdp_ext_types {
//    NHDP_HELLO_EXT = DESSERT_EXT_USER,  ///< The nhdp hello packet.
//};

/* ================= NHDP Information Base ========================== */
/*
 * The Information base is stored for each local interface. It consists of the
 * link set and the two hop neighbor set (N2_set).
 */

/* Link Set.
 * Records links to other routers that are/were 1-Hop Neighbors.
 * Each local interface owns such a link set.
 */

typedef struct Linkset_tuple {
    uint8_t                         L_neighbor_iface_addr[ETH_ALEN];    ///< Address of this 1-hop neighbor's MANET interface.
    struct timeval                  L_HEARD_time;                       ///< time this link stays in status L_HEARD not considering link quality
    struct timeval                  L_SYM_time;                         ///< time this link stays in status L_SYM not considering link quality
    struct timeval                  L_time;                             ///< time this tuple stays valid
    double                          L_quality;                          ///< this links quality
    uint8_t                         L_pending;                          ///< reflects if the link is pending
    uint8_t                         L_lost;                             ///< reflects if the links is lost
    /* MPR related variables */
    uint8_t                         L_mpr_selector;
    UT_hash_handle                  hh;
} L_set_tuple_t;

/* Each linkset tuple represents a link to a one hop neighbors interface.*/
typedef struct Linkset {
    uint8_t                         L_local_iface_addr[ETH_ALEN];   ///< address of the local interface [key]
    L_set_tuple_t                   *l_set_tuple_list;              ///< the linkset tuple
    UT_hash_handle                  hh;
} L_set_t;

/*
 * 2-Hop Set.
 * Records network addresses of symmetric 2-hop neighbors and the symmetric
 * links to symmetric 1-hop neighbors through which these can be reached.
 */

typedef struct N2_set_tuple {
    uint8_t         N2_neighbor_iface_addr[ETH_ALEN];   ///< address of symmetric 1-hop neighbor's MANET interface  <-------------------- [KEY1]
    uint8_t         N2_2hop_addr[ETH_ALEN];             ///< address of symmetric 2-hop neighbor with link to this 1-hop neighbor <------ [KEY2]
    struct timeval  N2_time;                            ///< specifies when this tuple expires
    UT_hash_handle  hh;
} N2_set_tuple_t;

typedef struct N2_set {
    uint8_t         N2_local_iface_addr[ETH_ALEN];                ///< the local interfaces address [key]
    N2_set_tuple_t  *N2_set_tuple_list;                           ///< a list containing tuples
    UT_hash_handle  hh;
} N2_set_t;

/* This is the uthash key used with the N2 set. */
typedef struct N2_lookup_key {
    uint8_t N2_neighbor_iface_addr[ETH_ALEN];
    uint8_t N2_2hop_addr[ETH_ALEN];
} N2_set_lookup_key_t;

/* ================= NHDP Neighbor Information Base ==========================
 *
 * The Neighbor information base consists of the 1-hop neighbor set (N_set) and
 * the lost neighbor set.
 */

/* Neighbor Set.
 * Records all 1-hop neighbors (their addresses). This information is interface
 * independent (valid per router).
 */
typedef struct N_set_addrlist {
    uint8_t         N_neighbor_addr[ETH_ALEN];  ///< address belonging to this router
    UT_hash_handle  hh;
} N_set_addrlist_t;

typedef struct N_set {
    N_set_addrlist_t        *N_neighbor_addr_list;          ///< [KEY] list that contains all this neighbors interface addresses
    uint8_t                 N_symmetric;                    ///< boolean flag, describing if this is a symmetric 1-hop neighbor.
    uint8_t                 N_mpr;                          ///< boolean flag, describing if this neighbor is selected as mpr
    uint8_t                 N_willingness;                  ///< this neighbors willingness to be an mpr
    uint8_t                 N_mpr_selector;
    UT_hash_handle          hh1, hh2;
} N_set_t;
 
/*
 * Lost Neighbor Set.
 * Records neighbors that were symmetric 1-hop neighbors, but are reported as lost.
*/

typedef struct lost_neighbor_set {
    uint8_t         NL_neighbor_addr[ETH_ALEN];     ///< lost neighbors address
    struct timeval  NL_time;                        ///< specifies whent this tuple expires
    UT_hash_handle hh;
} NL_set_t;

/* Local address block buffer. Buffers addressblocks from the incoming hello message. */
typedef struct AB_buffer {
    uint8_t                 AB_addr[ETH_ALEN];
    uint8_t                 AB_flags;
    UT_hash_handle hh;
} AB_buffer_t;

typedef struct n2_set_addrlist {
    uint8_t     *key_ptr;
    UT_hash_handle hh;
} N2_set_addrlist_t;


/* =========== MPR RELATED TYPES ======================= */
typedef struct MPR_NI_set {
    N_set_t         *n_set_ptr;     ///< pointer to the neighborset entry
    uint8_t         r_value;        ///< this neighborset entries' R(Y,I) value (Y = router, I = interface)
    uint8_t         d_value;        ///< this neighborset entries' D(Y,I) value (Y = router, I = interface)
    UT_hash_handle  hh;
} MPR_NI_set_t;

typedef struct MPR_CONNECTED_set {
    uint8_t         n2_addr[ETH_ALEN];
    uint8_t         counter;
    MPR_NI_set_t    *mpr_ni_set_ptr;
    UT_hash_handle  hh;
} MPR_CONNECTED_set_t;


/* ==== PDR calculation related structures ==== */


/* This structure holds the timestamps of received packets. */
typedef struct PDR_neighbor_trap {
    struct timeval     timestamp;
    UT_hash_handle     hh;
} PDR_neighbor_trap_t;
/* This structure holds the single packet trap 
   for each neighbor */
typedef struct PDR_global_trap {
    uint8_t             n_addr[ETH_ALEN];
    PDR_neighbor_trap_t *pdr_packet_trap;
    UT_hash_handle      hh;
} PDR_global_trap_t;



/* =========== PROTOTYPES ===============*/

void            init_nhdp();
void            _print_nset();
uint8_t         _check_expired(struct timeval *timeval);
uint8_t         _lset_get_linkstatus(L_set_tuple_t* linkset_entry);
uint8_t         _create_local_iface_addrlist(dessert_meshif_t* this_if, AB_buffer_t **ab_buffer);
uint8_t         _create_linkset_addrlist(dessert_meshif_t* local_iface, AB_buffer_t **ab_buffer);
uint8_t         _create_neighbor_addrlist(AB_buffer_t **ab_buffer);
void            _do_neighborset_mpr_updates(AB_buffer_t *ab_buffer, N_set_t **current_n_set_tuple, uint8_t *willingness, bool *reselect_mprs);
void            _do_neighborset_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *neighbor_address_list, N_set_addrlist_t **removed_address_list, N_set_addrlist_t **lost_address_list, uint8_t *willingness, bool *reselect_mprs);
void            _do_lost_neighborset_updates(N_set_addrlist_t *lost_address_list);
void            _do_linkset_mpr_updates(AB_buffer_t *ab_buffer, L_set_tuple_t **current_l_set_tuple);
void            _do_linkset_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *removed_address_list, uint8_t *sending_address, dessert_meshif_t *receiving_iface, uint16_t *validity_time, bool *reselect_mprs, float *pdr);
void            _do_n2set_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *removed_address_list, N_set_addrlist_t *neighbor_address_list, uint8_t *sending_address, dessert_meshif_t* receiving_iface, uint16_t *validity_time, bool *reselect_mprs);
void            _link_heard_timeout(L_set_tuple_t *l_set_tuple, L_set_t **l_set_entry);
void            _link_changed_to_symmetric(L_set_tuple_t *l_set_tuple);
void            _link_changed_to_not_symmetric(L_set_tuple_t *l_set_tuple, L_set_t *l_set_entry);
void            _do_cleanup(AB_buffer_t **ab_buffer, N_set_addrlist_t **neighbor_address_list, N_set_addrlist_t **removed_address_list, N_set_addrlist_t **lost_address_list);
void            _pdr_packet_trap_add(float *pdr, uint8_t *address);
uint8_t         _num_neighbors();
uint8_t         _num_symmetric_neighbors();
uint8_t         _num_strict_2hop_neighbors();
uint8_t         _num_mprs();
uint8_t         _num_mpr_selectors();
uint8_t         _calculate_D(N_set_t *y, dessert_meshif_t *i);
uint8_t         _calculate_R(N_set_t *y, dessert_meshif_t *i);
void            _create_mpr_ni_set(MPR_NI_set_t **mpr_ni_set, dessert_meshif_t *local_iface);
void            _free_mpr_ni_set(MPR_NI_set_t **mpr_ni_set);
void            _filter_mpr_candidates(MPR_NI_set_t **mpr_ni_set);
bool            _isNeighbor(N2_set_tuple_t *n2_tuple);
void            _set_pdr_started();
void            _reset_pdr_packet_traps();
/* start/stop tasks */
void            _start_pdr_delay_task();
void            _stop_pdr_delay_task();
void            _start_nhdp_hello_task();
void            _start_nhdp_cleanup_task();
void            _start_mpr_selectors_logging_task();
void            _start_n2_logging_task();
void            _start_pdr_delay_task();
void            _start_mpr_selection_task();
void            _stop_mpr_selection_task();
dessert_per_result_t _reselect_mprs(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t _log_mpr_selector_string(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t _log_n2_set_string(void* data, struct timeval* scheduled, struct timeval* interval);
//void                _check_removed_and_lost_addresses(N_set_t *n_set_matches, N_set_addrlist_t *neighbor_address_list, N_set_addrlist_t *removed_address_list, N_set_addrlist_t *lost_address_list);

/* timeval helpers */
void            _timeval_add_ms(struct timeval *tv, uint16_t *milliseconds);
int             _timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
uint16_t        _timeval_get_ms(struct timeval *tv);

/* scheduled functions */
dessert_per_result_t nhdp_send_hello(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t nhdp_cleanup_expired(void* data, struct timeval* scheduled, struct timeval* interval);

void _check_constraints();


/* cli functions */
int cli_show_nhdp_linkset(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_show_nhdp_neighborset(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_show_nhdp_n2set(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_show_nhdp_strict_n2set(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_show_mprselectors(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_set_nhdp_ht(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_set_nhdp_hi(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_set_mpr_ri(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_flush_hashes(struct cli_def *cli, char *command, char *argv[], int argc);
int cli_set_minpdr(struct cli_def *cli, char *command, char *argv[], int argc);

#endif
