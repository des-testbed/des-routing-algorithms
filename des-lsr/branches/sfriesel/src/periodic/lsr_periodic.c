#include "lsr_periodic.h"
#include "../lsr_config.h"
#include "../database/lsr_database.h"
#include "../pipeline/lsr_pipeline.h"

#include <uthash.h>

dessert_periodic_t *periodic_send_hello;
dessert_periodic_t *periodic_send_tc;
dessert_periodic_t *periodic_refresh_nh;
dessert_periodic_t *periodic_refresh_rt;
dessert_periodic_t *periodic_regenerate_rt;

typedef struct tc_ext {
	mac_addr addr;
	uint8_t lifetime;
	uint8_t weight;
} __attribute__((__packed__)) tc_ext_t;

dessert_per_result_t _lsr_periodic_send_tc_with_ttl(uint8_t ttl) {
	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = ttl;
	tc->u8  = 0;
	tc->u16 = htons((uint16_t) lsr_db_broadcast_get_seq_nr());
	
	dessert_ext_t *ext;
	
	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header *) ext->data;
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	
	neighbor_info_t *neighbor_list = NULL;
	int neighbor_count = 0;
	lsr_db_dump_neighbor_table(&neighbor_list, &neighbor_count);
	uint32_t ext_size = sizeof(tc_ext_t) * neighbor_count;
	if(dessert_msg_addext(tc, &ext, LSR_EXT_TC, ext_size) != DESSERT_OK)
		dessert_notice("TC extension too big! This is an implementation bug");

	// copy NH list into extension
	tc_ext_t *tc_element = (tc_ext_t*) ext->data;
	for(int i = 0; i < neighbor_count; ++i) {
		memcpy(tc_element[i].addr, neighbor_list[i].addr, ETH_ALEN);
		tc_element[i].lifetime = neighbor_list[i].lifetime;
		tc_element[i].weight = neighbor_list[i].weight;
	}
	free(neighbor_list);

	lsr_send_randomized(tc);
	dessert_msg_destroy(tc);
	return DESSERT_PER_KEEP;
}

dessert_per_result_t lsr_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	return _lsr_periodic_send_tc_with_ttl(1);
}

dessert_per_result_t lsr_periodic_send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	return _lsr_periodic_send_tc_with_ttl(LSR_TTL_MAX);
}

dessert_per_result_t lsr_periodic_regenerate_rt(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_rt_regenerate();
	return DESSERT_PER_KEEP;
}

dessert_per_result_t age_neighbors(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_nt_age_all();
	return DESSERT_PER_KEEP;
}

dessert_per_result_t age_nodes(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_tc_age_all();
	return DESSERT_PER_KEEP;
}
