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

#include "pdr.h"
#include "../../config.h"

pdr_neighbor_entry_t* pdr_neighbor_entry_create(mac_addr ether_neighbor_addr, uint16_t hello_interv) {
    pdr_neighbor_entry_t* new_entry;
    new_entry = malloc(sizeof(pdr_neighbor_entry_t));

    if(new_entry == NULL) {
        return NULL;
    }

    mac_copy(new_entry->ether_neighbor, ether_neighbor_addr);
    new_entry->rcvd_hello_count = 0;
    new_entry->nb_rcvd_hello_count = 0;
    new_entry->hello_interv = hello_interv;
    new_entry->msg_list = NULL;

    if(hello_interv*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
        new_entry->expected_hellos = tracking_factor;
    }
    else {
        new_entry->expected_hellos = PDR_MIN_TRACKING_INTERVAL / hello_interv;
    }

    uint32_t purge_ms = (uint32_t) hello_interv * tracking_factor * PDR_TRACKING_PURGE_FACTOR;
    dessert_ms2timeval(purge_ms, &new_entry->purge_tv);

    return new_entry;
}

pdr_neighbor_hello_msg_t* pdr_hello_entry_create(uint16_t hello_seq) {
    pdr_neighbor_hello_msg_t* new_entry;
    new_entry = malloc(sizeof(pdr_neighbor_hello_msg_t));

    if(new_entry == NULL) {
        return NULL;
    }

    new_entry->seq_num = hello_seq;

    return new_entry;
}

void pdr_neighbor_entry_update(pdr_neighbor_entry_t* update_entry, uint16_t new_interval) {
    update_entry->hello_interv = new_interval;
    if(new_interval*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
        update_entry->expected_hellos = tracking_factor;
    }
    else {
        update_entry->expected_hellos = PDR_MIN_TRACKING_INTERVAL / new_interval;
    }

    uint32_t purge_ms = (uint32_t) new_interval * tracking_factor * PDR_TRACKING_PURGE_FACTOR;
    dessert_ms2timeval(purge_ms, &update_entry->purge_tv);
}

void pdr_nt_purge_hello_msg(struct timeval* timestamp, void* src_object, void* object) {
    pdr_neighbor_entry_t* curr_entry = src_object;
    pdr_neighbor_hello_msg_t* curr_hello = object;
    HASH_DEL(curr_entry->msg_list, curr_hello);

    curr_entry->rcvd_hello_count -= 1;
    free(curr_hello);
}

void pdr_nt_purge_nb(struct timeval* timestamp, void* src_object, void* del_object) {
    pdr_neighbor_entry_t* nb_entry = del_object;

    dessert_info("Delete entry in pdr tracker for " MAC " due to no hello communication", EXPLODE_ARRAY6(nb_entry->ether_neighbor));
    pdr_nt_msg_destroy(nb_entry);
    timeslot_destroy(nb_entry->ts);
    HASH_DEL(pdr_nt.entries, nb_entry);
    free(nb_entry);
}

int aodv_db_pdr_nt_init() {
    pdr_nt.entries = NULL;

    if(hello_interval*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
        pdr_nt.nb_expected_hellos = tracking_factor;
    }
    else {
        pdr_nt.nb_expected_hellos = (uint16_t) PDR_MIN_TRACKING_INTERVAL / hello_interval;
    }

    //creating default purge timeout, should normally not be used when adding an entry
    //but needed for initialization
    uint32_t def_purge_ms = (uint32_t) hello_interval * tracking_factor * PDR_TRACKING_PURGE_FACTOR;
    struct timeval def_purge_tv;
    dessert_ms2timeval(def_purge_ms, &def_purge_tv);

    return timeslot_create(&pdr_nt.ts, &def_purge_tv, &pdr_nt, pdr_nt_purge_nb);
}

int aodv_db_pdr_nt_upd_expected(uint16_t new_interval) {
    if(new_interval*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
        pdr_nt.nb_expected_hellos = tracking_factor;
    }
    else {
        pdr_nt.nb_expected_hellos = (uint16_t) PDR_MIN_TRACKING_INTERVAL / new_interval;
    }
    return true;
}

int pdr_nt_neighbor_destroy(uint32_t* count_out) {
    *count_out = 0;

    pdr_neighbor_entry_t* neigh = NULL;
    pdr_neighbor_entry_t* tmp = NULL;
    HASH_ITER(hh, pdr_nt.entries, neigh, tmp) {
        pdr_nt_msg_destroy(neigh);
        timeslot_destroy(neigh->ts);
        HASH_DEL(pdr_nt.entries, neigh);
        free(neigh);
        (*count_out)++;
    }
    return true;
}

int pdr_nt_msg_destroy(pdr_neighbor_entry_t* curr_nb) {
    pdr_neighbor_hello_msg_t* nb_msg = NULL;
    pdr_neighbor_hello_msg_t* tmp = NULL;
    HASH_ITER(hh, curr_nb->msg_list, nb_msg, tmp) {
        HASH_DEL(curr_nb->msg_list, nb_msg);
        free(nb_msg);
    }
    return true;
}

int aodv_db_pdr_nt_neighbor_reset(uint32_t* count_out) {

    int result = true;
    
    result &= pdr_nt_neighbor_destroy(count_out);
    result &= aodv_db_pdr_nt_init();

    return result;
}

int aodv_db_pdr_nt_cleanup(struct timeval* timestamp) {
    return timeslot_purgeobjects(pdr_nt.ts, timestamp);
}

int aodv_db_pdr_nt_cap_hello(mac_addr ether_neighbor_addr, uint16_t hello_seq, uint16_t hello_interv, struct timeval* timestamp) {
    struct timeval teststamp;
    teststamp.tv_sec = timestamp->tv_sec;
    teststamp.tv_usec = timestamp->tv_usec;
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL) {
        //start new pdr tracker
        curr_entry = pdr_neighbor_entry_create(ether_neighbor_addr, hello_interv);

        if(curr_entry == NULL) {
            return false;
        }

        /** Determine Track Interval with tracking_factor*hello_interv - Minimum is 500 ms*/
        struct timeval pdr_watch_interval;
        if (hello_interv*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
            uint32_t tracking_interval = hello_interv * tracking_factor;
            dessert_ms2timeval(tracking_interval, &pdr_watch_interval);
        }
        else {
            dessert_ms2timeval(PDR_MIN_TRACKING_INTERVAL, &pdr_watch_interval);
        }

        if(timeslot_create(&(curr_entry->ts), &pdr_watch_interval, curr_entry, pdr_nt_purge_hello_msg) != true) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, pdr_nt.entries, curr_entry->ether_neighbor, ETH_ALEN, curr_entry);
        dessert_info("New neighbor entry with %" PRIu16 " expected hellos in pdr tracker created for " MAC, curr_entry->expected_hellos, EXPLODE_ARRAY6(ether_neighbor_addr));
    }
    else if (curr_entry->hello_interv != hello_interv) {
        dessert_debug("Neighbor " MAC " switched his hello interval from %" PRIu16 " ms to %" PRIu16 " ms",EXPLODE_ARRAY6(ether_neighbor_addr), curr_entry->hello_interv, hello_interv);
        pdr_neighbor_entry_update(curr_entry, hello_interv);
    }

    timeslot_addobject_varpurge(pdr_nt.ts, timestamp, curr_entry, &(curr_entry->purge_tv));

    pdr_neighbor_hello_msg_t* curr_hello = NULL;
    HASH_FIND(hh, curr_entry->msg_list, &hello_seq, 2, curr_hello);
    if(curr_hello == NULL) {
        //this hello seq number is unknown, create new entry
        curr_hello = pdr_hello_entry_create(hello_seq);

        if(curr_hello == NULL) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, curr_entry->msg_list, &(curr_hello->seq_num), 2, curr_hello);
    }

    curr_entry->rcvd_hello_count += 1;

    timeslot_addobject(curr_entry->ts, &teststamp, curr_hello);
    return true;
}

int aodv_db_pdr_nt_cap_hellorsp(mac_addr ether_neighbor_addr, uint16_t hello_interv, uint8_t hello_count, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL) {
        //start new pdr tracker for neighbor
        curr_entry = pdr_neighbor_entry_create(ether_neighbor_addr, hello_interv);

        if(curr_entry == NULL) {
            return false;
        }

        /** Determine Track Interval with tracking_factor*hello_interv - Minimum is 500 ms*/
        struct timeval pdr_watch_interval;
        if (hello_interv*tracking_factor >= PDR_MIN_TRACKING_INTERVAL) {
            uint32_t tracking_interval = hello_interv * tracking_factor;
            dessert_ms2timeval(tracking_interval, &pdr_watch_interval);
        }
        else {
            dessert_ms2timeval(PDR_MIN_TRACKING_INTERVAL, &pdr_watch_interval);
        }

        if(timeslot_create(&(curr_entry->ts), &pdr_watch_interval, curr_entry, pdr_nt_purge_hello_msg) != true) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, pdr_nt.entries, curr_entry->ether_neighbor, ETH_ALEN, curr_entry);
        dessert_info("New neighbor entry with %" PRIu16 " expected hellos in pdr tracker created for " MAC, curr_entry->expected_hellos, EXPLODE_ARRAY6(ether_neighbor_addr));
    }
    else if (curr_entry->hello_interv != hello_interv) {
        dessert_info("Neighbor " MAC " switched his hello interval from %" PRIu16 " ms to %" PRIu16 " ms",EXPLODE_ARRAY6(ether_neighbor_addr), curr_entry->hello_interv, hello_interv);
        pdr_neighbor_entry_update(curr_entry, hello_interv);
    }

    timeslot_addobject_varpurge(pdr_nt.ts, timestamp, curr_entry, &(curr_entry->purge_tv));

    curr_entry->nb_rcvd_hello_count = hello_count;

    return true;
}

int pdr_nt_cleanup(pdr_neighbor_entry_t* given_entry, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = given_entry;
    return timeslot_purgeobjects(curr_entry->ts, timestamp);
}

int aodv_db_pdr_nt_get_pdr(mac_addr ether_neighbor_addr, metric_t* pdr_out, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL){
        return false;
    }

    pdr_nt_cleanup(curr_entry, timestamp);

    /** Encode pdr as uint16_t value*/
    if(curr_entry->rcvd_hello_count >= curr_entry->expected_hellos) {
        *pdr_out = AODV_MAX_METRIC;
    }
    else {
        *pdr_out = (metric_t)((uintmax_t)AODV_MAX_METRIC * curr_entry->rcvd_hello_count / curr_entry->expected_hellos);
    }
    return true;
}

int aodv_db_pdr_nt_get_etx_mul(mac_addr ether_neighbor_addr, metric_t* etx_out, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL){
        return false;
    }

    pdr_nt_cleanup(curr_entry, timestamp);

    /* clamp rcvd counts to prevent pdr's over 1 */
    uintmax_t    rcvd_hellos  = min(curr_entry->rcvd_hello_count, curr_entry->expected_hellos);
    uintmax_t nb_rcvd_hellos  = min(curr_entry->nb_rcvd_hello_count, pdr_nt.nb_expected_hellos);

    /* this is equivalent to round_trip_pdr = AODV_MAX_METRIC * pdr * nb_pdr, just reordered operations to allow integer arithmetic */
    uintmax_t round_trip_pdr  = (uintmax_t) AODV_MAX_METRIC * rcvd_hellos * nb_rcvd_hellos;
              round_trip_pdr /= (uintmax_t) curr_entry->expected_hellos * pdr_nt.nb_expected_hellos;

    *etx_out = (metric_t) round_trip_pdr;
    return true;
}

int aodv_db_pdr_nt_get_etx_add(mac_addr ether_neighbor_addr, metric_t* etx_out, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL){
        return false;
    }

    pdr_nt_cleanup(curr_entry, timestamp);

    if(curr_entry->rcvd_hello_count == 0 || curr_entry->nb_rcvd_hello_count == 0) {
        *etx_out = AODV_MAX_METRIC;
    }

    /* clamp rcvd counts to prevent pdr's over 1 */
    uintmax_t    rcvd_hellos  = min(curr_entry->rcvd_hello_count, curr_entry->expected_hellos);
    uintmax_t nb_rcvd_hellos  = min(curr_entry->nb_rcvd_hello_count, pdr_nt.nb_expected_hellos);

    /* this is equivalent to etx = 256 / (pdr * nb_pdr), just reordered operations to allow integer arithmetic */
    uintmax_t etx  = (uintmax_t) 0x100 * curr_entry->expected_hellos * pdr_nt.nb_expected_hellos;
              etx /= (uintmax_t) rcvd_hellos * nb_rcvd_hellos;

    *etx_out = (metric_t) min(etx, (uintmax_t)AODV_MAX_METRIC);
    return true;
}

int aodv_db_pdr_nt_get_rcvdhellocount(mac_addr ether_neighbor_addr, uint8_t* count_out, struct timeval* timestamp) {
    pdr_neighbor_entry_t* curr_entry = NULL;
    HASH_FIND(hh, pdr_nt.entries, ether_neighbor_addr, ETH_ALEN, curr_entry);

    if(curr_entry == NULL){
        return false;
    }

    pdr_nt_cleanup(curr_entry, timestamp);

    *count_out = curr_entry->rcvd_hello_count;
    if(*count_out > 100) {
        dessert_debug("Returned %" PRIu16 " rcvd hellos for neighbor " MAC " in tracker interval",(*count_out), EXPLODE_ARRAY6(ether_neighbor_addr));
    }
    return true;
}

int aodv_db_pdr_nt_report(char** str_out) {
    pdr_neighbor_entry_t* current_entry = pdr_nt.entries;
    char* output;
    char entry_str[REPORT_RT_STR_LEN  + 1];

    uint32_t len = 0;

    while(current_entry != NULL) {
        len += REPORT_RT_STR_LEN * 2;
        current_entry = current_entry->hh.next;
    }

    current_entry = pdr_nt.entries;
    output = malloc(sizeof(char) * REPORT_RT_STR_LEN * (4 + len) + 1);

    if(output == NULL) {
        return false;
    }

    output[0] = '\0';
    strcat(output, "+-------------------+-------------------+-------------------+-------------------+----------------------+\n"
           "|     neighbor      |  hello interval   |  received hellos  |  expected hellos  | neighbor rcvd hellos |\n"
           "+-------------------+-------------------+-------------------+-------------------+----------------------+\n");

    while(current_entry != NULL) {
        snprintf(entry_str, REPORT_RT_STR_LEN, "| " MAC " |      %" PRIu16 " ms      |        %" PRIu8 "        |       %" PRIu16 "       |        %" PRIu8 "        |\n", EXPLODE_ARRAY6(current_entry->ether_neighbor), current_entry->hello_interv, current_entry->rcvd_hello_count, current_entry->expected_hellos, current_entry->nb_rcvd_hello_count);
        strcat(output, entry_str);
        strcat(output, "+-------------------+-------------------+-------------------+-------------------+----------------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return true;
}
