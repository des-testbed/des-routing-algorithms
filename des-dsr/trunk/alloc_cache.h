#ifndef ALLOC_CACHE_H_
#define ALLOC_CACHE_H_


#include "dsr.h"

#ifdef CACHE_STATISTICS
extern int cache_allocs;
extern int cache_underrun;
extern int cache_deallocs;
extern int cache_overflow;
extern int cache_max;
extern int cache_mean;

int cache_statistics(void* data, struct timeval* scheduled,
                     struct timeval* interval);
#endif

int dsr_dessert_msg_clone(dessert_msg_t** msgnew, const dessert_msg_t* msgold);
int dsr_dessert_msg_new(dessert_msg_t** msgout);
void dsr_dessert_msg_destroy(dessert_msg_t* msg);


#define dessert_msg_clone(a,b,s) dsr_dessert_msg_clone(a,b)
#define dessert_msg_new(m) dsr_dessert_msg_new(m)
#define dessert_msg_destroy(m) dsr_dessert_msg_destroy(m)



#endif /* ALLOC_CACHE_H_ */
