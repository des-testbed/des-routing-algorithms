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

#include <time.h>
#include <dessert.h>
#include "../../config.h"
#include "../../helper.h"
#include "../timeslot.h"
#include "batman_invrt.h"

#define REPORT_RT_STR_LEN 114

/**
 * B.A.T.M.A.N routig table
 */
struct batman_rt {
    /** pointer to first HASH_MAP entry */
    batman_irt_entry_t* 	entrys;
    /** time-slot to manage (pudge) old entrys */
    timeslot_t*			ts;
} irt;

/** create output interface entry. */
int batman_db_rt_if_entry_create(batman_irt_if_entry_t** rt_if_entry_out,
                                 uint8_t iface_num) {
    batman_irt_if_entry_t* new_entry;
    batman_irt_nht_t* new_nh;

    new_entry = malloc(sizeof(batman_irt_if_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    if(batman_db_inht_create(&new_nh) == false) {
        free(new_entry);
        return false;
    }

    new_entry->iface_num = iface_num;
    new_entry->nht = new_nh;
    *rt_if_entry_out = new_entry;
    return true;
}

/** destroy output interface entry */
int batman_db_rt_if_entry_destroy(batman_irt_if_entry_t* rt_if_entry) {
    batman_db_inht_destroy(rt_if_entry->nht);
    free(rt_if_entry);
    return true;
}

int batman_db_rt_entry_create(batman_irt_entry_t** rt_entry,
                              uint8_t ether_dest_addr[ETH_ALEN], time_t timestamp,
                              batman_irt_if_entry_t* rt_if_entry, uint16_t seq_num) {
    batman_irt_entry_t* new_entry;
    new_entry = malloc(sizeof(batman_irt_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    memcpy(new_entry->ether_source_addr, ether_dest_addr, ETH_ALEN);
    new_entry->if_entrys = NULL;
    HASH_ADD_KEYPTR(hh, new_entry->if_entrys, &rt_if_entry->iface_num, sizeof(uint8_t), rt_if_entry);
    new_entry->best_output_iface = rt_if_entry;
    new_entry->curr_seq_num = seq_num;
    new_entry->last_aw_time = timestamp;
    *rt_entry = new_entry;
    return true;
}

int batman_db_rt_entry_destroy(batman_irt_entry_t* rt_entry) {
    batman_irt_if_entry_t* curr_if_entry;

    while(rt_entry->if_entrys != NULL) {
        curr_if_entry = rt_entry->if_entrys;
        HASH_DEL(rt_entry->if_entrys, curr_if_entry);
        batman_db_rt_if_entry_destroy(curr_if_entry);
    }

    free(rt_entry);
    return true;
}

int batman_db_irt_addroute(uint8_t ether_dest_addr[ETH_ALEN], uint8_t iface_num,
                           time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num) {
    batman_irt_entry_t* rt_entry;
    // First find appropriate routing table entry
    HASH_FIND(hh, irt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    // if not exist -> create!
    if(rt_entry == NULL) {
        batman_irt_if_entry_t* new_if_entry;

        if(batman_db_rt_if_entry_create(&new_if_entry, iface_num) == false) {
            dessert_debug("!!!! MEMALLOC ERROR !!!!");
            return false;
        }

        if(batman_db_rt_entry_create(&rt_entry, ether_dest_addr, timestamp, new_if_entry, seq_num - 1) == false) {
            dessert_debug("!!!! MEMALLOC ERROR !!!!");
            batman_db_rt_if_entry_destroy(new_if_entry);
            return false;
        }

        HASH_ADD_KEYPTR(hh, irt.entrys, rt_entry->ether_source_addr, ETH_ALEN, rt_entry);
        dessert_debug("--- " MAC " - add backward route",
                      ether_dest_addr[0], ether_dest_addr[1], ether_dest_addr[2],
                      ether_dest_addr[3], ether_dest_addr[4], ether_dest_addr[5]);
    }
    else {
        // if exist, but contains no if_entry with iface_num key -> create if_entry
        batman_irt_if_entry_t* new_if_entry;
        HASH_FIND(hh, rt_entry->if_entrys, &iface_num, sizeof(uint8_t), new_if_entry);

        if(new_if_entry == NULL) {
            if(batman_db_rt_if_entry_create(&new_if_entry, iface_num) == false) {
                dessert_debug("!!!! MEMALLOC ERROR !!!!");
                return false;
            }

            HASH_ADD_KEYPTR(hh, rt_entry->if_entrys, &new_if_entry->iface_num, sizeof(uint8_t), new_if_entry);
        }
    }

    /*uint8_t* last_next_hop_addr = (rt_entry->best_output_iface->nht->best_next_hop == NULL)?
    	NULL : rt_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr; // only for debugging*/
    // actualize the BEST NEXT HOP towards destination and BEST_OUTPUT_IFACE for rt_entry
    batman_irt_if_entry_t* curr_iface = rt_entry->if_entrys;

    while(curr_iface != NULL) {
        if(curr_iface->iface_num == iface_num) {
            // actualize BEST NEXT HOP towards destination for given local_interface
            if(batman_db_inht_addseq(curr_iface->nht, ether_nexthop_addr, seq_num) == false) {
                dessert_debug("!!!! MEMALLOC ERROR !!!!");
                return false;
            }
        }
        else {
            // shift sliding window for all other interfaces
            batman_db_inht_shiftuptoseq(curr_iface->nht, seq_num);
        }

        // actualize BEST OUTPUT IFACE
        if(curr_iface->nht->best_next_hop != NULL &&
           (rt_entry->best_output_iface->nht->best_next_hop->sw->size + WINDOW_SWITCH_DIFF <= curr_iface->nht->best_next_hop->sw->size)) {
            rt_entry->best_output_iface = curr_iface;
        }

        batman_irt_if_entry_t* prev_iface = curr_iface;
        curr_iface = curr_iface->hh.next;

        // drop iface entry if no best next hop more for this entry known
        if(prev_iface->nht->best_next_hop == NULL) {
            HASH_DEL(rt_entry->if_entrys, prev_iface);

            if(rt_entry->best_output_iface == prev_iface) {
                rt_entry->best_output_iface = rt_entry->if_entrys;
            }

            batman_db_rt_if_entry_destroy(prev_iface);
        }
    }

    // actualize curr_seq_anum and last_aware_time
    // and
    // add/replace current routing entry in timeslot
    rt_entry->curr_seq_num = seq_num;
    rt_entry->last_aw_time = timestamp;
    timeslot_addobject(irt.ts, timestamp, rt_entry);
    return true;
}

batman_irt_entry_t*  batman_db_irt_getinvrt() {
    return irt.entrys;
}

int batman_db_irt_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]) {
    batman_irt_entry_t* rt_entry;
    // find appropriate routing table entry
    HASH_FIND(hh, irt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return -1;
    }

    return rt_entry->curr_seq_num;
}

int batman_db_irt_cleanup() {
    return timeslot_purgeobjects(irt.ts, time(0));
}

int batman_db_irt_change_pt(time_t pudge_timeout) {
    timeslot_change_pt(irt.ts, pudge_timeout);
    return true;
}

/** routing entry element by timeout */
void pudge_old_destination(time_t lasw_aw_time, void* entry) {
    batman_irt_entry_t* rt_entry = entry;
    dessert_debug("--- " MAC " - inv route timeout",
                  rt_entry->ether_source_addr[0], rt_entry->ether_source_addr[1], rt_entry->ether_source_addr[2],
                  rt_entry->ether_source_addr[3], rt_entry->ether_source_addr[4], rt_entry->ether_source_addr[5]);
    HASH_DEL(irt.entrys, rt_entry);
    batman_db_rt_entry_destroy(rt_entry);
}

int batman_db_irt_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]) {
    batman_irt_entry_t* rt_entry;
    // find appropriate routing table entry
    HASH_FIND(hh, irt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return false;
    }

    if(timeslot_deleteobject(irt.ts, rt_entry) == true) {
        HASH_DEL(irt.entrys, rt_entry);
        batman_db_rt_entry_destroy(rt_entry);
        return true;
    }

    return false;
}

int batman_db_irt_init() {
    irt.entrys = NULL;
    timeslot_t* ts;
    timeslot_create(&ts, PUDGE_TIMEOUT, pudge_old_destination);
    irt.ts = ts;
    return true;
}

// ------------------- reporting -----------------------------------------------

int batman_db_irt_report(char** str_out) {
    batman_irt_entry_t* current_entry = irt.entrys;
    char* output;
    char entry_str[REPORT_RT_STR_LEN  + 1];

    // compute str length
    uint len = 0;

    while(current_entry != NULL) {
        len += REPORT_RT_STR_LEN * (HASH_COUNT(current_entry->if_entrys) + 1);
        current_entry = current_entry->hh.next;
    }

    current_entry = irt.entrys;
    output = malloc(sizeof(char) * REPORT_RT_STR_LEN * (3 + len) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+-----------------+-----------------+---------------+---------------+-------------------+\n");
    strcat(output, "|  destination addr | last aware time | current seq_num | seq_num count | out iface num |   best next hop   |\n");
    strcat(output, "+-------------------+-----------------+-----------------+---------------+---------------+-------------------+\n");

    while(current_entry != NULL) {
        // first line for best output interface
        snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| " MAC " | %15i | %15i | %7i / %3i |*%13i | " MAC " |\n",
                 current_entry->ether_source_addr[0], current_entry->ether_source_addr[1],
                 current_entry->ether_source_addr[2], current_entry->ether_source_addr[3],
                 current_entry->ether_source_addr[4], current_entry->ether_source_addr[5],
                 (int) current_entry->last_aw_time, current_entry->curr_seq_num,
                 ((USE_PRECURSOR_LIST == true) ? current_entry->best_output_iface->nht->best_next_hop->sw->size : batman_db_inht_get_totalsncount(current_entry->best_output_iface->nht)),
                 WINDOW_SIZE,
                 current_entry->best_output_iface->iface_num,
                 current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[0], current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[1],
                 current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[2], current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[3],
                 current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[4], current_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr[5]);
        strcat(output, entry_str);
        batman_irt_if_entry_t* curr_iface = current_entry->if_entrys;

        while(curr_iface != NULL) {
            if(current_entry->best_output_iface != curr_iface) {
                snprintf(entry_str, REPORT_RT_STR_LEN + 1, "|                   |                 |                 | %7i / %3i | %13i | " MAC " |\n",
                         ((USE_PRECURSOR_LIST == true) ? curr_iface->nht->best_next_hop->sw->size : batman_db_inht_get_totalsncount(curr_iface->nht)),
                         WINDOW_SIZE,
                         curr_iface->iface_num,
                         curr_iface->nht->best_next_hop->ether_nexthop_addr[0], curr_iface->nht->best_next_hop->ether_nexthop_addr[1],
                         curr_iface->nht->best_next_hop->ether_nexthop_addr[2], curr_iface->nht->best_next_hop->ether_nexthop_addr[3],
                         curr_iface->nht->best_next_hop->ether_nexthop_addr[4], curr_iface->nht->best_next_hop->ether_nexthop_addr[5]);
                strcat(output, entry_str);
            }

            curr_iface = curr_iface->hh.next;
        }

        strcat(output, "+-------------------+-----------------+-----------------+---------------+---------------+-------------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return true;
}
