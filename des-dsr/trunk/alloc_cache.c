#include "dsr.h"

//#define CHUNK_SIZE DESSERT_MAXFRAMELEN
// \todo remove magic number
#define CHUNK_SIZE 2500
#define CACHE_SIZE 500

pthread_mutex_t _alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef CACHE_STATISTICS
int cache_allocs;
int cache_underrun;
int cache_deallocs;
int cache_overflow;
int cache_max;
int cache_mean;
#endif

int ll_len = 0;
int cache_size = 0;

typedef struct el {

    struct el* next;

} el_t;

el_t* ll_head = NULL;

#ifdef CACHE_STATISTICS
int cache_statistics(void* data, struct timeval* scheduled,
                     struct timeval* interval) {

    dessert_info("   allocs: %.10i  deallocs: %.10i  cache_max: %i  ", cache_allocs, cache_deallocs, cache_max);
    dessert_info(" underrun: %.10i  overflow: %.10i     ll_len: %i  ", cache_underrun, cache_overflow, ll_len);
    dessert_info(" cache_size: %i", cache_size);

    return (0);
}
#endif

static inline dessert_msg_t* alloc() {
    dessert_msg_t* ptr;

    pthread_mutex_lock(&_alloc_mutex); 	/* LOCK */

#ifdef CACHE_STATISTICS
    cache_allocs++;
#endif

    if(unlikely(ll_len == 0)) {
        ptr = malloc(CHUNK_SIZE);
#ifdef CACHE_STATISTICS
        cache_underrun++;
        cache_size++;
#endif
        assert(ptr != NULL);
    }
    else {
        ptr = (dessert_msg_t*)ll_head;
        ll_head = ll_head->next;
        ll_len--;
    }

    pthread_mutex_unlock(&_alloc_mutex); /* UNLOCK */

    return ptr;
}

static inline void dealloc(dessert_msg_t* ptr) {

    pthread_mutex_lock(&_alloc_mutex); 	/* LOCK */

#ifdef CACHE_STATISTICS
    cache_deallocs++;
#endif

    if(unlikely(ll_len >= CACHE_SIZE)) {
        free(ptr);
#ifdef CACHE_STATISTICS
        cache_overflow++;
        cache_size--;
#endif
    }
    else {
        ((el_t*) ptr)->next = ll_head;
        ll_head = (el_t*) ptr;
        ll_len++;
#ifdef CACHE_STATISTICS

        if(ll_len > cache_max) {
            cache_max = ll_len;
        }

#endif
    }

    pthread_mutex_unlock(&_alloc_mutex); /* UNLOCK */
}

/** generates a copy of a dessert_msg
 * @arg **msgnew (out) pointer to return message address
 * @arg *msgold pointer to the message to clone
 * @arg sparse whether to allocate DESSERT_MAXFRAMELEN or only hlen+plen
 * @return DESSERT_OK on success, -errno otherwise
 **/
int dsr_dessert_msg_clone(dessert_msg_t** msgnew, const dessert_msg_t* msgold) {

    dessert_msg_t* msg;
    size_t msglen = ntohs(msgold->hlen) + ntohs(msgold->plen);

    msg = alloc(CHUNK_SIZE);

    if(msg == NULL) {
        return (DESSERT_ERR);
    }

    memcpy(msg, msgold, msglen);

    msg->flags &= DESSERT_RX_FLAG_SPARSE ^ DESSERT_RX_FLAG_SPARSE;

    *msgnew = msg;
    return (DESSERT_OK);
}

/** creates a new dessert_msg_t and initializes it.
 * @arg **msgout (out) pointer to return message address
 * @return 0 on success, -errno on error
 **/
int dsr_dessert_msg_new(dessert_msg_t** msgout) {
    dessert_msg_t* msg;

    msg = alloc(CHUNK_SIZE);

    if(msg == NULL) {
        dessert_err("failed to allocate buffer for new message!");
        return (DESSERT_ERR);
    }

    memset(msg, 0, CHUNK_SIZE);
    msg->l2h.ether_type = htons(DESSERT_ETHPROTO);
    memset(msg->l2h.ether_dhost, 255, ETHER_ADDR_LEN);
    memcpy(msg->proto, dessert_proto, DESSERT_PROTO_STRLEN);
    msg->ver = dessert_ver;
    msg->ttl = 0xff;
    msg->u8 = 0x00;
    msg->u16 = htons(0xbeef);
    msg->hlen = htons(sizeof(dessert_msg_t));
    msg->plen = htons(0);

    *msgout = msg;
    return (DESSERT_OK);

}

/** free a dessert_msg
 * @arg *msg message to free
 **/
void dsr_dessert_msg_destroy(dessert_msg_t* msg) {
    dealloc(msg);
}


