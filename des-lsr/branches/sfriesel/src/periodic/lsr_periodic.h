#include <dessert.h>

extern dessert_periodic_t *periodic_send_hello;
extern dessert_periodic_t *periodic_send_tc;
extern dessert_periodic_t *periodic_refresh_nh;
extern dessert_periodic_t *periodic_refresh_rt;
extern dessert_periodic_t *periodic_regenerate_rt;

//dessert_per_result_t lsr_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t lsr_periodic_send_tc(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t lsr_periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t lsr_periodic_regenerate_rt(void *data, struct timeval *scheduled, struct timeval *interval);
//dessert_per_result_t refresh_list();
//dessert_per_result_t refresh_rt();
