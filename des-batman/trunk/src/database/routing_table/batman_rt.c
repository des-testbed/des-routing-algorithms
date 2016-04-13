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
#include <utlist.h>
#include <dessert.h>
#include "../../config.h"
#include "../../helper.h"
#include "batman_rt.h"
#include "batman_sw.h"
#include "../timeslot.h"

#define REPORT_RT_STR_LEN 200

/** Entry of NextHop-List table */
typedef struct batman_rt_nht_entry {
    /** MAC address of next hop towards destination as string */
    uint8_t ether_nexthop_addr[ETH_ALEN]; // key value
    /**
     * Sliding Window of this connection towards destination.
     * Indicates quality of connection.
     */
    batman_sw_t* sw;
    UT_hash_handle hh;
} batman_rt_nht_entry_t;

/** Output interface entry.
 * To one entry of routing table belongs one or more if_entrys.
 */
typedef struct batman_rt_if_entry {
    /** pointer to interface over that the OGM received */
    dessert_meshif_t* ether_iface;	// key value
    /** Next-Hops towards destination */
    batman_rt_nht_entry_t* nh_entrys;
    /** the best Next-Hop entry with most SeqNum count towards destination */
    batman_rt_nht_entry_t* best_next_hop;
    UT_hash_handle 			hh;
} batman_rt_if_entry_t;

/** Row entry of B.A.T.M.A.N routing table */
typedef struct batman_rt_entry {
    /** MAC address of destination */
    uint8_t 				ether_dest_addr[ETH_ALEN];	// key value
    /** pointer to first HASH_MAP iface entry */
    batman_rt_if_entry_t*	if_entrys;
    /** Last aware time of destination */
    time_t 					last_aw_time;
    /** most actual sequence number for destination */
    uint16_t 				curr_seq_num;
    // /* HNA list
    // /* Gateway capabilities
    /** the best output interface for packets towards destination */
    batman_rt_if_entry_t*	best_output_iface;
    UT_hash_handle 			hh;
} batman_rt_entry_t;

/**
 * B.A.T.M.A.N routig table
 */
struct batman_rt {
    /** pointer to first HASH_MAP entry */
    batman_rt_entry_t* 	entrys;
    /** time-slot to manage (pudge) old entrys */
    timeslot_t*			ts;
} rt;

/** create output interface entry. */
int batman_db_rt_if_entry_create(batman_rt_if_entry_t** rt_if_entry_out, dessert_meshif_t* local_iface) {
    batman_rt_if_entry_t* new_entry;

    new_entry = malloc(sizeof(batman_rt_if_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    new_entry->ether_iface = local_iface;
    new_entry->nh_entrys = NULL;
    *rt_if_entry_out = new_entry;
    return true;
}

int batman_db_nht_entry_create(batman_rt_nht_entry_t** rt_nh_entry_out, uint8_t ether_addr[ETH_ALEN]) {
    batman_rt_nht_entry_t* new_entry;
    batman_sw_t* new_sw;
    new_entry = malloc(sizeof(batman_rt_nht_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    if(batman_sw_create(&new_sw, window_size) == false) {
        free(new_entry);
        return false;
    };

    memcpy(new_entry->ether_nexthop_addr, ether_addr, ETH_ALEN);

    new_entry->sw = new_sw;

    *rt_nh_entry_out = new_entry;

    return true;
}

/** destroy output interface entry */
int batman_db_rt_if_entry_destroy(batman_rt_if_entry_t* rt_if_entry) {
    while(rt_if_entry->nh_entrys != NULL) {
        batman_rt_nht_entry_t* nht_entry = rt_if_entry->nh_entrys;
        batman_sw_destroy(nht_entry->sw);
        HASH_DEL(rt_if_entry->nh_entrys, nht_entry);
        free(nht_entry);
    }

    free(rt_if_entry);
    return true;
}

int batman_db_nht_entry_destroy(batman_rt_nht_entry_t* rt_nh_entry) {
    batman_sw_destroy(rt_nh_entry->sw);
    free(rt_nh_entry);
    return true;
}

int batman_db_rt_entry_create(batman_rt_entry_t** rt_entry,
                              uint8_t ether_dest_addr[ETH_ALEN], time_t timestamp,
                              batman_rt_if_entry_t* rt_if_entry, uint16_t seq_num) {
    batman_rt_entry_t* new_entry;
    new_entry = malloc(sizeof(batman_rt_entry_t));

    if(new_entry == NULL) {
        return false;
    }

    memcpy(new_entry->ether_dest_addr, ether_dest_addr, ETH_ALEN);
    new_entry->if_entrys = NULL;
    HASH_ADD_KEYPTR(hh, new_entry->if_entrys, &rt_if_entry->ether_iface, sizeof(int), rt_if_entry);
    new_entry->best_output_iface = rt_if_entry;
    new_entry->curr_seq_num = seq_num;
    new_entry->last_aw_time = timestamp;
    *rt_entry = new_entry;
    return true;
}

int batman_db_rt_entry_destroy(batman_rt_entry_t* rt_entry) {
    batman_rt_if_entry_t* curr_if_entry;

    while(rt_entry->if_entrys != NULL) {
        curr_if_entry = rt_entry->if_entrys;
        HASH_DEL(rt_entry->if_entrys, curr_if_entry);
        batman_db_rt_if_entry_destroy(curr_if_entry);
    }

    free(rt_entry);
    return true;
}

int batman_db_nht_shiftuptoseq(batman_rt_if_entry_t* rt_iface, uint16_t seq_num) {
    batman_rt_nht_entry_t* nh_entry = rt_iface->nh_entrys;
    rt_iface->best_next_hop = nh_entry; // reset best next hop

    while(nh_entry != NULL) {
        batman_sw_dropsn(nh_entry->sw, seq_num); // shift window by all entrys to seq_num

        if(rt_iface->best_next_hop->sw->size + WINDOW_SWITCH_DIFF <= nh_entry->sw->size) {
            rt_iface->best_next_hop = nh_entry;
        }

        batman_rt_nht_entry_t* prev_element = nh_entry;
        nh_entry = nh_entry->hh.next;

        if(prev_element->sw->size == 0) {
            // previous element is empty -> delete
            HASH_DEL(rt_iface->nh_entrys, prev_element);

            if(rt_iface->best_next_hop == prev_element) {
                rt_iface->best_next_hop = rt_iface->nh_entrys;
            }

            batman_db_nht_entry_destroy(prev_element);
        }
    }

    return true;
}

int batman_db_nht_addseq(batman_rt_if_entry_t* iface_entry,
                         uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num) {
    batman_rt_nht_entry_t* nh_entry;

    // Find entry with ether_nexthop_addr
    HASH_FIND(hh, iface_entry->nh_entrys, ether_nexthop_addr, ETH_ALEN, nh_entry);

    // if no entry -> create!
    if(nh_entry == NULL) {
        if(batman_db_nht_entry_create(&nh_entry, ether_nexthop_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, iface_entry->nh_entrys, nh_entry->ether_nexthop_addr,
                        ETH_ALEN, nh_entry);
    }

    // add sequence number to entry sliding window
    batman_sw_addsn(nh_entry->sw, seq_num);

    // drop all out-of-range seq_num from other entry
    // AND
    // set the BEST NEXT HOP to entry with most count of seq numbers
    batman_db_nht_shiftuptoseq(iface_entry, seq_num);
    return true;
}

int batman_db_rt_addroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t* local_iface, time_t timestamp, uint8_t ether_nexthop_addr[ETH_ALEN], uint16_t seq_num) {
    batman_rt_entry_t* rt_entry;
    // First find appropriate routing table entry
    HASH_FIND(hh, rt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    // if not exist -> create!
    if(rt_entry == NULL) {
        batman_rt_if_entry_t* new_if_entry;

        if(batman_db_rt_if_entry_create(&new_if_entry, local_iface) == false) {
            dessert_crit("could not allocate memory");
            return false;
        }

        if(batman_db_rt_entry_create(&rt_entry, ether_dest_addr, timestamp, new_if_entry, seq_num - 1) == false) {
            dessert_crit("could not allocate memory");
            batman_db_rt_if_entry_destroy(new_if_entry);
            return false;
        }

        HASH_ADD_KEYPTR(hh, rt.entrys, rt_entry->ether_dest_addr, ETH_ALEN, rt_entry);
        dessert_debug("--- " MAC " - add route", EXPLODE_ARRAY6(ether_dest_addr));
    }
    else {
        // if exist, but contains no if_entry with ether_iface_addr -> create if_entry
        batman_rt_if_entry_t* new_if_entry;
        HASH_FIND(hh, rt_entry->if_entrys, &local_iface, sizeof(int), new_if_entry);

        if(new_if_entry == NULL) {
            if(batman_db_rt_if_entry_create(&new_if_entry, local_iface) == false) {
                dessert_crit("could not allocate memory");
                return false;
            }

            HASH_ADD_KEYPTR(hh, rt_entry->if_entrys, &new_if_entry->ether_iface, sizeof(int), new_if_entry);
        }
    }

    /*uint8_t* last_next_hop_addr = (rt_entry->best_output_iface->nht->best_next_hop == NULL)?
    	NULL : rt_entry->best_output_iface->nht->best_next_hop->ether_nexthop_addr; // only for debugging*/
    // actualize the BEST NEXT HOP towards destination and BEST_OUTPUT_IFACE for rt_entry
    batman_rt_if_entry_t* curr_iface = rt_entry->if_entrys;

    while(curr_iface != NULL) {
        if(curr_iface->ether_iface == local_iface) {
            // actualize BEST NEXT HOP towards destination for given local_interface
            if(batman_db_nht_addseq(curr_iface, ether_nexthop_addr, seq_num) == false) {
                dessert_crit("could not allocate memory");
                return false;
            }
        }
        else {
            // shift sliding window for all other interfaces
            batman_db_nht_shiftuptoseq(curr_iface, seq_num);
        }

        // actualize BEST OUTPUT IFACE
        if(curr_iface->best_next_hop != NULL &&
           (rt_entry->best_output_iface->best_next_hop->sw->size + WINDOW_SWITCH_DIFF
            <= curr_iface->best_next_hop->sw->size)) {
            rt_entry->best_output_iface = curr_iface;
        }

        batman_rt_if_entry_t* prev_iface = curr_iface;
        curr_iface = curr_iface->hh.next;

        // drop iface entry if no best next hop more for this entry known
        if(prev_iface->best_next_hop == NULL) {
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
    timeslot_addobject(rt.ts, timestamp, rt_entry);
    return true;
}

int batman_db_rt_getbestroute(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** local_iface, uint8_t ether_nexthop_addr_out[ETH_ALEN]) {
    batman_rt_entry_t* rt_entry;

    // find appropriate routing table entry
    HASH_FIND(hh, rt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return false;
    }

    *local_iface = rt_entry->best_output_iface->ether_iface;
    memcpy(ether_nexthop_addr_out, rt_entry->best_output_iface->best_next_hop->ether_nexthop_addr, ETH_ALEN);
    return true;
}

void _add_myinterfaces_to_precursors(uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN], uint8_t* precursors_iface_count) {
    dessert_meshif_t* my_ifaces = dessert_meshiflist_get();

    while(my_ifaces != NULL) {
        if((*precursors_iface_count) < OGM_PREC_LIST_SIZE) {
            // increment size and write my default address to end of list
            uint8_t* el_start_address = precursors_iface_list + ETH_ALEN * (*precursors_iface_count);
            memcpy(el_start_address, my_ifaces->hwaddr, ETH_ALEN);
            (*precursors_iface_count)++;
        }
        else {   // precursor list is full
            // delete first element and move all elements to begin of the list.
            // add my default address to end of list.
            if(OGM_PREC_LIST_SIZE > 1) {
                memmove(precursors_iface_list, precursors_iface_list + ETH_ALEN, ETH_ALEN *(OGM_PREC_LIST_SIZE - 1));
            }

            memcpy(precursors_iface_list + (OGM_PREC_LIST_SIZE - 1)*ETH_ALEN, my_ifaces->hwaddr, ETH_ALEN);
        }

        my_ifaces = my_ifaces->next;
    }
}

int _check_precursors_list(uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
                           uint8_t* precursors_iface_count, uint8_t iface_addr[ETH_ALEN]) {
    int i;

    for(i = (*precursors_iface_count) - 1; i >= 0; i--) {
        // start to search from !END! It is very presumably that i am the last entry in the list
        uint8_t* el_start_address = precursors_iface_list + ETH_ALEN * i;

        if(memcmp(el_start_address, iface_addr, ETH_ALEN) == 0) {
            return true;
        }
    }

    return false;
}

typedef struct route_entry {
    uint8_t             next_hop[ETH_ALEN];
    dessert_meshif_t*   out_iface;
    uint8_t             quality;
    struct route_entry* next, *prev;
} route_entry_t;

int compare_routing_entrys(route_entry_t* e1, route_entry_t* e2) {
    if(e1->quality > e2->quality) {
        return -1;
    }

    if(e1->quality < e2->quality) {
        return 1;
    }

    return 0;
}

int batman_db_rt_getbestroute_arl(uint8_t ether_dest_addr[ETH_ALEN], dessert_meshif_t** ether_iface_out, uint8_t ether_nexthop_addr_out[ETH_ALEN], uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN], uint8_t* presursosr_iface_count) {
    batman_rt_entry_t* rt_entry;

    // find appropriate routing table entry
    HASH_FIND(hh, rt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return false;
    }

    // first check if best route not contained in precursors list
    if(_check_precursors_list(precursors_iface_list, presursosr_iface_count,
                              rt_entry->best_output_iface->best_next_hop->ether_nexthop_addr) == false) {
        *ether_iface_out = rt_entry->best_output_iface->ether_iface;
        memcpy(ether_nexthop_addr_out, rt_entry->best_output_iface->best_next_hop->ether_nexthop_addr, ETH_ALEN);
        _add_myinterfaces_to_precursors(precursors_iface_list, presursosr_iface_count);
        return true;
    }

    // otherwise find another next hop
    dessert_debug("route looping detected! trying to avoid ... ");

    route_entry_t* re_list = NULL;
    batman_rt_if_entry_t* if_entry = rt_entry->if_entrys;

    while(if_entry != NULL) {
        batman_rt_nht_entry_t* nht_entry = if_entry->nh_entrys;

        while(nht_entry != NULL) {
            route_entry_t* re = malloc(sizeof(route_entry_t));

            if(re != NULL) {
                memcpy(re->next_hop, nht_entry->ether_nexthop_addr, ETH_ALEN);
                re->out_iface = if_entry->ether_iface;
                re->quality = nht_entry->sw->size;
                DL_APPEND(re_list, re);
            }

            nht_entry = nht_entry->hh.next;
        }

        if_entry = if_entry->hh.next;
    }

    int found = false;
    DL_SORT(re_list, compare_routing_entrys);
    route_entry_t* re = re_list;

    while(re != NULL) {
        route_entry_t* current_entry = re;
        re = re->next;

        if(found == false && _check_precursors_list(precursors_iface_list, presursosr_iface_count,
                current_entry->next_hop) == false) {
            // set next hop and output_iface towards destination
            *ether_iface_out = current_entry->out_iface;
            memcpy(ether_nexthop_addr_out, current_entry->next_hop, ETH_ALEN);
            found = true;
            // add myself to precursors
            _add_myinterfaces_to_precursors(precursors_iface_list, presursosr_iface_count);
        }

        DL_DELETE(re_list, current_entry);
        free(current_entry);
    }

    if(found == true) {
        dessert_debug("... DONE");
    }
    else {
        dessert_debug("... FAILED");
    }

    return found;
}

int batman_db_rt_getroutesn(uint8_t ether_dest_addr[ETH_ALEN]) {
    batman_rt_entry_t* rt_entry;
    // find appropriate routing table entry
    HASH_FIND(hh, rt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return -1;
    }

    return rt_entry->curr_seq_num;
}

int batman_db_rt_cleanup() {
    return timeslot_purgeobjects(rt.ts, time(0));
}

int batman_db_rt_change_pt(time_t pudge_timeout) {
    timeslot_change_pt(rt.ts, pudge_timeout);
    return true;
}

/** routing entry element by timeout */
void pudge_old_destination(time_t lasw_aw_time, void* entry) {
    batman_rt_entry_t* rt_entry = entry;
    dessert_debug("--- " MAC " - timeout", EXPLODE_ARRAY6(rt_entry->ether_dest_addr));
    HASH_DEL(rt.entrys, rt_entry);
    batman_db_rt_entry_destroy(rt_entry);
}

int batman_db_rt_deleteroute(uint8_t ether_dest_addr[ETH_ALEN]) {
    batman_rt_entry_t* rt_entry;
    // find appropriate routing table entry
    HASH_FIND(hh, rt.entrys, ether_dest_addr, ETH_ALEN, rt_entry);

    if(rt_entry == NULL) {
        // entry not found
        return false;
    }

    if(timeslot_deleteobject(rt.ts, rt_entry) == true) {
        HASH_DEL(rt.entrys, rt_entry);
        batman_db_rt_entry_destroy(rt_entry);
        return true;
    }

    return false;
}

void batman_db_rt_change_window_size(uint8_t window_size) {
    batman_rt_entry_t* rt_entry = rt.entrys;
    batman_rt_if_entry_t* if_entry;
    batman_rt_nht_entry_t* nht_entry;

    // iterate over entire Routing Table and change window size in all sliding windows
    while(rt_entry != NULL) {
        if_entry = rt_entry->if_entrys;

        while(if_entry != NULL) {
            nht_entry = if_entry->nh_entrys;

            while(nht_entry != NULL) {
                batman_sw_chage_size(nht_entry->sw, window_size);
                nht_entry = nht_entry->hh.next;
            }

            if_entry = if_entry->hh.next;
        }

        if_entry = if_entry->hh.next;
    }
}

int batman_db_rt_init() {
    rt.entrys = NULL;
    timeslot_t* ts;
    timeslot_create(&ts, PURGE_TIMEOUT, pudge_old_destination);
    rt.ts = ts;
    return true;
}

int batman_db_nht_get_totalsncount(batman_rt_if_entry_t* rt_iface) {
    int count = 0;
    batman_rt_nht_entry_t* curr_entry = rt_iface->nh_entrys;

    while(curr_entry != NULL) {
        count += curr_entry->sw->size;
        curr_entry = curr_entry->hh.next;
    }

    return count;
}

// ------------------- reporting -----------------------------------------------

int batman_db_rt_report(char** str_out) {
    batman_rt_entry_t* current_entry = rt.entrys;
    char* output;
    char entry_str[REPORT_RT_STR_LEN  + 1];

    // compute str length
    uint len = 0;

    while(current_entry != NULL) {
        len += REPORT_RT_STR_LEN * (HASH_COUNT(current_entry->if_entrys) + 1);
        current_entry = current_entry->hh.next;
    }

    current_entry = rt.entrys;
    output = malloc(sizeof(char) * REPORT_RT_STR_LEN * (3 + len) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+-------------------+-------------------+-----------------+-----------------+---------------+\n");
    strcat(output, "|    destination    |      next hop     |  out iface addr   | last aware time | current seq_num | seq_num count |\n");
    strcat(output, "+-------------------+-------------------+-------------------+-----------------+-----------------+---------------+\n");

    while(current_entry != NULL) {		// first line for best output interface
        snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| " MAC " | " MAC " | " MAC " | %15i | %15i | %7i / %3i |\n",
            EXPLODE_ARRAY6(current_entry->ether_dest_addr),
            EXPLODE_ARRAY6(current_entry->best_output_iface->best_next_hop->ether_nexthop_addr),
            EXPLODE_ARRAY6(current_entry->best_output_iface->ether_iface->hwaddr),
            (int) current_entry->last_aw_time,
            current_entry->curr_seq_num,
            current_entry->best_output_iface->best_next_hop->sw->size,
            current_entry->best_output_iface->best_next_hop->sw->window_size);
        strcat(output, entry_str);
        batman_rt_if_entry_t* curr_iface = current_entry->if_entrys;

        while(curr_iface != NULL) {
            if(current_entry->best_output_iface != curr_iface) {
                snprintf(entry_str, REPORT_RT_STR_LEN + 1, "| " MAC " | " MAC " | | | | %7i / %3i |\n",
                    EXPLODE_ARRAY6(curr_iface->best_next_hop->ether_nexthop_addr),
                    EXPLODE_ARRAY6(curr_iface->ether_iface->hwaddr),
                    curr_iface->best_next_hop->sw->size,
                    curr_iface->best_next_hop->sw->window_size);
                strcat(output, entry_str);
            }

            curr_iface = curr_iface->hh.next;
        }

        strcat(output, "+-------------------+-----------------+-----------------+---------------+-------------------+-------------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return true;
}
