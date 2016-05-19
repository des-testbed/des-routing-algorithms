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

#include "../2hop_neighbor_set/2hop_neighbor_set.h"
#include "../neighbor_set/neighbor_set.h"
#include "route_calculation.h"
#include "../../config.h"
#include "../routing_table/routing_table.h"
#include "../topology_set/topology_set.h"
#include <utlist.h>


// ------------------ MPR -----------------------------------------------------

typedef struct olsr_db_rc_1hn {
    uint8_t		ether_main_addr[ETH_ALEN];
    uint8_t 		willingness;
    uint64_t		willing_koeff;
    uint8_t		link_quality;
    UT_hash_handle	hh;
} olsr_db_rc_1hn_t;

olsr_db_rc_1hn_t* get_1hnwset() {
    olsr_2hns_neighbor_t* _1hnset = olsr_db_2hns_get1hnset();
    olsr_db_rc_1hn_t* 	_1hnwset = NULL;
    uint8_t is_MPR, is_MRP_SEL, will;

    while(_1hnset != NULL) {
        olsr_2hns_neighbor_t* _1hn = _1hnset;

        if(olsr_db_ns_getneigh(_1hn->ether_addr, &is_MPR, &is_MRP_SEL, &will) == true) {
            olsr_db_rc_1hn_t* _1hnw = malloc(sizeof(olsr_db_rc_1hn_t));

            if(_1hnw != NULL) {
                memcpy(_1hnw->ether_main_addr, _1hn->ether_addr, ETH_ALEN);
                _1hnw->willingness = will;
                _1hnw->link_quality = olsr_db_ns_getlinkquality(_1hn->ether_addr);
                HASH_ADD_KEYPTR(hh, _1hnwset, _1hnw->ether_main_addr, ETH_ALEN, _1hnw);
            }
        }

        HASH_DEL(_1hnset, _1hn);
        free(_1hn);
    }

    return _1hnwset;
}

void select_as_mpr(uint8_t mpr_ether_addr[ETH_ALEN], olsr_db_rc_1hn_t** _1hop_wneighbors, olsr_2hns_neighbor_t** _2hop_neighbors) {
    olsr_db_ns_setneigh_mprstatus(mpr_ether_addr, true);
    uint8_t _1hop_quality = olsr_db_ns_getlinkquality(mpr_ether_addr);
    olsr_2hns_neighbor_t* _2hn = olsr_db_2hns_get2hneighbors(mpr_ether_addr);

    while(_2hn != NULL) {
        olsr_2hns_neighbor_t* r_2hn;
        HASH_FIND(hh, *_2hop_neighbors, _2hn->ether_addr, ETH_ALEN, r_2hn);
        uint8_t _2hop_link_quality = _1hop_quality * _2hn->link_quality / 100;

        if(r_2hn != NULL && _2hop_link_quality >= MPR_QUALITY_THRESHOLD) {
            HASH_DEL(*_2hop_neighbors, r_2hn);
            free(r_2hn);
        }

        _2hn = _2hn->hh.next;
    }

    olsr_db_rc_1hn_t* _1hwn;
    HASH_FIND(hh, *_1hop_wneighbors, mpr_ether_addr, ETH_ALEN, _1hwn);

    if(_1hwn != NULL) {
        HASH_DEL(*_1hop_wneighbors, _1hwn);
        free(_1hwn);
    }
}

void set_willing_koeff(olsr_db_rc_1hn_t* _1hwn, olsr_2hns_neighbor_t* unreached_2hopset) {
    olsr_2hns_neighbor_t* _2hop_neihbors = olsr_db_2hns_get2hneighbors(_1hwn->ether_main_addr);
    // total number of 2hop neighbors reached over this 1hop neighbor
    // sum of link qualitys of unreached 2hop neighbors
    uint64_t u_qsum = 0;
    // sum of_link qualitys of reached 2hop neighbors
    uint64_t qsum = 0;

    while(_2hop_neihbors != NULL) {
        uint8_t link_quality = olsr_db_2hns_getlinkquality(_1hwn->ether_main_addr, _2hop_neihbors->ether_addr);
        olsr_2hns_neighbor_t* unreached_2hop;
        HASH_FIND(hh, unreached_2hopset, _2hop_neihbors->ether_addr, ETH_ALEN, unreached_2hop);

        if(unreached_2hop != NULL) {
            u_qsum += link_quality;
        }
        else {
            qsum += link_quality;
        }

        _2hop_neihbors = _2hop_neihbors->hh.next;
    }

    _1hwn->willing_koeff = _1hwn->willingness * (u_qsum * 3 + qsum);
}

void olsr_db_rc_chose_mprset() {
    olsr_db_ns_removeallmprs();
    olsr_2hns_neighbor_t* _2hop_neighbors = olsr_db_2hns_get2hnset();
    olsr_db_rc_1hn_t* _1hop_wneighbors = get_1hnwset();

    while(_2hop_neighbors != NULL && _1hop_wneighbors != NULL) {
        olsr_db_rc_1hn_t* _1hwn = _1hop_wneighbors;
        olsr_db_rc_1hn_t* best_kandidate = _1hwn;

        while(_1hwn != NULL) {
            set_willing_koeff(_1hwn, _2hop_neighbors);

            if(_1hwn->willing_koeff > best_kandidate->willing_koeff) {
                best_kandidate = _1hwn;
            }

            _1hwn = _1hwn->hh.next;
        }

        if(best_kandidate != NULL) {
            select_as_mpr(best_kandidate->ether_main_addr, &_1hop_wneighbors, &_2hop_neighbors);
        }
        else {
            olsr_2hns_neighbor_t* _2hn = _2hop_neighbors;
            HASH_DEL(_2hop_neighbors, _2hn);
            free(_2hn);
        }
    }

    // clear rest of copied 2hop neighbors
    while(_2hop_neighbors != NULL) {
        olsr_2hns_neighbor_t* curr_el = _2hop_neighbors;
        HASH_DEL(_2hop_neighbors, curr_el);
        free(curr_el);
    }

    // clear rest of copied 1hop neighbors
    while(_1hop_wneighbors != NULL) {
        if(_1hop_wneighbors->willingness >= WILL_ALLWAYS) {
            olsr_db_ns_setneigh_mprstatus(_1hop_wneighbors->ether_main_addr, true);
        }

        olsr_db_rc_1hn_t* curr_el = _1hop_wneighbors;
        HASH_DEL(_1hop_wneighbors, curr_el);
        free(curr_el);
    }
}


// --------------- ROUTING TABLE ----------------------------------------

typedef struct rt_el {
    uint8_t		ether_addr[ETH_ALEN];
    uint8_t		hop_count;
    uint8_t		precursor_addr[ETH_ALEN];
    float			quality; // if PDR or probabilistic ETX :0 - no link, 100 - full link
    // if additive ETX : 1 - full link, 65k - no link
    struct rt_el*	prev, *next;
} rt_el_t;

rt_el_t* candidate_hosts;

rt_el_t* create_rtel(uint8_t ether_addr[ETH_ALEN], uint8_t precursor_addr[ETH_ALEN], uint8_t hop_count, float quality) {
    rt_el_t* entry = malloc(sizeof(rt_el_t));

    if(entry == NULL) {
        return NULL;
    }

    memcpy(entry->ether_addr, ether_addr, ETH_ALEN);
    memcpy(entry->precursor_addr, precursor_addr, ETH_ALEN);
    entry->hop_count = hop_count;
    entry->quality = quality;
    return entry;
}


int compare_candidates_plr(rt_el_t* hostA, rt_el_t* hostB) {
    if(hostA->quality > hostB->quality) {
        return -1;
    }

    if(hostA->quality < hostB->quality) {
        return 1;
    }

    if(hostA->hop_count > hostB->hop_count) {
        return 1;
    }

    if(hostA->hop_count < hostB->hop_count) {
        return -1;
    }

    return 0;
}

int compare_candidates_hc(rt_el_t* hostA, rt_el_t* hostB) {
    if(hostA->hop_count > hostB->hop_count) {
        return 1;
    }

    if(hostA->hop_count < hostB->hop_count) {
        return -1;
    }

    return 0;
}

int compare_candidates_etx_additive(rt_el_t* hostA, rt_el_t* hostB) {
    if(hostA->quality > hostB->quality) {
        return 1;
    }

    if(hostA->quality < hostB->quality) {
        return -1;
    }

    return 0;
}
float calculate_etx(uint8_t link_quality) {
    if(link_quality == 0) {
        return 100;
    }

    float x = 100;
    x = x / link_quality;
    return x;
}

void olsr_db_rc_dijkstra() {
    // initialize
    candidate_hosts = NULL;
    olsr_db_ns_tuple_t* source_neighbors = olsr_db_ns_getneighset();

    // initialize candidate set
    while(source_neighbors != NULL) {
        float link_quality = source_neighbors->best_link.quality;

        if(rc_metric == RC_METRIC_ETX_ADD) {
            link_quality = calculate_etx(link_quality);
        }

        rt_el_t* candidate = create_rtel(source_neighbors->neighbor_main_addr, dessert_l25_defsrc,
                                         1, link_quality);

        if(candidate != NULL) {
            DL_APPEND(candidate_hosts, candidate);
        }

        source_neighbors = source_neighbors->hh.next;
    }

    // create Dijkstra graph
    while(candidate_hosts != NULL) {
        if(rc_metric == RC_METRIC_PLR || rc_metric == RC_METRIC_ETX) {
            DL_SORT(candidate_hosts, compare_candidates_plr);
        }
        else if(rc_metric == RC_METRIC_HC) {
            DL_SORT(candidate_hosts, compare_candidates_hc);
        }
        else {
            DL_SORT(candidate_hosts, compare_candidates_etx_additive);
        }

        rt_el_t* best_candidate = candidate_hosts;

        // get hext hop towards best_candidate
        uint8_t next_hop[ETH_ALEN];

        if(memcmp(best_candidate->precursor_addr, dessert_l25_defsrc, ETH_ALEN) == 0) {
            memcpy(next_hop, best_candidate->ether_addr, ETH_ALEN);
        }
        else {
            olsr_db_rt_getnexthop(best_candidate->precursor_addr, next_hop);
        }

        // capture_route;
        olsr_db_rt_addroute(best_candidate->ether_addr, next_hop, best_candidate->precursor_addr, best_candidate->hop_count, best_candidate->quality);

        //add neighbors of best_candidate to candidates
        olsr_db_tc_tcsentry_t* bc_neighbors = olsr_db_tc_getneighbors(best_candidate->ether_addr);

        while(bc_neighbors != NULL) {
            if((olsr_db_rt_getnexthop(bc_neighbors->neighbor_main_addr, next_hop) != true) &&
               (memcmp(bc_neighbors->neighbor_main_addr, dessert_l25_defsrc, ETH_ALEN) != 0)) {

                // if PLR or probabilistic path ETX metric:
                float total_link_quality = (best_candidate->quality * bc_neighbors->link_quality) / 100;

                // if additive ETX metric
                if(rc_metric == RC_METRIC_ETX_ADD) {
                    total_link_quality = best_candidate->quality + calculate_etx(bc_neighbors->link_quality);
                }

                rt_el_t* candidate = create_rtel(bc_neighbors->neighbor_main_addr, best_candidate->ether_addr, best_candidate->hop_count + 1, total_link_quality);

                if(candidate != NULL) {
                    DL_APPEND(candidate_hosts, candidate);
                }
            }

            bc_neighbors = bc_neighbors->hh.next;
        }

        // remove all candidates with ether_addr of this candidate
        rt_el_t* el = candidate_hosts;
        uint8_t best_cand_addr[ETH_ALEN];
        memcpy(best_cand_addr, best_candidate->ether_addr, ETH_ALEN);

        while(el != NULL) {
            rt_el_t* remove_el = el;
            el = el->next;

            if(memcmp(remove_el->ether_addr, best_cand_addr, ETH_ALEN) == 0) {
                DL_DELETE(candidate_hosts, remove_el);
                free(remove_el);
            }
        }
    }
}
