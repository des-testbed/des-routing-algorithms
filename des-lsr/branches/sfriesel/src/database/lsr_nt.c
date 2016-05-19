#include "../lsr_config.h"
#include "lsr_nt.h"
#include "lsr_tc.h"

#include <utlist.h>

// neighbor table; hash map of all neighboring l2 interfaces
static neighbor_t *nt = NULL;

int16_t _timeout2lifetime(struct timeval *timeout, struct timeval *now) {
	if(dessert_timevalcmp(timeout, now) < 0)
		return -1;
	uint64_t diff = dessert_timeval2ms(timeout) - dessert_timeval2ms(now);
	return diff / tc_interval;
}

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t ** const result, int * const neighbor_count) {
	// circular list of edges pointing to l25 neighbor nodes
	int out_size = 16;
	neighbor_info_t *out = calloc(out_size, sizeof(neighbor_info_t));
	int out_used = 0;
	
	
	*neighbor_count = 0;
	struct timeval now;
	gettimeofday(&now, NULL);
	
	neighbor_t *neighbor, *tmp;
	HASH_ITER(hh, nt, neighbor, tmp) {
		int16_t lifetime = _timeout2lifetime(&neighbor->timeout, &now);
		if(lifetime < 0) {
			continue;
		}
		int j;
		for(j = 0; j < out_used; ++j) {
			if(mac_equal(neighbor->node->addr, out[j].addr) && neighbor->weight < out[j].weight) {
				break;
			}
		}
		if(j >= out_used) {
			out_used++;
			if(out_used >= out_size) {
				out = realloc(out, out_size *= 2);
			}
		}
		mac_copy(out[j].addr, neighbor->node->addr);
		out[j].lifetime = lifetime;
		out[j].weight = neighbor->weight;
	}
	*neighbor_count = out_used;
	*result = out;
	return DESSERT_OK;
}

struct timeval lsr_nt_calc_timeout(void) {
	uint32_t lifetime_ms = neighbor_lifetime * hello_interval;
	struct timeval timeout;
	gettimeofday(&timeout, NULL);
	dessert_timevaladd(&timeout, lifetime_ms / 1000, (lifetime_ms % 1000) * 1000);
	return timeout;
}

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr, uint8_t weight) {
	node_t *node = lsr_tc_get_or_create_node(neighbor_l25);
	
	neighbor_t *neighbor = NULL;
	HASH_FIND(hh, nt, neighbor_l2, ETH_ALEN, neighbor);
	if(!neighbor) {
		neighbor = malloc(sizeof(neighbor_t));
		memcpy(neighbor->addr, neighbor_l2, ETH_ALEN);
		neighbor->node = node;
		neighbor->iface = iface;
		HASH_ADD_KEYPTR(hh, nt, neighbor->addr, ETH_ALEN, neighbor);
	}
	neighbor->timeout = lsr_nt_calc_timeout();
	neighbor->weight = weight;
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age(neighbor_t *neighbor, const struct timeval *now) {
	if(dessert_timevalcmp(now, &neighbor->timeout) < 0) {
		HASH_DEL(nt, neighbor);
		free(neighbor);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age_all(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		lsr_nt_age(neighbor, &now);
	}
	return DESSERT_OK;
}

/**
 * pre-condition: all nodes that are referenced by neighbor interfaces have infinite weight
 */
dessert_result_t lsr_nt_set_neighbor_weights(void) {
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		if(neighbor->weight < neighbor->node->weight) {
			neighbor->node->next_hop = neighbor;
			neighbor->node->weight = neighbor->weight;
		}
	}
	return DESSERT_OK;
}

