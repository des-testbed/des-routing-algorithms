#ifndef LSR_NT
#define LSR_NT

#include "lsr_database.h"

#include <uthash.h>
#include <dessert.h>

struct node;

/** level 2 neighbor with reference to l25 node */
typedef struct neighbor {
	struct timeval timeout;
	struct node *node;
	uint32_t weight;
	dessert_meshif_t *iface;
	mac_addr addr;
	UT_hash_handle hh;
	//struct neighbor *prev, *next; //circular list for all l2 interfaces belonging to one l25 neighbor
} neighbor_t;

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr, uint8_t weight);
dessert_result_t lsr_nt_age_all(void);

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t **result, int *neighbor_count);

/**
 * pre-condition: all nodes that are referenced by neighbor interfaces have infinite weight
 */
dessert_result_t lsr_nt_set_neighbor_weights(void);

#endif
