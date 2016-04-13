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

dessert_periodic_t* ogm_periodic = NULL;

uint16_t sequence_num = 0;
uint8_t reset_flag_counter = OGM_RESET_COUNT;

dessert_per_result_t batman_periodic_send_ogm(void* data, struct timeval* scheduled, struct timeval* interval) {
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

    dessert_msg_addext(ogm_msg, &ext, OGM_EXT_TYPE, sizeof(struct batman_msg_ogm));
    struct batman_msg_ogm* ogm_ext = (struct batman_msg_ogm*) ext->data;
    ogm_ext->version = VERSION;
    ogm_ext->flags = BATMAN_OGM_UFLAG; // set unidirectional flag

    if(reset_flag_counter > 0) {
        reset_flag_counter--;
        ogm_ext->flags = ogm_ext->flags | BATMAN_OGM_RFLAG;
    }

    ogm_ext->gw_flags = 0x00;
    ogm_ext->sequence_num = sequence_num;
    ogm_ext->gw_port = 0x00;
    ogm_ext->precursors_count = 0;

    dessert_msg_dummy_payload(ogm_msg, ogm_size);

    dessert_meshsend_fast(ogm_msg, NULL);
    dessert_msg_destroy(ogm_msg);
    return DESSERT_PER_KEEP;
}

dessert_per_result_t batman_periodic_log_rt(void* data, struct timeval* scheduled, struct timeval* interval) {
    char* rt_str;
    batman_db_rlock();
    batman_db_view_routingtable(&rt_str);
    batman_db_unlock();
    dessert_info("\n%s", rt_str);
    free(rt_str);
    return DESSERT_PER_KEEP;
}



dessert_per_result_t batman_periodic_cleanup_database(void* data, struct timeval* scheduled, struct timeval* interval) {
    batman_db_wlock();
    batman_db_cleanup();
    batman_db_unlock();
    return DESSERT_PER_KEEP;
}

int batman_periodic_register_send_ogm(time_t ogm_int) {
    // change pudge timeout
    batman_db_wlock();
    batman_db_change_pt(PURGE_TIMEOUT_KOEFF * ogm_int * window_size);
    batman_db_unlock();
    // update callback
    struct timeval send_ogm_interval;
    dessert_debug("set OGM interval to %i sek", ogm_int);
    send_ogm_interval.tv_sec = ogm_int / 1000;
    send_ogm_interval.tv_usec = (ogm_int % 1000) * 1000;

    if(ogm_periodic != NULL) {
        dessert_periodic_del(ogm_periodic);
    }

    ogm_periodic = dessert_periodic_add(batman_periodic_send_ogm, NULL, NULL, &send_ogm_interval);
    return true;
}
