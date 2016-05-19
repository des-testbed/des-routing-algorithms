#ifndef GOSSIPING_CLI
#define GOSSIPING_CLI

#include <math.h>
#include <pthread.h>
#include <netinet/in.h>
#include <dessert.h>
#include <uthash.h>

#ifdef ANDROID
#endif

extern double p;
extern double p2;               ///< use p2 instead of p if neighbors<n
extern uint8_t k;               ///< flood on the first k hops
extern uint8_t n;               ///< use p2 instead of p if neighbors<n
extern uint8_t m;               ///< gossip if receiving<m messages in time t [ms]
extern struct timeval timeout;  ///< timeout value for trapped packets
extern uint8_t gossip;          ///< gossip mode
extern uint8_t helloTTL;        ///< time-to-live for HELLOs in gossip4
extern uint8_t logrx;
extern uint8_t logtx;
extern uint8_t logfw;
extern struct timeval hello_interval;
extern struct timeval cleanup_interval;
extern uint16_t T_MAX_ms;       ///< max. value for random timeout [ms] in gossip7
extern double p_min;
extern double p_max;
extern uint16_t hello_size;
extern uint8_t forwarder;

#define HELLO_SIZE  128

/* flags */
#define USE_SEQ     0x02
#define SYN_SEQ     0x04
#define USE_K       0x08
#define HELLO       0x10
#define DELAYED     0x20
#define NHDP_HELLO  0x40

/* flags */
#define USE_KHOPS   0x01
#define USE_HELLOS  0x02
#define USE_NHDP    0x04
#define USE_GOSSIP13 0x08

#define LOG_SIZE 200

extern uint8_t activated;
extern bool gossip13_drop_seq2_duplicates;
extern bool gossip13_piggyback;

enum packettypes {
    EXT_HOPS = DESSERT_EXT_USER,
    NODE_LIST,
    GOSSIP13_UPDATE,                      ///< for Blywis-Reinecke mode
    NHDP_HELLO_EXT                        ///< for NHDP/MPR (gossip11)
};

typedef struct gossip_ext_update {
    uint16_t tau;                   ///< restart interval
    uint8_t  restarts;              ///< number of restarts
    uint8_t  nodes;                 ///< number of known nodes
    uint16_t update_time_ms;        ///< current update interval resp. when to expect the next update
    uint16_t observation_time_ms;   ///< duration the update is valid for
    uint32_t rx_packets;            ///< number of received packets
    uint32_t tx_packets;            ///< number of transmitted packets (without duplicates)
    double   mean_dist;             ///< mean distance from this node to all others
    double   eccentricity;          ///< max. distance from this node to all others
} gossip_ext_update_t;

typedef struct gossip_ext_hops {
    uint8_t     hops;               ///< traveled distance
    uint16_t    seq2;               ///< inner sequence number (outer sequence number is in desssert header)
    uint8_t     padding[1];
} gossip_ext_hops_t;

enum gossip_types {
    gossip_0 = 0,   ///< gossip with probability p aka simple gossiping
    gossip_1,       ///< gossip k hops with p=1 by Haas et al.
    gossip_2,       ///< gossip with p2 if fewer than n neighbors by Haas et al.
    gossip_3,       ///< gossip if receiving less than m messages until timeout by Haas et al.
    gossip_4,       ///< use unicast routing to nodes in zone of z hops by Haas et al.
    gossip_5,       ///< combined gossip_2 and gossip_3
    gossip_6,       ///< gossip3 version with adaptive calculation of p after timeout by Shi and Shen
    gossip_7,       ///< gossip3 with random timeout value and fixed p
    gossip_8,       ///< gossip2 with adaptive calculation for p by Hanashi et al.
    gossip_9,       ///< adaptive p with node list (considers coverage) by Abdulai et al.
    gossip_10,      ///< overhear if sent packets are forwarded by neighbors TODO implement
    gossip_11,      ///< gossip with nhdp and mpr logic TODO finish implementation
    gossip_12,      ///< MCDS mode (manually select forwarding modes)
    gossip_13,      ///< Blywis-Reinecke mode
    gossip_14,      ///< like gossip3 but no p
    gossip_unused
};

typedef struct _observation {
    struct timeval          start;
    struct timeval          end;
    uint32_t                packets;
    struct _observation*    next;
} observation_t;

typedef struct _history {
    observation_t*      observations;
    char*               name;
    pthread_rwlock_t    lock;
} history_t;

typedef struct _observation_mean {
    struct timeval          start;
    struct timeval          end;
    double                  mean;
    uint32_t                count;
    struct _observation_mean*   next;
} observation_mean_t;

typedef struct _history_mean {
    observation_mean_t*     observations;
    char*                   name;
    pthread_rwlock_t        lock;
} history_mean_t;

typedef struct _seqlog {
    u_char addr[ETHER_ADDR_LEN];///< key for the hash map
    uint16_t seqs[LOG_SIZE];    ///< list of the last LOG_SIZE rx packets (outer number)
    uint16_t seqs2[LOG_SIZE];   ///< list of the last LOG_SIZE rx packets (inner number)
    uint16_t next_seq;          ///< next pos in seqs
    uint16_t next_seq2;         ///< next pos in seqs2

    // what I measured about the node
    history_t*      observed_history;///< number of packets received from this source without duplicates
    history_mean_t* my_hops_history;///< number of hops the packets traveled from the node to me

    // what the other node measured
    history_t*      tx_history;      ///< number of transmitted packets by this source without duplicates
    history_t*      total_tx_history;///< number of transmitted packets by this source including duplicates
    history_t*      rx_history;      ///< number of packets received by the source without duplicates
    history_mean_t* hops_history;    ///< path length of the packets received by the source
    history_t*      nodes_history;   ///< number of known nodes

    // latest information (no history)
    history_mean_t* mean_dist_history;  ///< current mean distance of the node to all others; \todo replace by history_mean_t*
    history_mean_t* eccentricity_history;///< current eccentricity of the node; \todo replace by history_mean_t*
    uint16_t        tau;               ///< current restart interval; \todo asses if we actually need this
    uint16_t        update_time_ms;    ///< current update interval of the node; can also tell us if we missed updates

    UT_hash_handle hh;
} seqlog_t;

extern seqlog_t* seqlog;
extern pthread_rwlock_t seqlog_lock;

/* gossiping_cli.c */
void init_cli();
int cli_start(struct cli_def *cli, char *command, char *argv[], int argc);

/* gossiping_core */
dessert_cb_result sendToNetwork(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *tunif, dessert_frameid_t id);
dessert_cb_result deliver(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result floodOnFirstKHops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
void addSeq(dessert_msg_t* msg);
dessert_cb_result drop_zero_mac(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);

/* gossiping_neighbors */
void start_neighborMon();
void stop_neighborMon();
void update_neighborMon();
uint16_t numNeighbors();
bool isInZone(dessert_msg_t* msg);
void attachNodeList(dessert_msg_t* msg);
uint8_t coveredNeighbors(dessert_msg_t* msg, dessert_meshif_t *iface);

dessert_per_result_t send_hello(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_cb_result handleHello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_per_result_t cleanup_neighbors(void *data, struct timeval *scheduled, struct timeval *interval);

int cli_showneighbors(struct cli_def* cli, char* command, char* argv[], int argc);

/* gossiping_packettrap */
uint8_t packetStored(dessert_msg_t* msg);
void storeDroppedPacket(dessert_msg_t* msg);
void storeForwardedPacket(dessert_msg_t* msg, uint8_t source);
void resetPacketTrap();
void resetSeqLog();
dessert_cb_result checkSeq(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_per_result_t handleTrappedPacket(void *data, struct timeval *scheduled, struct timeval *interval);
uint32_t gossip13_rxPackets();

/* gossiping_log */
void initLogger();
dessert_cb_result logRX(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
void logTX(dessert_msg_t* msg, dessert_meshif_t *iface);
int logForwarded(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface);
int logHelloFW(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface);
int logHello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface);
int logadaptivep(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, double p);
void    logNhdpHelloTx(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, uint8_t num_neighbors, uint8_t num_2hop_neighbors, uint8_t num_mprs, uint8_t num_mpr_selectors);
void    logNhdpHelloRx(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface);

/* gossiping_hello */
void reset_hello_counter();
void count_hello();
int cli_showcounterhello(struct cli_def *cli, char *command, char *argv[], int argc);

/* gossiping_nhdp*/
void    nhdp_start();
void    nhdp_stop();
dessert_cb_result nhdp_handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);
bool    is_mpr(dessert_msg_t *msg);

/* gossip13 */
void gossip13_start();
void gossip13_stop();
void gossip13_schedule_retransmission(dessert_msg_t *msg, bool keep);
void gossip13_add_ext(dessert_msg_t* msg);
void gossip13_activate_emission();
void gossip13_deactivate_emission();
void gossip13_delayed_schedule(struct timeval* scheduled, struct timeval* interval);
inline void gossip13_seq2(gossip_ext_hops_t* ext);
inline bool gossip13_rx_packet(seqlog_t* s, dessert_msg_t*, bool init);
inline void gossip13_tx_packet();
dessert_cb_result gossip13_eval_update(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
int gossip13_stats(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_mystats(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_tau(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_restarts(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_update(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_observation(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_emission(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_tau(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_restarts(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_update(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_observation(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_emission(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_seq2(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_seq2(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_tx_history(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_rx_history(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_hops_history(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_deadline(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_deadline(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_data(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_show_send_updates(struct cli_def *cli, char *command, char *argv[], int argc);
int gossip13_set_send_updates(struct cli_def *cli, char *command, char *argv[], int argc);

dessert_per_result_t gossip13_restart(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t gossip13_generate_data(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t gossip13_update_data(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t gossip13_store_observations(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t gossip13_cleanup_observations(void *data, struct timeval *scheduled, struct timeval *interval);
void gossip13_send_update();
inline void gossip13_new_observation(observation_t** observation);
inline void gossip13_new_observation_mean(observation_mean_t** observation);
inline void gossip13_new_history(history_t** hist, bool local_observation);
inline void gossip13_new_history_mean(history_mean_t** hist, bool local_observation);

#endif
