/******************************************************************************
 Copyright 2009, Bastian Blywis, Sebastian Hofmann, Freie Universitaet Berlin
 (FUB).
 All rights reserved.

 These sources were originally developed by Bastian Blywis
 at Freie Universitaet Berlin (http://www.fu-berlin.de/),
 Computer Systems and Telematics / Distributed, Embedded Systems (DES) group
 (http://cst.mi.fu-berlin.de/, http://www.des-testbed.net/)
 ------------------------------------------------------------------------------
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program. If not, see http://www.gnu.org/licenses/ .
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
        http://www.des-testbed.net/
*******************************************************************************/

#include "gossiping.h"

typedef struct neighbor {
    u_char addr[ETHER_ADDR_LEN]; // key
    time_t lastseen;
    uint8_t distance;
    UT_hash_handle hh;
} neighbor_t;

dessert_periodic_t* task_hello = NULL;
dessert_periodic_t* task_cleanup = NULL;

neighbor_t* neighbors = NULL;
pthread_rwlock_t neighbor_table_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t schedule_mutex = PTHREAD_MUTEX_INITIALIZER;

uint16_t hello_size = HELLO_SIZE;

/** Returns the number of neighbors (node degree)
 */
inline uint16_t numNeighbors() {
    uint16_t c = 0;
    neighbor_t* n;

    /*###*/
    pthread_rwlock_rdlock(&neighbor_table_lock);
    for(n = neighbors; n != NULL; n = n->hh.next) {
        if(n->distance == 1) {
            c++;
        }
    }
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/

    return c;
}

/** Returns if a node is the gossip4 zone
 *
 * @return 1 if true, else 0
 */
bool isInZone(dessert_msg_t* msg) {
    neighbor_t* neigh;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    /*###*/
    pthread_rwlock_rdlock(&neighbor_table_lock);
    HASH_FIND(hh, neighbors, shost, ETHER_ADDR_LEN, neigh);
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/

    if(neigh) {
        return true;
    }
    return false;
}

/** Sends a HELLO packet
 *
 * Sends a HELLO packet with TTL set to helloTTL.
 */
dessert_per_result_t send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
    struct ether_header *eth;
    dessert_ext_t  *ext;
    dessert_msg_t *msg;

    dessert_msg_new(&msg);
    msg->u8 |= HELLO;
    msg->ttl = helloTTL;
    addSeq(msg);

    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_dummy_payload(msg, hello_size);

    logHello(msg, 0, NULL, NULL);
    dessert_meshsend(msg, NULL);
    count_hello();
    dessert_msg_destroy(msg);

    return DESSERT_PER_KEEP;
}

/** Removes neighbors from hash map after some time
 */
dessert_per_result_t cleanup_neighbors(void *data, struct timeval *scheduled, struct timeval *interval) {
    neighbor_t* n;
    time_t now = time(NULL);

    /*###*/
    pthread_rwlock_wrlock(&neighbor_table_lock);
    for(n = neighbors; n != NULL; n = n->hh.next) {
        time_t diff = difftime(now, n->lastseen);
        if(abs(diff) > cleanup_interval.tv_sec) {
            dessert_debug("lost neighbor: " MAC ", dist=%d", EXPLODE_ARRAY6(n->addr), n->distance);
            HASH_DEL(neighbors, n);
            free(n);
        }
    }
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/

    return DESSERT_PER_KEEP;
}

void reset_neighbors() {
    neighbor_t* current_neighbor;
    /*###*/
    pthread_rwlock_wrlock(&neighbor_table_lock);
    while(neighbors) {
        current_neighbor = neighbors;
        HASH_DEL(neighbors, current_neighbor);
        free(current_neighbor);
    }
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/
    neighbors = NULL;
    reset_hello_counter();
    dessert_notice("neighbor hash map flushed");
}

void add_neighbor(u_char* addr, uint8_t d) {
    dessert_debug("adding neighbor: " MAC ", dist=%d", EXPLODE_ARRAY6(addr), d);
    neighbor_t* n = malloc(sizeof(neighbor_t));
    if(!n) {
        dessert_crit("could not allocate memory");
        return;
    }
    memcpy(n->addr, addr, ETHER_ADDR_LEN);
    n->lastseen = time(NULL);
    n->distance = d;

    /*###*/
    pthread_rwlock_wrlock(&neighbor_table_lock);
    HASH_ADD(hh, neighbors, addr, ETHER_ADDR_LEN, n);
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/
}

uint8_t isInRadius(dessert_msg_t* msg) {
    neighbor_t* n;
    u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;

    /*###*/
    pthread_rwlock_rdlock(&neighbor_table_lock);
    HASH_FIND(hh, neighbors, shost, ETHER_ADDR_LEN, n);
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/

    if(n)
        return n->distance;
    return 0;
}

/** Evaluate received HELLO messages
 *
 * For every received HELLO message, the timestamp in the
 * neighbors data structure is updated. If the HELLO message has
 * a TTL > 0 after decrementation, it will be forwarded.
 */
dessert_cb_result handleHello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(msg->u8 & HELLO) {
        if(activated & USE_HELLOS) {
            neighbor_t* n;
            u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;;
            u_char* l2_shost  = msg->l2h.ether_shost;
            extern char hostname[20];

            uint8_t hops = 0;
            dessert_ext_t* ext = NULL;
            if(dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
                gossip_ext_hops_t* str = (gossip_ext_hops_t*) ext->data;
                hops = str->hops;
            }
#ifndef ANDROID
            avg_node_result_t rssi = dessert_rssi_avg(l2_shost, iface);
            dessert_info("[rh] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", ttl=%d, seq=%d, flags=%#x, hops=%d, rssi=%d",
                hostname,
                EXPLODE_ARRAY6(iface->hwaddr),
                iface->if_name,
                EXPLODE_ARRAY6(l2_shost),
                EXPLODE_ARRAY6(shost),
                msg->ttl, msg->u16, msg->u8, hops, rssi.avg_rssi);
#else
            dessert_info("[rh] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", ttl=%d, seq=%d, flags=%#x, hops=%d",
                hostname,
                EXPLODE_ARRAY6(iface->hwaddr),
                iface->if_name,
                EXPLODE_ARRAY6(l2_shost),
                EXPLODE_ARRAY6(shost),
                msg->ttl, msg->u16, msg->u8, hops);
#endif
            /*###*/
            pthread_rwlock_wrlock(&neighbor_table_lock);
            HASH_FIND(hh, neighbors, shost, ETHER_ADDR_LEN, n);
            if(n) {
                n->lastseen = time(NULL);
                n->distance = min(n->distance, helloTTL - msg->ttl + 1);
                dessert_debug("updating neighbor: " MAC ", seq=%d, dist=%d", EXPLODE_ARRAY6(shost), msg->u16, n->distance);
                pthread_rwlock_unlock(&neighbor_table_lock);
                /*###*/
            }
            else {
                pthread_rwlock_unlock(&neighbor_table_lock);
                /*###*/
                add_neighbor(shost, helloTTL - msg->ttl + 1);
            }

            if(msg->ttl > 0) {
                msg->ttl--;
                dessert_debug("forwarding HELLO: " MAC ", seq=%d", EXPLODE_ARRAY6(shost), msg->u16);
                dessert_meshif_t* iface = NULL;
                logHelloFW(msg, 0, NULL, iface);
                dessert_meshsend(msg, iface);
            }
        }
        else {
            dessert_warning("got HELLO packet but HELLOs are disabled");
        }
        return DESSERT_MSG_DROP;
    }
    return DESSERT_MSG_KEEP;
}

void attachNodeList(dessert_msg_t* msg){
    neighbor_t* n;
    dessert_ext_t *ext;

    if(dessert_msg_getext(msg, &ext, NODE_LIST, 0)) {
        dessert_msg_delext(msg, ext);
    }

    // room for up to 40 neighbors
    uint8_t list_len = min(ETHER_ADDR_LEN*numNeighbors(), 240);
    dessert_msg_addext(msg, &ext, NODE_LIST, list_len);
    if(!msg) {
        dessert_crit("could not allocate memory");
        return;
    }
    dessert_debug("reserved space for %d neighbors in extension", list_len);
    uint8_t* ptr = ext->data;

    /*###*/
    pthread_rwlock_rdlock(&neighbor_table_lock);
    for(n = neighbors; n != NULL; n = n->hh.next) {
        memcpy(ptr, n->addr, ETHER_ADDR_LEN);
        ptr += ETHER_ADDR_LEN;
    }
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/
}

/** Determine the number of shared neighbors for gossip9
 *
 * @return number of neighors shared by the packet sender and this node
 */
uint8_t coveredNeighbors(dessert_msg_t* msg, dessert_meshif_t *iface){
    dessert_ext_t *ext;
    neighbor_t* n;
    uint8_t covered_neighbors = 0;

    if(dessert_msg_getext(msg, &ext, NODE_LIST, 0)) {
        uint8_t list_len = ext->len;

        /*###*/
        pthread_rwlock_rdlock(&neighbor_table_lock);
        for(n = neighbors; n != NULL; n = n->hh.next) {
            uint8_t* neigh_ptr = ext->data;
            uint8_t i = 0;
            while(i < list_len) {
                if(!memcmp(n->addr, neigh_ptr+i, ETHER_ADDR_LEN)) {
                    covered_neighbors++;
                }
                i += ETHER_ADDR_LEN;
            }
        }
        pthread_rwlock_unlock(&neighbor_table_lock);
        /*###*/
    }
    return covered_neighbors;
}

void update_neighborMon() {
    pthread_mutex_lock(&schedule_mutex);
    if(task_hello) {
        dessert_periodic_del(task_hello);
        task_hello = NULL;

        struct timeval interval;
        interval.tv_sec = hello_interval.tv_sec;
        interval.tv_usec = hello_interval.tv_usec;
        task_hello = dessert_periodic_add((dessert_periodiccallback_t *) send_hello, NULL, NULL, &interval);
    }

    if(task_cleanup) {
        dessert_periodic_del(task_cleanup);
        task_cleanup = NULL;

        struct timeval interval;
        interval.tv_sec = cleanup_interval.tv_sec;
        interval.tv_usec = cleanup_interval.tv_usec;
        task_cleanup = dessert_periodic_add((dessert_periodiccallback_t *) cleanup_neighbors, NULL, NULL, &interval);
    }
    pthread_mutex_unlock(&schedule_mutex);
    dessert_notice("updated neighbor discovery configuration");
}

void start_neighborMon() {
    reset_neighbors();
    pthread_mutex_lock(&schedule_mutex);
    if(!task_hello) {
        struct timeval interval;
        interval.tv_sec = hello_interval.tv_sec;
        interval.tv_usec = hello_interval.tv_usec;
        struct timeval scheduled;
        gettimeofday(&scheduled, NULL);
        TIMEVAL_ADD(&scheduled, hello_interval.tv_sec, hello_interval.tv_usec);
        task_hello = dessert_periodic_add((dessert_periodiccallback_t *) send_hello, NULL, &scheduled, &interval);
    }
    if(!task_cleanup) {
        struct timeval interval;
        interval.tv_sec = cleanup_interval.tv_sec;
        interval.tv_usec = cleanup_interval.tv_usec;
        struct timeval scheduled;
        gettimeofday(&scheduled, NULL);
        TIMEVAL_ADD(&scheduled, cleanup_interval.tv_sec, cleanup_interval.tv_usec);
        task_cleanup = dessert_periodic_add((dessert_periodiccallback_t *) cleanup_neighbors, NULL, &scheduled, &interval);
    }
    pthread_mutex_unlock(&schedule_mutex);
    dessert_notice("started neighbor discovery");
}

void stop_neighborMon() {
    pthread_mutex_lock(&schedule_mutex);
    if(task_hello) {
        dessert_periodic_del(task_hello);
        task_hello = NULL;
    }
    if(task_cleanup) {
        dessert_periodic_del(task_cleanup);
        task_cleanup = NULL;
    }
    pthread_mutex_unlock(&schedule_mutex);
    dessert_notice("stopped neighbor discovery");
}

/******************************************************************************/

int cli_showneighbors(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(!(activated & USE_HELLOS)) {
        cli_print(cli, "HELLOs not enabled");
        return CLI_OK;
    }
    neighbor_t* n;
    uint16_t c = 0;

    /*###*/
    pthread_rwlock_rdlock(&neighbor_table_lock);
    cli_print(cli, "neighbors:");
    for(n = neighbors, c=0; n != NULL; n = n->hh.next, c++) {
        cli_print(cli, "%d = " MAC ", dist=%d", c, EXPLODE_ARRAY6(n->addr), n->distance);
    }
    pthread_rwlock_unlock(&neighbor_table_lock);
    /*###*/

    return CLI_OK;
}
