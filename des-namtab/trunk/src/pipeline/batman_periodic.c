/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
       http://www.des-testbed.net
*******************************************************************************/

#include <string.h>
#include "../database/batman_database.h"
#include "batman_pipeline.h"
#include <pthread.h>
#include <utlist.h>

dessert_periodic_t* ogm_periodic = NULL;

uint16_t 	sequence_num = 0;
uint8_t	reset_flag_counter = OGM_RESET_COUNT;

pthread_rwlock_t snapshot_rwlock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct rt_snapshot {
    uint8_t source_addr[ETH_ALEN];
    uint8_t interface_num;
    uint8_t next_hop[ETH_ALEN];
    struct rt_snapshot* next, *prev;
} rt_snapshot_t;

rt_snapshot_t* rt_list = NULL;
rt_snapshot_t* last_rt_el = NULL;
uint16_t rt_count = 0;
uint16_t last_rt_index = 0;
uint8_t max_ext_cnt_size = DESSERT_MAXEXTDATALEN / sizeof(struct batman_ogm_invrt);

void destroy_rt_snapshot() {
    rt_snapshot_t* el = rt_list;
    rt_snapshot_t* next_el;

    while(rt_list != NULL) {
        next_el = el->next;
        DL_DELETE(rt_list, el);
        free(el);
        el = next_el;
    }

    rt_count = last_rt_index = 0;
}

void create_rt_snapshot() {
    pthread_rwlock_wrlock(&snapshot_rwlock);
    destroy_rt_snapshot();
    batman_db_rlock();
    batman_irt_entry_t* irt_entrys = batman_db_irt_getinvrt();

    while(irt_entrys != NULL) {
        rt_snapshot_t* el = malloc(sizeof(rt_snapshot_t));

        if(el == NULL) {
            break;
        }

        memcpy(el->source_addr, irt_entrys->ether_source_addr, ETH_ALEN);
        memcpy(el->next_hop, irt_entrys->best_output_iface->nht->best_next_hop, ETH_ALEN);
        el->interface_num = irt_entrys->best_output_iface->iface_num;
        DL_APPEND(rt_list, el);
        irt_entrys = irt_entrys->hh.next;
        rt_count ++;
    }

    batman_db_unlock();
    last_rt_el = rt_list;
    pthread_rwlock_unlock(&snapshot_rwlock);
}

int batman_periodic_send_ogm(void* data, struct timeval* scheduled, struct timeval* interval) {
    dessert_msg_t* ogm_msg;
    dessert_ext_t* ext;

    // increment sequence number
    sequence_num ++;

    // create new ogm message with ogm_ext and l25 header for TTL.
    dessert_msg_new(&ogm_msg);
    ogm_msg->ttl = TTL_MAX;

    dessert_msg_addext(ogm_msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    struct ether_header* l25h = (struct ether_header*) ext->data;
    // set src address
    memcpy(l25h->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    // set destination broadcast address
    memcpy(l25h->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_addext(ogm_msg, &ext, OGM_EXT_TYPE, OGM_EXT_LEN);
    struct batman_msg_ogm* ogm_ext = (struct batman_msg_ogm*) ext->data;
    ogm_ext->version = VERSION;
    ogm_ext->flags = BATMAN_OGM_UFLAG; // set unidirectional flag

    if(reset_flag_counter > 0) {
        reset_flag_counter--;
        ogm_ext->flags = ogm_ext->flags | BATMAN_OGM_RFLAG;
    }

    ogm_ext->sequence_num = sequence_num;
    ogm_ext->precursors_count = 0;

    // add inverted routing table
    // compute required space
    if(last_rt_el == NULL) {
        create_rt_snapshot();
    }

    pthread_rwlock_rdlock(&snapshot_rwlock);
    dessert_ext_t* irt_ext;
    int msg_ext_count = 0;
    uint8_t	MAX_IRT_EXT_COUNT = (dessert_maxlen - sizeof(dessert_msg_t)
                                 - sizeof(struct ether_header)
                                 - sizeof(struct batman_msg_ogm)) / DESSERT_MAXEXTDATALEN;

    while(rt_count - last_rt_index > 0 && msg_ext_count < MAX_IRT_EXT_COUNT) {
        uint16_t ext_size = ((rt_count - last_rt_index) >= max_ext_cnt_size) ? max_ext_cnt_size : (rt_count - last_rt_index);
        dessert_msg_addext(ogm_msg, &irt_ext, OGM_INVRT_EXT_TYPE, ext_size * sizeof(struct batman_ogm_invrt));
        void* pointer = irt_ext->data;

        while(ext_size-- > 0) {
            struct batman_ogm_invrt* irt_data = pointer;
            pointer += sizeof(struct batman_ogm_invrt);
            memcpy(irt_data->source_addr, last_rt_el->source_addr, ETH_ALEN);
            memcpy(irt_data->next_hop, last_rt_el->next_hop, ETH_ALEN);
            irt_data->output_iface_num = last_rt_el->interface_num;
            last_rt_index++;
            last_rt_el = last_rt_el->next;
        }

        msg_ext_count++;
    }

    pthread_rwlock_unlock(&snapshot_rwlock);
    // send OGM
    uint8_t if_count = 0;
    const dessert_meshif_t* iface = dessert_meshiflist_get();

    while(iface != NULL) {
        if_count++;
        iface = iface->next;
    }

    const dessert_meshif_t* if_arr[if_count];

    int i;

    for(i = 0; i < if_count; i++) {
        const dessert_meshif_t* curr_if = NULL;
        iface = dessert_meshiflist_get();

        while(curr_if == NULL) {
            if(iface == NULL) {
                iface = dessert_meshiflist_get();
            }

            uint8_t r = rand() % 100;
            uint8_t lim = 100 % (if_count - i);

            if(r <= lim) {
                int find_cand = true;

                while(find_cand == true) {
                    int add = true;
                    int j;

                    for(j = 0; j < i; j++) {
                        if(if_arr[j] == iface) {
                            add = false;
                            break;
                        }
                    }

                    if(add == true) {
                        find_cand = false;
                    }
                    else {
                        iface = iface->next;

                        if(iface == NULL) {
                            iface = dessert_meshiflist_get();
                        }
                    }
                }

                curr_if = iface;
            }

            iface = iface->next;
        }

        if_arr[i] = curr_if;
    }

    for(i = 0; i < if_count; i++) {
        ogm_ext->output_iface_num = if_arr[i]->if_index;
        dessert_meshsend_fast(ogm_msg, if_arr[i]);
    }

    dessert_msg_destroy(ogm_msg);
    return 0;
}

int batman_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval) {
    batman_db_wlock();
    batman_db_cleanup();
    batman_db_unlock();
    return 0;
}

int batman_periodic_register_send_ogm(time_t ogm_int) {
    // change pudge timeout
    batman_db_wlock();
    batman_db_change_pt(PURGE_TIMEOUT_KOEFF * ogm_int * WINDOW_SIZE);
    batman_db_unlock();
    // update callback
    struct timeval send_ogm_interval;
    dessert_debug("set OGM interval to %i sek", ogm_int);
    send_ogm_interval.tv_sec = ogm_int;
    send_ogm_interval.tv_usec = 0;

    if(ogm_periodic != NULL) {
        dessert_periodic_del(ogm_periodic);
    }

    ogm_periodic = dessert_periodic_add(batman_periodic_send_ogm, NULL, NULL, &send_ogm_interval);
    return true;
}
