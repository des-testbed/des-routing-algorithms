/******************************************************************************
 Copyright 2009, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
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
 ------------------------------------------------------------------------------

 ******************************************************************************/

#ifndef LINKCACHE_H_
#define LINKCACHE_H_

#include "dsr.h"

#define DSR_LINKCACHE_SUCCESS                             0
#define DSR_LINKCACHE_ERROR_MEMORY_ALLOCATION            -1
#define DSR_LINKCACHE_ERROR_NO_SUCH_LINK                 -2
#define DSR_LINKCACHE_ERROR_LINK_ALREADY_IN_CACHE        -3

extern pthread_rwlock_t _dsr_linkcache_rwlock;
extern struct dsr_linkcache* dsr_linkcache;

int dsr_linkcache_init(uint8_t default_src[ETHER_ADDR_LEN]);
int dsr_linkcache_add_link(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t weight);
int dsr_linkcache_remove_link(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
int dsr_linkcache_get_weight(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t* weight);
int dsr_linkcache_set_weight(uint8_t u[ETHER_ADDR_LEN], uint8_t v[ETHER_ADDR_LEN], uint16_t* weight);
int dsr_linkcache_get_shortest_path(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN], dsr_path_t** path);
int dsr_linkcache_run_dijkstra(uint8_t u[ETHER_ADDR_LEN]);
void dsr_linkcache_print(void);

int dessert_cli_cmd_showlinkcache(struct cli_def* cli, char* command, char* argv[], int argc);

dessert_per_result_t dsr_linkcache_run_dijkstra_periodic(void* data, struct timeval* scheduled, struct timeval* interval);

typedef struct dsr_adjacency_list {
    uint8_t address[ETHER_ADDR_LEN]; /* key */
    struct dsr_linkcache* hop;
    uint16_t weight; /* in 'fixed point' notation, e.g. 1.00 is 100 . MAX is 65535   */
    UT_hash_handle hh;
} dsr_adjacency_list_t;

typedef struct dsr_linkcache {
    uint8_t address[ETHER_ADDR_LEN]; /* key */
    struct dsr_adjacency_list* adj;
    uint8_t in_use; /** counts how often v appears in other nodes adjacency lists*/
    UT_hash_handle hh;
    /* -------------------------------DIJKSTRA------------------------------- */
    uint16_t d;                 /** shortest-path estimate             */
    struct dsr_linkcache* p;   /** predecessor                        */
    UT_hash_handle qh;         /** hash handle for min priority queue */
    /* -------------------------------DIJKSTRA------------------------------- */
} dsr_linkcache_t;

#define WEIGHT(u,v) _dsr_linkcache_get_weight_XXX(u->address,v->address)
#define HOPCOUNT(u,v) 100

/* -------------------------------DIJKSTRA----------------------------------- */

#define INFINITE (UINT16_MAX)

#define D(u) u->d
#define P(u) u->p

#define INITIALIZE_SINGLE_SOURCE(G,s) do {                                     \
	                                      dsr_linkcache_t *v;                  \
                                          HASH_FOREACH(hh, G,v){               \
                                             D(v) = INFINITE;                  \
                                             P(v) = NULL;                      \
                                          }                                    \
                                          D(s) = 0;                            \
                                      } while(0)

#define RELAX(u,v,w) do {                                                      \
		               if(D(v) > (D(u) + w(u,v))){                             \
                           D(v) = (D(u) + w(u,v));                             \
                           P(v) = u;                                           \
                       }                                                       \
                   } while(0)

#define EXTRACT_MIN(Q,u) do {                                                  \
	                         dsr_linkcache_t *x;                               \
	                         u = Q;                                            \
	                         HASH_FOREACH(qh, Q,x) {                           \
	                        	if(x->d < u->d) u = x;                         \
	                         }                                                 \
	                         HASH_DELETE(qh,Q,u);                              \
                         } while(0)

/** ***************************************************************************
 * Dijkstra's algorithm for single-source shortest paths, adaptation from     *
 * Introduction to Algorithms, 2/e, Cormen et. al., p.595                     *
 * O(|V|^2 + |E|) due to the linear EXTRACT_MIN based on a linked list        *
 ******************************************************************************/
#define DIJKSTRA(G,w,s) do {                                                   \
                            dsr_linkcache_t *Q = NULL; /* MUST for uthash */   \
                            dsr_linkcache_t *u;                                \
                            dsr_adjacency_list_t *v;                           \
                                                                               \
	                        INITIALIZE_SINGLE_SOURCE(G,s);                     \
	                        HASH_SELECT(qh,Q,hh,G,COND_true); /* Q <- V[G] */  \
                            while (HASH_CNT(qh,Q) > 0) {  /* Q != {} */        \
                                EXTRACT_MIN(Q,u);                              \
                                HASH_FOREACH(hh, u->adj,v) RELAX(u,v->hop,w);  \
                            }                                                  \
                        } while(0)

/* -------------------------------DIJKSTRA-helpers--------------------------- */

#define COND_true(x) 1

/* -------------------------------DIJKSTRA----------------------------------- */

#endif /* LINKCACHE_H_ */
