#ifndef LSR_DATABASE
#define LSR_DATABASE
#include <dessert.h>

#define INFINITE_WEIGHT UINT32_MAX

// exchange format to export neighbor table into TC messages
typedef struct neighbor_info {
	uint8_t addr[ETH_ALEN];
	uint8_t lifetime;
	uint8_t weight;
} neighbor_info_t;

struct seq_interval {
	uint16_t start; //inclusive
	uint16_t end; //exclusive
};

dessert_result_t lsr_db_dump_neighbor_table(neighbor_info_t **result, int *neighbor_count);

/**
 * Update that the neighbor given by the arguments has been seen
 * returns: DESSERT_OK on successful update, DESSERT_ERR otherwise
 */
dessert_result_t lsr_db_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr);

/**
 * Update that the node given by the arguments has been seen
 * returns: DESSERT_OK on successful update, DESSERT_ERR otherwise
 */
dessert_result_t lsr_db_tc_update(mac_addr node_addr, uint16_t seq_nr);

/**
 * Update that the neighbor of node given by the arguments has been seen
 * @arg lifetime: the remaining lifetime of the neighbor connection as supplied in the node's TC message
 * @arg weight: the weight of the neighbor connection as supplied in the node's TC message
 * @returns: DESSERT_OK on successful update, DESSERT_ERR otherwise
 */
dessert_result_t lsr_db_tc_neighbor_update(mac_addr node_addr, mac_addr neighbor_addr, uint8_t lifetime, uint8_t weight);

/**
 * returns: true if the sequence number seq_nr was not yet seen, false otherwise
 */
bool lsr_db_unicast_check_seq_nr(mac_addr node_addr, uint16_t seq_nr);
/**
 * same for broadcast sequence number
 */
bool lsr_db_broadcast_check_seq_nr(mac_addr node_addr, uint16_t seq_nr);

/**
 * returns: a new broadcast sequence number for a packet to use
 */
uint64_t lsr_db_broadcast_get_seq_nr(void);

/**
 * returns: a new unicast sequence number for a packet to use
 */
uint64_t lsr_db_unicast_get_seq_nr(void);

dessert_result_t lsr_db_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface);

/**
 * recalculate all routing information
 */
dessert_result_t lsr_db_rt_regenerate(void);

/**
 * remove all neighbor entries that have not been refreshed for a certain time (defined in lsr_config.h)
 */
dessert_result_t lsr_db_nt_age_all(void);
dessert_result_t lsr_db_tc_age_all(void);

char *lsr_db_topology_to_string(void);

#endif
