#ifndef LSR_NODE
#define LSR_NODE

#include <uthash.h>
#include <dessert.h>

#define GAP_COUNT 8

typedef struct node {
	uint64_t multicast_seq_nr;           //the highest seen multicast sequence number of this node
	uint64_t unicast_seq_nr;             //the highest seen unicast sequence number of this node
	struct timeval timeout;              //time when this node info becomes stale and should be removed
	struct seq_interval *multicast_gaps; //list multicast seq id's of this not seen yet, but close before multicast_seq_nr
	struct seq_interval *unicast_gaps;   //list unicast seq id's of this not seen yet, but close before unicast_seq_nr
	struct edge *neighbors;              //list of edges pointing to neighboring nodes (accordings to the node's TC)
	struct neighbor *next_hop;           //the level 2 hop which should be used for forwarding to this node
	uint32_t weight;                     //total weight of the route to this node
	UT_hash_handle hh;                   //hash to group all nodes in a node set. hash key is addr
	mac_addr addr;                       //l2.5 address of the node
	uint8_t neighbor_size;               //size of the array pointed to by neighbors
	uint8_t neighbor_count;              //number of entries in neighbors
} node_t;

node_t *lsr_node_new(mac_addr addr);
bool lsr_node_age(node_t *this, const struct timeval *now);
void lsr_node_delete(node_t *this);
void lsr_node_set_timeout(node_t *this, struct timeval timeout);
void lsr_node_update_neighbor(node_t *this, node_t *neighbor, struct timeval timeout, uint8_t weight);
bool lsr_node_check_broadcast_seq_nr(node_t *node, uint16_t seq_nr);
bool lsr_node_check_unicast_seq_nr(node_t *node, uint16_t seq_nr);
char *lsr_node_to_string(node_t *this);

#endif
