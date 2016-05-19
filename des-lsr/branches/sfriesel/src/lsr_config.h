#ifndef LSR_CONFIG
#define LSR_CONFIG

#include <stdint.h>

#define LSR_EXT_TC DESSERT_EXT_USER

// milliseconds
#define TC_INTERVAL              4000
#define HELLO_INTERVAL           4000
#define NEIGHBOR_AGING_INTERVAL TC_INTERVAL
#define NODE_AGING_INTERVAL      HELLO_INTERVAL
#define RT_REBUILD_INTERVAL      500

//decrements once per aging interval
#define NEIGHBOR_LIFETIME   5
#define NODE_LIFETIME       32
#define DEFAULT_WEIGHT      1

#define LSR_TTL_MAX             UINT8_MAX

extern uint16_t tc_interval;
extern uint16_t hello_interval;
extern uint16_t neighbor_aging_interval;
extern uint16_t node_aging_interval;
extern uint8_t  neighbor_lifetime;
extern uint8_t  node_lifetime;
extern uint16_t rt_rebuild_interval;

#endif
