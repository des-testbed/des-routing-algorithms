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

#include "batman_database.h"
#include "../config.h"
#include "routing_table/batman_rt.h"
#include "neighbor_table/batman_nt.h"
#include "rl_seq_t/rl_seq.h"
#include <pthread.h>
#include "broadcast_log/broadcast_log.h"

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void batman_db_rlock() {
    pthread_rwlock_rdlock(&rwlock);
}

void batman_db_wlock() {
    pthread_rwlock_wrlock(&rwlock);
}

void batman_db_unlock() {
    pthread_rwlock_unlock(&rwlock);
}

int batman_db_init() {
    batman_db_rt_init();
    batman_db_brct_init();

    if(batman_db_nt_init() == false) {
        return false;
    }

    if(rl_table_init() == false) {
        return false;
    }

    return true;
}

int batman_db_cleanup() {
    if(batman_db_rt_cleanup() == false) {
        return false;
    }

    return true;
}

int batman_db_change_pt(time_t pudge_timeout) {
    batman_db_rt_change_pt(pudge_timeout);
    batman_db_nt_change_pt(pudge_timeout);
    return true;
}

int batman_db_captureroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* local_iface, time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num) {
    return batman_db_rt_addroute(ether_dest_addr, local_iface, timestamp, ether_nexthop_addr, seq_num);
}

int batman_db_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]) {
    return batman_db_rt_deleteroute(ether_dest_addr);
}

int batman_db_getbestroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** local_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN]) {
    return batman_db_rt_getbestroute(ether_dest_addr, local_iface_out, ether_nexthop_addr_out);
}

int batman_db_getbestroute_arl(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN], uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN], uint8_t* presursosr_iface_count) {
    return batman_db_rt_getbestroute_arl(ether_dest_addr, ether_iface_out, ether_nexthop_addr_out, precursors_iface_list, presursosr_iface_count);
}

int batman_db_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]) {
    return batman_db_rt_getroutesn(ether_dest_addr);
}

/**
 * Write to DB that there exist bidirectional connection between this to neighbors
 */
int batman_db_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* local_iface) {
    return batman_db_nt_cap2Dneigh(ether_neighbor_addr, local_iface);
}

/**
 * Check if there exist bidirectional connection between this to neighbors
 */
int batman_db_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], dessert_meshif_t* local_iface) {
    return batman_db_nt_check2Dneigh(ether_neighbor_addr, local_iface);
}

int batman_db_addbrcid(uint8_t source_addr[ETH_ALEN], uint16_t id) {
    return batman_db_brct_addid(source_addr, id);
}

int batman_db_addogmseq(uint8_t source_addr[ETH_ALEN], uint16_t seq) {
    return batman_db_ogm_addid(source_addr, seq);
}

void batman_db_change_window_size(uint8_t window_size) {
    batman_db_rt_change_window_size(window_size);
}

// ------------------- reporting -----------------------------------------------

int batman_db_view_routingtable(char** str_out) {
    return batman_db_rt_report(str_out);
}
