/******************************************************************************
 Copyright 2009, Alexander Ende, Freie Universitaet Berlin
 (FUB).
 All rights reserved.
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
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
        http://www.des-testbed.net/
*******************************************************************************/

#include "gossiping.h"
#include "gossiping_nhdp.h"


/* These are all the tasks that might be scheduled. */
dessert_periodic_t* nhdp_hello_task                     = NULL;
dessert_periodic_t* nhdp_cleanup_expired_task           = NULL;
dessert_periodic_t* log_mpr_selectors_task              = NULL;
dessert_periodic_t* log_n2_set_task                     = NULL;
dessert_periodic_t* pdr_started_task                    = NULL;
dessert_periodic_t* reselect_mprs_task             = NULL;

bool            pdr_calculation_started         = false;
bool            n2_logging                      = true;
bool            mpr_selectors_logging           = true;
uint8_t         hi_increased_counter            = 0;
double          mpr_minpdr                      = 0.4;

/* timeval initializations */
struct timeval NHDP_HELLO_INTERVAL              = {2, 0};
struct timeval NHDP_HELLO_INTERVAL_OLD          = {0, 0}; 
struct timeval H_HOLD_TIME                      = {8, 0};
struct timeval L_HOLD_TIME                      = {8, 0};
struct timeval N_HOLD_TIME                      = {8, 0};
struct timeval NHDP_CLEANUP_INTERVAL            = {2, 0};
struct timeval MPR_RESELECT_INTERVAL            = {10,0};
struct timeval PDR_CALC_DELAY                   = {10,0};
struct timeval N2_LOGGING_INTERVAL              = {2,0};
struct timeval MPR_SELECTOR_LOGGING_INTERVAL    = {2,0};
struct timeval DAEMON_START;

pthread_mutex_t     all_sets_lock  = PTHREAD_MUTEX_INITIALIZER;

N_set_t     *n_set  = NULL;     ///< The 1-hop neighborhood set (routerwide)
N2_set_t    *n2_set = NULL;     ///< The 2-hop neighborhood set (interface bound)
L_set_t     *l_set  = NULL;     ///< The link set (interface bound)
NL_set_t    *nl_set = NULL;     ///< The lost neighbor set
PDR_global_trap_t       *pdr_global_trap = NULL; ///< The packet traps for pdr calculation. For each neighbor one trap.

void init_nhdp() {
    // nhdp related cli commandos
    cli_register_command(dessert_cli, dessert_cli_show, "ls", cli_show_nhdp_linkset, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show nhdp linkset");
    cli_register_command(dessert_cli, dessert_cli_show, "ns", cli_show_nhdp_neighborset, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show nhdp neighborset");
    cli_register_command(dessert_cli, dessert_cli_show, "n2s", cli_show_nhdp_n2set, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show nhdp 2-hop neighborset");
    cli_register_command(dessert_cli, dessert_cli_show, "n2ss", cli_show_nhdp_strict_n2set, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show nhdp strict 2-hop neighborset");
    cli_register_command(dessert_cli, dessert_cli_show, "mprsel", cli_show_mprselectors, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show mpr selectors (nodes that selected me as mpr)");
    cli_register_command(dessert_cli, dessert_cli_set, "nhdp_hi", cli_set_nhdp_hi, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set the hello interval for nhdp");
    cli_register_command(dessert_cli, dessert_cli_set, "nhdp_ht", cli_set_nhdp_ht, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set the validity time for hello information for nhdp");
    cli_register_command(dessert_cli, dessert_cli_set, "mpr_ri", cli_set_mpr_ri, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set the interval after which mprs are reselected (0 = event based)");
    cli_register_command(dessert_cli, dessert_cli_set, "mpr_minpdr", cli_set_minpdr, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set the minimum pdr for a link used for mpr selection");
    cli_register_command(dessert_cli, dessert_cli_set, "nhdp_flush", cli_flush_hashes, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "flush all hashes (same as quick restart)");
    NHDP_HELLO_INTERVAL.tv_sec = hello_interval.tv_sec;
    NHDP_HELLO_INTERVAL.tv_usec = hello_interval.tv_usec;
    gettimeofday(&DAEMON_START, NULL);
}

void _print_nset() {
    N_set_t     *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *n_addr, *n_addr_tmp;
    uint8_t counter;
    
    counter = 0;
    dessert_crit("===================================");
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        counter++;
        dessert_crit("%d: ", counter);
        //dessert_crit("  symmetric: %s", n_set_entry->N_symmetric ? "true" : "false");
        //dessert_crit("  mpr      : %s", n_set_entry->N_mpr ? "true" : "false");
        HASH_ITER(hh, n_set_entry->N_neighbor_addr_list, n_addr, n_addr_tmp) {
            dessert_info("  "MAC"", EXPLODE_ARRAY6(n_addr->N_neighbor_addr));
        }
    }
    dessert_crit("===================================");
}

uint8_t _check_expired(struct timeval *timeval) {
    struct timeval current_time;

    gettimeofday(&current_time, NULL);
    return timercmp(&current_time, timeval, >);
}

uint8_t _lset_get_linkstatus(L_set_tuple_t *l_set_tuple) {
    if(l_set_tuple->L_pending == true) {
        return PENDING;
    } else if(l_set_tuple->L_lost == true) {
        return LOST;
    } else if(_check_expired(&l_set_tuple->L_SYM_time) == 0) {
        return SYMMETRIC;
    } else if(_check_expired(&l_set_tuple->L_HEARD_time) == 0) {
        return HEARD;
    }
    return LOST;
}

uint8_t _create_local_iface_addrlist(dessert_meshif_t *this_if, AB_buffer_t **ab_buffer) {
    dessert_meshif_t            *local_ifaces;
    AB_buffer_t                 *ab_buffer_entry;
    uint8_t                     num_entries;
    
    num_entries = 0;
    local_ifaces = dessert_meshiflist_get();
    while(local_ifaces != NULL) {
        ab_buffer_entry = malloc(sizeof(AB_buffer_t));
        memcpy(&ab_buffer_entry->AB_addr, &local_ifaces->hwaddr, ETH_ALEN);
        ab_buffer_entry->AB_flags    = (LOCAL_IF << 4);
        if(memcmp(&local_ifaces->hwaddr, &this_if->hwaddr, ETH_ALEN) == 0) {
            ab_buffer_entry->AB_flags   = (ab_buffer_entry->AB_flags | THIS_IF);
        } else {
            ab_buffer_entry->AB_flags = (ab_buffer_entry->AB_flags | OTHER_IF);
        }
        HASH_ADD(hh, *ab_buffer, AB_addr, ETH_ALEN, ab_buffer_entry);
        num_entries++;
        dessert_debug("    " MAC " FLAGS: %x added to local interface addresslist.", EXPLODE_ARRAY6(ab_buffer_entry->AB_addr), ab_buffer_entry->AB_flags);
        local_ifaces = local_ifaces->next;
    }
    return num_entries;
}

uint8_t _create_linkset_addrlist(dessert_meshif_t *local_iface, AB_buffer_t **ab_buffer) {
    L_set_t                     *l_set_entry;
    L_set_tuple_t               *l_set_tuplelist_item, *l_set_tuplelist_item_tmp;
    uint8_t                     L_status, num_entries;
    AB_buffer_t*                ab_buffer_entry;

    num_entries = 0;
    HASH_FIND(hh, l_set, &local_iface->hwaddr, ETH_ALEN, l_set_entry);
    if(l_set_entry == NULL) {
        dessert_debug("No linkset entries to process for interface "MAC".", EXPLODE_ARRAY6(local_iface->hwaddr));
        return 0;
    }
    HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuplelist_item, l_set_tuplelist_item_tmp) {
        L_status = _lset_get_linkstatus(l_set_tuplelist_item);
        if(L_status != PENDING) {
            ab_buffer_entry = malloc(sizeof(AB_buffer_t));
            memcpy(&ab_buffer_entry->AB_addr, &l_set_tuplelist_item->L_neighbor_iface_addr, ETH_ALEN);
            //ab_buffer_entry->AB_type    = LINK_STATUS;
            //ab_buffer_entry->AB_value   = L_status;
            ab_buffer_entry->AB_flags = (LINK_STATUS << 4);
            ab_buffer_entry->AB_flags = (ab_buffer_entry->AB_flags | L_status);
            HASH_ADD(hh, *ab_buffer, AB_addr, ETH_ALEN, ab_buffer_entry);
            num_entries++;
            dessert_debug("    " MAC " FLAGS: %x added to linkset addresslist.", EXPLODE_ARRAY6(l_set_tuplelist_item->L_neighbor_iface_addr), ab_buffer_entry->AB_flags);
        }
    }
    return num_entries;
}

uint8_t _create_neighbor_addrlist(AB_buffer_t **ab_buffer) {
    N_set_t                     *n_set_entry, *tmp1;
    N_set_addrlist_t            *neighbor_addresslist_item, *tmp2;
    AB_buffer_t                 *new_address_block, *ab_match, *new_mpr_address_block;
    bool                        already_added;
    uint8_t                     num_entries;
    
    num_entries = 0;
    HASH_ITER(hh1, n_set, n_set_entry, tmp1) {
        if(n_set_entry->N_symmetric == true) {
            already_added = false;
            HASH_ITER(hh, n_set_entry->N_neighbor_addr_list, neighbor_addresslist_item, tmp2) {
                new_address_block = malloc(sizeof(AB_buffer_t));
                memcpy(&new_address_block->AB_addr, &neighbor_addresslist_item->N_neighbor_addr, ETH_ALEN);
                HASH_FIND(hh, *ab_buffer, new_address_block, ETH_ALEN, ab_match);
                if(ab_match != NULL) {
                    //if(ab_match->AB_type == LINK_STATUS && ab_match->AB_value == SYMMETRIC) {
                    if((ab_match->AB_flags >> 4) == LINK_STATUS && (ab_match->AB_flags & 0x0F) == SYMMETRIC) {
                        free(new_address_block);
                        /* we already have such an address block. skip creating a new one. */
                        /* BUT DO mpr related stuff before */
                        if(USE_MPRS == true) {
                            if(n_set_entry->N_mpr == true && already_added == false) {
                                already_added = true;
                                new_mpr_address_block = malloc(sizeof(AB_buffer_t));
                                memcpy(&new_mpr_address_block->AB_addr, &neighbor_addresslist_item->N_neighbor_addr, ETH_ALEN);
                                new_mpr_address_block->AB_flags = (MPR << 4);
                                new_mpr_address_block->AB_flags = (new_mpr_address_block->AB_flags | FLOODING);
                                HASH_ADD(hh, *ab_buffer, AB_addr, ETH_ALEN, new_mpr_address_block);
                                num_entries++;
                                dessert_debug("    " MAC " FLAGS: %x added to mpr address list.", EXPLODE_ARRAY6(neighbor_addresslist_item->N_neighbor_addr), new_mpr_address_block->AB_flags);
                            }
                        }
                        continue;
                    }
                }
                new_address_block->AB_flags = (OTHER_NEIGHB << 4);
                new_address_block->AB_flags = (new_address_block->AB_flags | SYMMETRIC);
                HASH_ADD(hh, *ab_buffer, AB_addr, ETH_ALEN, new_address_block);
                num_entries++;
                dessert_debug("    " MAC " FLAGS: %x added to neighbor address list.", EXPLODE_ARRAY6(neighbor_addresslist_item->N_neighbor_addr), new_address_block->AB_flags);
            }
        }
    }
    return num_entries;
}

void _do_neighborset_mpr_updates(AB_buffer_t *ab_buffer, N_set_t **current_n_set_tuple, uint8_t *willingness, bool *reselect_mprs) {
    AB_buffer_t             *ab_buffer_item, *ab_buffer_item_tmp;
    dessert_meshif_t        *local_ifaces;
    uint8_t                 mpr_ab_found;

    /* MPR related updates to the Neighbor Set*/
    mpr_ab_found = false;
    local_ifaces = dessert_meshiflist_get();
    while(local_ifaces != NULL) {
        HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
            if(memcmp(&local_ifaces->hwaddr, &ab_buffer_item->AB_addr, 0)) {
                if((ab_buffer_item->AB_flags >> 4) == MPR) {
                    if((ab_buffer_item->AB_flags & 0x0F) == UNDEFINED || (ab_buffer_item->AB_flags & 0x0F) == ROUTING) {
                        (*current_n_set_tuple)->N_mpr_selector = true;
                        mpr_ab_found = true;
                        break;
                    }
                }
            }
        }
        if(mpr_ab_found == true) {
            break;
        }
        local_ifaces = local_ifaces->next;
    }
    if(mpr_ab_found == false) {
        local_ifaces = dessert_meshiflist_get();
        while(local_ifaces != NULL) {
            HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
                if(memcmp(&local_ifaces->hwaddr, &ab_buffer_item->AB_addr, 0)) {
                    if((ab_buffer_item->AB_flags >> 4) == LINK_STATUS && (ab_buffer_item->AB_flags & 0x0F) == SYMMETRIC) {
                        (*current_n_set_tuple)->N_mpr_selector = false;
                        mpr_ab_found = true;
                        dessert_debug("  "MAC" (1-hop) N_mpr_selector = false", EXPLODE_ARRAY6(ab_buffer_item->AB_addr));
                        break;
                    }
                }
            }
            if(mpr_ab_found == true) {
                break;
            }
            local_ifaces = local_ifaces->next;
        }
    }
    /* OLSRv2 draft 17.6.1 */
    if((*current_n_set_tuple)->N_willingness == WILL_NEVER && (*current_n_set_tuple)->N_symmetric == true && *willingness != WILL_NEVER) {
        *reselect_mprs = true;
    }
    if((*current_n_set_tuple)->N_mpr == true && (*current_n_set_tuple)->N_willingness != WILL_NEVER && *willingness == WILL_NEVER) {
        *reselect_mprs = true;
    }
    if((*current_n_set_tuple)->N_symmetric == true && (*current_n_set_tuple)->N_mpr == false && (*current_n_set_tuple)->N_willingness != WILL_ALWAYS && *willingness == WILL_ALWAYS) {
        *reselect_mprs = true;
    }
    /* OLSRv2 draft 17.6.1 */
    if((*current_n_set_tuple)->N_mpr == false && (*willingness > (*current_n_set_tuple)->N_willingness)) {
        *reselect_mprs = true;
    }
    /* OLSRv2 draft 17.6.2 */
    if((*current_n_set_tuple)->N_mpr == true && (*willingness < (*current_n_set_tuple)->N_willingness)) {
        *reselect_mprs = true;
    }

    /* OLSRv2 draft 17.3.3 */
    if((*current_n_set_tuple)->N_symmetric == false) {
        (*current_n_set_tuple)->N_mpr  = false;
        (*current_n_set_tuple)->N_mpr_selector  = false;
    }
    /* OLSRv2 draft 15.3.2.2 */
    (*current_n_set_tuple)->N_willingness = *willingness;

}

/* NHDP 12.3.3.1 */
/* Addresses in this router's neighbor address list, that are not contained in the hello's neighbor address list, 
   are added to the removed and lost address lists. */
void _check_removed_and_lost_addresses(N_set_t *n_set_matches, N_set_addrlist_t *neighbor_address_list, N_set_addrlist_t *removed_address_list, N_set_addrlist_t *lost_address_list) {
    N_set_t             *n_set_matches_item, *n_set_matches_item_tmp;
    N_set_addrlist_t    *neighbor_address_list_item, *neighbor_address_list_item_tmp, *new_addresslist_item, *address_match;
    
    HASH_ITER(hh2, n_set_matches, n_set_matches_item, n_set_matches_item_tmp) {
        HASH_ITER(hh, n_set_matches->N_neighbor_addr_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
            HASH_FIND(hh, neighbor_address_list, neighbor_address_list_item, ETH_ALEN, address_match);
            if(address_match == NULL) {
                dessert_debug("  "MAC" (1-hop) + removed address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                HASH_ADD(hh, removed_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                dessert_crit("REMOVED: %d", HASH_COUNT(removed_address_list));
                if(n_set_matches->N_symmetric == true) {
                    dessert_debug("    "MAC" (1-hop) + lost address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                    new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                    memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                    HASH_ADD(hh, lost_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                }
            }
        }       
    }
}
void _do_neighborset_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *neighbor_address_list, N_set_addrlist_t **removed_address_list, N_set_addrlist_t **lost_address_list, uint8_t *willingness, bool *reselect_mprs) {
    N_set_t                 *new_n_set_tuple, *new_neighbor_tuple, *n_set_entry, *n_set_entry_tmp, *n_set_matches, *n_set_matches_item, *n_set_matches_item_tmp;
    N_set_addrlist_t        *new_addresslist_item, *neighbor_address_list_item, *neighbor_address_list_item_tmp, *address_match, *n_addrlist_match;
    uint8_t                 n_set_matches_count, addrlist_length;

    n_set_matches           = NULL;     ///< n_set entries with address lists containing addresses matching with the HELLO's Neighbor Address List
    
    /* NHDP 12.3.1 */
    /* Find address matches between local neighbor address list and the neighbor address list of this hello message. 
       If a match is found, it is buffered in n_set_matches. IMPORTANT: Use of different hash handles! */
    HASH_ITER(hh, neighbor_address_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
        dessert_debug("    "MAC" (1-hop) on HELLO's neighbor list. Processing...", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
        HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
            HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, neighbor_address_list_item->N_neighbor_addr, ETH_ALEN, n_addrlist_match);
            if(n_addrlist_match != NULL) {
                addrlist_length = HASH_COUNT(n_set_entry->N_neighbor_addr_list) * sizeof(N_set_addrlist_t);
                /* IMPORTANT: No need to malloc here, because we add an EXISTING tuple to another set.*/
                HASH_ADD(hh2, n_set_matches, N_neighbor_addr_list, addrlist_length, n_set_entry);
            }
        }
    }
    n_set_matches_count = HASH_CNT(hh2, n_set_matches);
    if(n_set_matches_count == 0) {
        /* NHDP 12.3.2 */
        /* No matches were buffered into n_set_matches. That means we dont know about that neighbor yet. Create a new tuple in neighbor set. */
        new_neighbor_tuple = malloc(sizeof(N_set_t));
        new_neighbor_tuple->N_symmetric  = false;
        new_neighbor_tuple->N_neighbor_addr_list = NULL;
        new_neighbor_tuple->N_mpr = false;
        new_neighbor_tuple->N_willingness = WILL_NEVER;
        new_neighbor_tuple->N_mpr_selector = false;
        HASH_ITER(hh, neighbor_address_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
            new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
            memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
            HASH_ADD(hh, new_neighbor_tuple->N_neighbor_addr_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
            //dessert_crit(""MAC" added to local neighbor address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
        }
        addrlist_length = HASH_COUNT(new_neighbor_tuple->N_neighbor_addr_list) * sizeof(N_set_addrlist_t);
        if(USE_MPRS == true) {
            _do_neighborset_mpr_updates(ab_buffer, &new_neighbor_tuple, willingness, reselect_mprs);
        }
        HASH_ADD(hh1, n_set, N_neighbor_addr_list, addrlist_length, new_neighbor_tuple);
        /* constraint check */
        dessert_debug("  "MAC" (1-hop) added as neighbor.", EXPLODE_ARRAY6(new_neighbor_tuple->N_neighbor_addr_list->N_neighbor_addr));
    } else if(n_set_matches_count == 1) {
        /* NHDP 12.3.3.1 */
        HASH_ITER(hh2, n_set_matches, n_set_matches_item, n_set_matches_item_tmp) {
            HASH_ITER(hh, n_set_matches->N_neighbor_addr_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
                HASH_FIND(hh, neighbor_address_list, neighbor_address_list_item, ETH_ALEN, address_match);
                if(address_match == NULL) {
                    dessert_crit("  "MAC" (1-hop) added to removed address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                    new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                    memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                    HASH_ADD(hh, *removed_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                    if(n_set_matches->N_symmetric == true) {
                        dessert_crit("    "MAC" (1-hop) added to lost address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                        new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                        memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                        HASH_ADD(hh, *lost_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                    }
                }
            }       
        }
        //_check_removed_and_lost_addresses(n_set_matches, neighbor_address_list, *removed_address_list, *lost_address_list);
        /* NHDP 12.3.3.2 */
        /* Exactly one match was buffered into n_set_matches. That means this neighbor is already known to us. Update the existing neighbor set tuple. */
        /* Clear existing neighborset tuple's neighbor address list. */
        HASH_ITER(hh, n_set_matches->N_neighbor_addr_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
            HASH_DEL(n_set_matches->N_neighbor_addr_list, neighbor_address_list_item);
            free(neighbor_address_list_item);
        }
        n_set_matches->N_neighbor_addr_list = NULL;
        /* Refill the existing neighborset tuples address list with the addresses from the hello's neighbor address list. */
        HASH_ITER(hh, neighbor_address_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
            new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
            memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
            HASH_ADD(hh, n_set_matches->N_neighbor_addr_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
            dessert_debug("    "MAC" (1-hop) updated.", EXPLODE_ARRAY6(new_addresslist_item->N_neighbor_addr));
        }
        if(USE_MPRS == true) {
            _do_neighborset_mpr_updates(ab_buffer, &n_set_matches, willingness, reselect_mprs);
        }
    } else if(n_set_matches_count >= 2) {
        dessert_crit("!!!!!!!!!!!!!!!!!!!!!!! n_set_matches >= 2 call ignored.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        return;
        /* NHDP 12.3.4.1 */
        HASH_ITER(hh2, n_set_matches, n_set_matches_item, n_set_matches_item_tmp) {
            HASH_ITER(hh, n_set_matches->N_neighbor_addr_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
                HASH_FIND(hh, neighbor_address_list, neighbor_address_list_item, ETH_ALEN, address_match);
                if(address_match == NULL) {
                    dessert_debug("  "MAC" (1-hop) + removed address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                    new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                    memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                    HASH_ADD(hh, *removed_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                    dessert_crit("REMOVED: %d", HASH_COUNT(*removed_address_list));
                    if(n_set_matches->N_symmetric == true) {
                        dessert_debug("    "MAC" (1-hop) + lost address list.", EXPLODE_ARRAY6(neighbor_address_list_item->N_neighbor_addr));
                        new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
                        memcpy(&new_addresslist_item->N_neighbor_addr, &neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
                        HASH_ADD(hh, *lost_address_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
                    }
                }
            }       
        }
        //_check_removed_and_lost_addresses(n_set_matches, neighbor_address_list, removed_address_list, lost_address_list);
        /* NHDP 12.3.4.2 */
        /* Replace the matching neighbor tuples with ONE new neighbor tuple. */
        /* TODO: implement for multi interfaces */
        dessert_crit("NSET MATCHES: 2 ----> THINK ABOUT THAT!!!!");
        dessert_crit("N_SET BEFORE: %d", HASH_CNT(hh1, n_set));
        HASH_ITER(hh2, n_set_matches, n_set_matches_item, n_set_matches_item_tmp) {
            dessert_crit("...deleted n_set_match entry.");
            HASH_DELETE(hh2, n_set_matches, n_set_matches_item);
            free(n_set_matches_item);
        }
        dessert_crit("N_SET AFTER: %d", HASH_CNT(hh1, n_set));
        new_n_set_tuple = malloc(sizeof(N_set_t));
        new_n_set_tuple->N_symmetric = false;
        new_n_set_tuple->N_mpr = false;
        new_n_set_tuple->N_willingness = WILL_NEVER;
        new_n_set_tuple->N_neighbor_addr_list = NULL;
        HASH_ITER(hh, neighbor_address_list, neighbor_address_list_item, neighbor_address_list_item_tmp) {
            new_addresslist_item = malloc(sizeof(N_set_addrlist_t));
            memcpy(&new_addresslist_item->N_neighbor_addr, neighbor_address_list_item->N_neighbor_addr, ETH_ALEN);
            HASH_ADD(hh, new_n_set_tuple->N_neighbor_addr_list, N_neighbor_addr, ETH_ALEN, new_addresslist_item);
            dessert_debug("    " MAC " added to neighborset.", EXPLODE_ARRAY6(new_addresslist_item->N_neighbor_addr));
        }
        if(USE_MPRS == true) {
            _do_neighborset_mpr_updates(ab_buffer, &new_n_set_tuple, willingness, reselect_mprs);
        }
        /* Finally, add the tuple to the neighbor set */
        addrlist_length = HASH_COUNT(new_n_set_tuple->N_neighbor_addr_list) * sizeof(N_set_addrlist_t);
        dessert_crit("ADDING NEIGHBOR: addrlist_length: %d NSET: ", addrlist_length, n_set == NULL ? "NULL" : "OK");
        HASH_ADD(hh1, n_set, N_neighbor_addr_list, addrlist_length, new_n_set_tuple);
        HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
            if(n_set_entry->N_neighbor_addr_list == NULL) {
                dessert_crit("n_set_entry->N_neighbor_addr_list == NULL!!!");
            }
        }
    }
}

void _do_lost_neighborset_updates(N_set_addrlist_t *lost_address_list) {
    N_set_addrlist_t    *lost_address_list_item, *addrlist_tmp;
    NL_set_t            *nl_set_item, *nl_set_match;
    struct timeval      current_time;

    dessert_debug("  Doing lost neighborset updates.");
    HASH_ITER(hh, lost_address_list, lost_address_list_item, addrlist_tmp) {
        HASH_FIND(hh, nl_set, &lost_address_list_item->N_neighbor_addr, ETH_ALEN, nl_set_match);
        if(nl_set_match == NULL) {
            gettimeofday(&current_time, NULL);
            nl_set_item = malloc(sizeof(NL_set_t));
            TIMEVAL_ADD(&nl_set_item->NL_time, N_HOLD_TIME.tv_sec, N_HOLD_TIME.tv_usec);
            HASH_ADD(hh, nl_set, NL_neighbor_addr, ETH_ALEN, nl_set_item);
            dessert_debug("    " MAC " added to lost neighborset.", EXPLODE_ARRAY6(nl_set_item->NL_neighbor_addr));
        }
    }
}

void _do_linkset_mpr_updates(AB_buffer_t *ab_buffer, L_set_tuple_t **current_l_set_tuple) {
    AB_buffer_t         *ab_buffer_item, *ab_buffer_item_tmp;
    dessert_meshif_t    *local_ifaces;
    uint8_t             L_status;
    
    /* MPR related updates */
    if(USE_MPRS == true) {
        dessert_debug("    Doing Linkset MPR updates for..."MAC"", EXPLODE_ARRAY6((*current_l_set_tuple)->L_neighbor_iface_addr));
        /* OLSRv2 draft 12 - 15.3.2.3.1 */
        local_ifaces = dessert_meshiflist_get();
        while(local_ifaces != NULL) {
            HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
                if(memcmp(&local_ifaces->hwaddr, &ab_buffer_item->AB_addr, ETH_ALEN) == 0) {
                    //if(ab_buffer_item->AB_type == MPR) {
                    if((ab_buffer_item->AB_flags >> 4) == MPR) {
                        //if(ab_buffer_item->AB_value == UNDEFINED || ab_buffer_item->AB_value == FLOODING) {
                        if((ab_buffer_item->AB_flags & 0x0F) == UNDEFINED || (ab_buffer_item->AB_flags & 0x0F) == FLOODING) {
                            (*current_l_set_tuple)->L_mpr_selector = true;
                            dessert_debug("      "MAC"  L_mpr_selector = true.", EXPLODE_ARRAY6((*current_l_set_tuple)->L_neighbor_iface_addr));
                            return;
                        }
                    }
                }
            }
            local_ifaces = local_ifaces->next;
        }
        /* OLSRv2 draft 12 - 15.3.2.3.2
         * This part is only executed if there was no address block with type mpr found.
         */
        local_ifaces = dessert_meshiflist_get();
        while(local_ifaces != NULL) {
            HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
                if(memcmp(&local_ifaces->hwaddr, &ab_buffer_item->AB_addr, ETH_ALEN) == 0) {
                    //dessert_debug("      "MAC" %d %d", EXPLODE_ARRAY6(ab_buffer_item->AB_addr), ab_buffer_item->AB_type, ab_buffer_item->AB_value);
                    dessert_debug("      "MAC" FLAGS: %x", EXPLODE_ARRAY6(ab_buffer_item->AB_addr), ab_buffer_item->AB_flags);
                    //if(ab_buffer_item->AB_type == LINK_STATUS && ab_buffer_item->AB_value == SYMMETRIC) {
                    if((ab_buffer_item->AB_flags >> 4) == LINK_STATUS && (ab_buffer_item->AB_flags & 0x0F) == SYMMETRIC) {
                        dessert_debug("      "MAC"  L_mpr_selector = false.", EXPLODE_ARRAY6((*current_l_set_tuple)->L_neighbor_iface_addr));
                        (*current_l_set_tuple)->L_mpr_selector = false;
                        break;
                    }
                }
            }
            local_ifaces = local_ifaces->next;
        }
        L_status = _lset_get_linkstatus(*current_l_set_tuple);
        if(L_status != SYMMETRIC) {
            dessert_debug("      "MAC"  L_mpr_selector = false.", EXPLODE_ARRAY6((*current_l_set_tuple)->L_neighbor_iface_addr));
            (*current_l_set_tuple)->L_mpr_selector = false;
        }
        dessert_debug("    Linkset MPR updates finished.");
    }
}

void _do_linkset_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *removed_address_list, uint8_t *sending_address, dessert_meshif_t *receiving_iface, uint16_t *validity_time, bool *reselect_mprs, float *pdr) {
    N_set_addrlist_t                *removed_address_list_item, *removed_address_list_item_tmp;
    L_set_t                         *l_set_entry, *l_set_entry_tmp, *new_l_set_entry;
    L_set_tuple_t                   *l_set_tuple, *l_set_tuple_match, *new_l_set_tuple;;
    uint8_t                         L_status, L_status_old, add_new;
    struct timeval                  current_time, time1;
    AB_buffer_t                     *ab_buffer_item, *ab_buffer_item_tmp;

    dessert_debug("  Doing linkset updates.");
    /* Remove addresses contained in the Removed Address List from all linkset tuples.*/
    /* 12.5.1 */
    HASH_ITER(hh, removed_address_list, removed_address_list_item, removed_address_list_item_tmp) {
        dessert_crit("Processing "MAC" from removed address list.", EXPLODE_ARRAY6(removed_address_list_item->N_neighbor_addr));
        HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
            HASH_FIND(hh, l_set_entry->l_set_tuple_list, removed_address_list_item, ETH_ALEN, l_set_tuple);
            if(l_set_tuple != NULL) {
                /* 12.5.2 */
                L_status = (uint8_t) _lset_get_linkstatus(l_set_tuple);
                if(L_status == SYMMETRIC) {
                    _link_changed_to_not_symmetric(l_set_tuple, l_set_entry);
                    *reselect_mprs = true;
                }
                dessert_debug("    " MAC " removed from linkset.", EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
                HASH_DEL(l_set_entry->l_set_tuple_list, l_set_tuple);
                free(l_set_tuple);
            }
            if(HASH_COUNT(l_set_entry->l_set_tuple_list) == 0) {
                dessert_debug("    WARNING: l_set_entry " MAC " is empty. Delete?", EXPLODE_ARRAY6(l_set_entry->L_local_iface_addr));
            }
        }
    }
    /* Now update linkset for receiving interface.*/
    /* 12.5.2.1 */
    HASH_FIND(hh, l_set, &receiving_iface->hwaddr, ETH_ALEN, l_set_entry);
    if(l_set_entry != NULL) {
        dessert_debug("    " MAC " (existing):", EXPLODE_ARRAY6(receiving_iface->hwaddr));
        HASH_FIND(hh, l_set_entry->l_set_tuple_list, sending_address, ETH_ALEN, l_set_tuple_match);
    } else {
        dessert_debug("    " MAC " (local iface) created:", EXPLODE_ARRAY6(receiving_iface->hwaddr));
        new_l_set_entry = malloc(sizeof(L_set_t));
        memcpy(&new_l_set_entry->L_local_iface_addr, &receiving_iface->hwaddr, ETH_ALEN);
        new_l_set_entry->l_set_tuple_list = NULL;
        l_set_tuple_match = NULL;
        HASH_ADD(hh, l_set, L_local_iface_addr, ETH_ALEN, new_l_set_entry);
        /* Get the just created l_set_entry for later use */
        HASH_FIND(hh, l_set, &receiving_iface->hwaddr, ETH_ALEN, l_set_entry);
    }
    /* See if there already is an existing tuple for this sending address. */
    if(l_set_tuple_match == NULL) {
        add_new = true;
        gettimeofday(&current_time, NULL);
        new_l_set_tuple = malloc(sizeof(L_set_tuple_t));
        new_l_set_tuple->L_HEARD_time.tv_sec = current_time.tv_sec - 1;
        new_l_set_tuple->L_HEARD_time.tv_usec = current_time.tv_usec;
        new_l_set_tuple->L_SYM_time.tv_sec = current_time.tv_sec - 1;
        new_l_set_tuple->L_SYM_time.tv_usec = current_time.tv_usec;
        memset(&new_l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, 0);
        new_l_set_tuple->L_quality = INITIAL_QUALITY;
        new_l_set_tuple->L_pending = INITIAL_PENDING;
        new_l_set_tuple->L_lost = false;
        new_l_set_tuple->L_time = current_time;
        _timeval_add_ms(&new_l_set_tuple->L_time, validity_time);
        /* MPR related variables */
        new_l_set_tuple->L_mpr_selector = false;
    } else {
        add_new = false;
        new_l_set_tuple = l_set_tuple_match;
        l_set_tuple_match->L_quality = *pdr;
    }
    /* get the linkstatus BEFORE the updates */
    L_status_old = _lset_get_linkstatus(new_l_set_tuple);
    /* 12.5.2.4 */
    HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
        if(memcmp(&receiving_iface->hwaddr, &ab_buffer_item->AB_addr, ETH_ALEN) == 0) {
            //if(ab_buffer_item->AB_type == LINK_STATUS) {
            if((ab_buffer_item->AB_flags >> 4) == LINK_STATUS) {
                //if(ab_buffer_item->AB_value == HEARD || ab_buffer_item->AB_value == SYMMETRIC) {
                if((ab_buffer_item->AB_flags & 0x0F) == HEARD || (ab_buffer_item->AB_flags & 0x0F) == SYMMETRIC) {
                    gettimeofday(&current_time, NULL);
                    new_l_set_tuple->L_SYM_time = current_time;
                    _timeval_add_ms(&new_l_set_tuple->L_SYM_time, validity_time);
                //} else if(ab_buffer_item->AB_value == LOST) {
                } else if((ab_buffer_item->AB_flags & 0x0F) == LOST) {
                    if(_check_expired(&new_l_set_tuple->L_SYM_time) == 0) {
                        gettimeofday(&current_time, NULL);
                        new_l_set_tuple->L_SYM_time.tv_sec = current_time.tv_sec - 1;
                        new_l_set_tuple->L_SYM_time.tv_usec = current_time.tv_usec;
                        L_status = (uint8_t) _lset_get_linkstatus(new_l_set_tuple);
                        if(L_status == HEARD) {
                            gettimeofday(&new_l_set_tuple->L_time, NULL);
                            TIMEVAL_ADD(&new_l_set_tuple->L_time, L_HOLD_TIME.tv_sec, L_HOLD_TIME.tv_usec);
                        }
                    }
                }
            }
        }
    }
    memcpy(&new_l_set_tuple->L_neighbor_iface_addr, sending_address, ETH_ALEN);
    gettimeofday(&time1, NULL);
    _timeval_add_ms(&time1, validity_time);
    if(timercmp(&time1, &new_l_set_tuple->L_SYM_time, >) == 1) {
        new_l_set_tuple->L_HEARD_time = time1;
    } else {
        new_l_set_tuple->L_HEARD_time = new_l_set_tuple->L_SYM_time;
    }
    L_status = (uint8_t) _lset_get_linkstatus(new_l_set_tuple);
    if(L_status == PENDING) {
        if(timercmp(&new_l_set_tuple->L_time, &new_l_set_tuple->L_HEARD_time, <) == 1) {
            new_l_set_tuple->L_time = new_l_set_tuple->L_HEARD_time;
        } 
    } else if(L_status == HEARD || L_status == SYMMETRIC) {
        gettimeofday(&time1, NULL);
        TIMEVAL_ADD(&time1, L_HOLD_TIME.tv_sec, L_HOLD_TIME.tv_usec);
        if(timercmp(&new_l_set_tuple->L_time, &time1, <) == 1) {
            new_l_set_tuple->L_time = time1;
        }
    }
    /* execute actions related to L_status changes */
    if(L_status_old != SYMMETRIC && L_status == SYMMETRIC) {
        _link_changed_to_symmetric(new_l_set_tuple);
    } else if(L_status_old == SYMMETRIC && L_status != SYMMETRIC) {
        _link_changed_to_not_symmetric(new_l_set_tuple, l_set_entry);
    }
    _do_linkset_mpr_updates(ab_buffer, &new_l_set_tuple);
    if(L_status_old == SYMMETRIC && (L_status == HEARD || L_status == LOST)) {
        *reselect_mprs = true;
    } else if(L_status == SYMMETRIC && (L_status_old == LOST || L_status_old == HEARD)) {
        *reselect_mprs = true;
    }
    if(add_new == true) {
        if(L_status == SYMMETRIC) {
            *reselect_mprs = true;
        }
        HASH_ADD(hh, l_set_entry->l_set_tuple_list, L_neighbor_iface_addr, ETH_ALEN, new_l_set_tuple);
        dessert_debug("      "MAC" (neighbor) added.", EXPLODE_ARRAY6(new_l_set_tuple->L_neighbor_iface_addr));
    } else {
        dessert_debug("      "MAC" (neighbor) updated.", EXPLODE_ARRAY6(new_l_set_tuple->L_neighbor_iface_addr));
    }
}

void _do_n2set_updates(AB_buffer_t *ab_buffer, N_set_addrlist_t *removed_address_list, N_set_addrlist_t *neighbor_address_list, uint8_t *sending_address, dessert_meshif_t* receiving_iface, uint16_t *validity_time, bool *reselect_mprs) {
    dessert_meshif_t    *I_local_iface_addr_list;
    N2_set_t            *n2_set_entry, *new_n2_set_entry;
    N2_set_lookup_key_t *lookup_key;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp, *n2_set_tuple_match, *new_n2_set_tuple;
    L_set_t             *l_set_entry;
    L_set_tuple_t       *l_set_tuple;
    AB_buffer_t         *ab_buffer_item, *ab_buffer_item_tmp;
    N_set_addrlist_t    *neighbor_address_list_match, *removed_address_list_mach;
    uint8_t             L_status, is_local, keylen;
    struct timeval      current_time;

    dessert_debug("  Doing n2_set updates.");

    /* 12.6.1 */
    /* Remove all tuples containing addresses from the Removed Address List. */
    /* Therefor, first get the n2_set entry for the receiving local interface. */
    HASH_FIND(hh, n2_set, &receiving_iface->hwaddr, ETH_ALEN, n2_set_entry);
    if(n2_set_entry != NULL) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            dessert_debug("    ("MAC" , "MAC") checked with removed address list.", EXPLODE_ARRAY6(n2_set_tuple->N2_neighbor_iface_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_2hop_addr));
            HASH_FIND(hh, removed_address_list, n2_set_tuple->N2_neighbor_iface_addr, ETH_ALEN, removed_address_list_mach);
            /* if an address is found, the whole tuple is removed */
            if(removed_address_list_mach != NULL) {
                dessert_debug("    "MAC" in removed address list. Removing n2 set tuple.", EXPLODE_ARRAY6(removed_address_list_mach->N_neighbor_addr));
                HASH_DEL(n2_set_entry->N2_set_tuple_list, n2_set_tuple);
                free(n2_set_tuple);
                *reselect_mprs = true;
            }
        }
    }

    /* 12.6.2 */
    /* Get the link tuple matching the sending address. */
    HASH_FIND(hh, l_set, &receiving_iface->hwaddr, ETH_ALEN, l_set_entry);
    if(l_set_entry == NULL) {
        dessert_debug("    "MAC" (local address) has no l_set entry. No further processing needed.", EXPLODE_ARRAY6(receiving_iface->hwaddr));
        return;
    }
    HASH_FIND(hh, l_set_entry->l_set_tuple_list, sending_address, ETH_ALEN, l_set_tuple);
    if(l_set_tuple == NULL) {
        dessert_debug("    "MAC" has no existing link tuple. Returning.", EXPLODE_ARRAY6(sending_address));
        return;
    }
    /* calculate the key length including padding, using formula from the UT HASH documentation*/
    keylen = offsetof(N2_set_tuple_t, N2_2hop_addr) + sizeof(lookup_key->N2_2hop_addr) - offsetof(N2_set_tuple_t, N2_neighbor_iface_addr);
    L_status = (uint8_t) _lset_get_linkstatus(l_set_tuple);
    if(L_status == SYMMETRIC) {
        HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
            HASH_FIND(hh, neighbor_address_list, ab_buffer_item->AB_addr, ETH_ALEN, neighbor_address_list_match);
            I_local_iface_addr_list = dessert_meshiflist_get();
            is_local = false;
            while(I_local_iface_addr_list != NULL) {
                if(memcmp(&I_local_iface_addr_list->hwaddr, &ab_buffer_item->AB_addr, ETH_ALEN) == 0) {
                    is_local = true;
                    break;
                }
                I_local_iface_addr_list = I_local_iface_addr_list->next;
            }
            if(neighbor_address_list_match == NULL && is_local == false) {
                //if(ab_buffer_item->AB_type == LINK_STATUS || ab_buffer_item->AB_type == OTHER_NEIGHB) {
                /* OLSRv2 draft 12.6.2.1.1 */
                if((ab_buffer_item->AB_flags >> 4) == LINK_STATUS || (ab_buffer_item->AB_flags >> 4) == OTHER_NEIGHB) {
                    //if(ab_buffer_item->AB_value == SYMMETRIC) {
                    if((ab_buffer_item->AB_flags & 0x0F) == SYMMETRIC) {
                        n2_set_tuple_match = NULL;
                        if(n2_set_entry != NULL) {
                            lookup_key = malloc(sizeof(N2_set_lookup_key_t));
                            memset(lookup_key, 0, sizeof(N2_set_lookup_key_t));
                            memcpy(&lookup_key->N2_neighbor_iface_addr, sending_address, ETH_ALEN);
                            memcpy(&lookup_key->N2_2hop_addr, &ab_buffer_item->AB_addr, ETH_ALEN);
                            HASH_FIND(hh, n2_set_entry->N2_set_tuple_list, &lookup_key->N2_neighbor_iface_addr, keylen, n2_set_tuple_match);
                            free(lookup_key);
                        } else {
                            /* We have no existing entry for this receiving interface. Create one.*/
                            new_n2_set_entry = malloc(sizeof(N2_set_t));
                            memset(new_n2_set_entry, 0, sizeof(N2_set_t));
                            memcpy(&new_n2_set_entry->N2_local_iface_addr, &receiving_iface->hwaddr, ETH_ALEN);
                            new_n2_set_entry->N2_set_tuple_list = NULL;
                            HASH_ADD(hh, n2_set, N2_local_iface_addr, ETH_ALEN, new_n2_set_entry);
                            n2_set_entry = new_n2_set_entry;
                        }
                        gettimeofday(&current_time, NULL);
                        if(n2_set_tuple_match == NULL) {
                            /* We have no existing tuple for that address pair. Create one. */
                            new_n2_set_tuple = malloc(sizeof(N2_set_tuple_t));
                            memset(new_n2_set_tuple, 0, sizeof(N2_set_tuple_t));
                            memcpy(&new_n2_set_tuple->N2_neighbor_iface_addr, sending_address, ETH_ALEN);
                            memcpy(&new_n2_set_tuple->N2_2hop_addr, &ab_buffer_item->AB_addr, ETH_ALEN);
                            new_n2_set_tuple->N2_time = current_time;
                            _timeval_add_ms(&new_n2_set_tuple->N2_time, validity_time);
                            HASH_ADD(hh, n2_set_entry->N2_set_tuple_list, N2_neighbor_iface_addr, keylen, new_n2_set_tuple);
                            dessert_debug("        ("MAC" , "MAC") pair added to n2_set.", EXPLODE_ARRAY6(new_n2_set_tuple->N2_neighbor_iface_addr), EXPLODE_ARRAY6(new_n2_set_tuple->N2_2hop_addr));
                            *reselect_mprs = true;
                        } else {
                            /* We have an existing tuple for that address pair. Update it. */
                            memcpy(&n2_set_tuple_match->N2_neighbor_iface_addr, sending_address, ETH_ALEN);
                            n2_set_tuple_match->N2_time = current_time;
                            _timeval_add_ms(&n2_set_tuple_match->N2_time, validity_time);
                            dessert_debug("      ("MAC" , "MAC") pair updated.", EXPLODE_ARRAY6(n2_set_tuple_match->N2_neighbor_iface_addr), EXPLODE_ARRAY6(n2_set_tuple_match->N2_2hop_addr));
                        }
                    }
                //} else if((ab_buffer_item->AB_type == LINK_STATUS && (ab_buffer_item->AB_value == LOST || ab_buffer_item->AB_value == HEARD)) ||
                    //(ab_buffer_item->AB_type == OTHER_NEIGHB && ab_buffer_item->AB_value == LOST)) {
                /* OLSRv2 draft 12.6.2.1.2 */
                } else if(((ab_buffer_item->AB_flags >> 4) == LINK_STATUS && ((ab_buffer_item->AB_flags & 0x0F) == LOST || (ab_buffer_item->AB_flags & 0x0F) == HEARD)) ||
                            ((ab_buffer_item->AB_flags >> 4) == OTHER_NEIGHB && (ab_buffer_item->AB_flags & 0x0F) == LOST)) {
                    lookup_key = malloc(sizeof(N2_set_lookup_key_t));
                    memset(lookup_key, 0, sizeof(N2_set_lookup_key_t));
                    memcpy(&lookup_key->N2_neighbor_iface_addr, sending_address, ETH_ALEN);
                    memcpy(&lookup_key->N2_2hop_addr, &ab_buffer_item->AB_addr, ETH_ALEN);
                    HASH_FIND(hh, n2_set_entry->N2_set_tuple_list, &lookup_key->N2_neighbor_iface_addr, keylen, n2_set_tuple_match);
                    if(n2_set_tuple_match != NULL) {
                        dessert_debug("        ("MAC" , "MAC") removed from n2 set.", EXPLODE_ARRAY6(n2_set_tuple_match->N2_neighbor_iface_addr), EXPLODE_ARRAY6(n2_set_tuple_match->N2_2hop_addr));
                        HASH_DEL(n2_set_entry->N2_set_tuple_list, n2_set_tuple_match);
                        free(n2_set_tuple_match);
                        *reselect_mprs = true;
                    }
                    free(lookup_key);
                }
            } 
        }
    }
}

void _link_heard_timeout(L_set_tuple_t *l_set_tuple, L_set_t **l_set_entry) {
    N_set_t             *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *n_addrlist_match, *n_addrlist_match2, *n_set_addrlist, *n_set_addrlist_tmp;
    L_set_tuple_t       *l_set_tuple_backup, *l_set_tuple_tmp;
    L_set_t             *l_set_entry2, *l_set_entry2_tmp;
    bool                link_tuples_remain;

    link_tuples_remain = false;
    /* First, backup this l_set_tuple */
    l_set_tuple_backup = malloc(sizeof(L_set_tuple_t));
    *l_set_tuple_backup = *l_set_tuple;
    /* Now, temporarily remove it */
    HASH_DEL((*l_set_entry)->l_set_tuple_list, l_set_tuple);
    free(l_set_tuple);

    /* 13.3.1 */
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple_backup->L_neighbor_iface_addr, ETH_ALEN, n_addrlist_match);
        if(n_addrlist_match != NULL) {
            /* 13.3.1.1*/
            HASH_ITER(hh, l_set, l_set_entry2, l_set_entry2_tmp) {
                HASH_ITER(hh, l_set_entry2->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
                    HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_addrlist_match2);
                    if(n_addrlist_match2 != NULL && _check_expired(&l_set_tuple->L_HEARD_time) == 0) {
                        link_tuples_remain = true;
                    }
                }
            }
            /* No remaining link tuples and L_HEARD time not expired */
            if(link_tuples_remain == false) {
                HASH_ITER(hh, n_set_entry->N_neighbor_addr_list, n_set_addrlist, n_set_addrlist_tmp) {
                    dessert_crit("  "MAC" removed from neighborset.", EXPLODE_ARRAY6(n_set_addrlist->N_neighbor_addr));
                }
                dessert_debug("  Neighbor tuple was %s.", n_set_entry->N_symmetric ? "SYMMETRIC" : "NOT SYMMETRIC");
                HASH_DELETE(hh1, n_set, n_set_entry);
                free(n_set_entry);
            }
        }
    }
    /* Restore the previously deleted link tuple */
    HASH_ADD(hh, (*l_set_entry)->l_set_tuple_list, L_neighbor_iface_addr, ETH_ALEN, l_set_tuple_backup);
    l_set_tuple = l_set_tuple_backup;
}

void _link_changed_to_symmetric(L_set_tuple_t *l_set_tuple) {
    N_set_t             *n_set_entry, *n_set_entry_tmp, *n_set_tuple;
    N_set_addrlist_t    *n_set_address;
    NL_set_t            *nl_set_entry, *nl_set_entry_tmp;

    /* NHDP 13.1 */
    n_set_tuple = NULL;
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_set_address);
        if(n_set_address != NULL) {
            dessert_debug("Set "MAC" to symmetric.", EXPLODE_ARRAY6(n_set_address->N_neighbor_addr));
            n_set_entry->N_symmetric = true;
            n_set_tuple = n_set_entry;
            break;
        }
    }
    if(n_set_tuple == NULL) {
        dessert_warn("Couldnt find "MAC" in nset.", EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
        return;
    }
    HASH_ITER(hh, nl_set, nl_set_entry, nl_set_entry_tmp) {
        HASH_FIND(hh, n_set_tuple->N_neighbor_addr_list, nl_set_entry->NL_neighbor_addr, ETH_ALEN, n_set_address);
        if(n_set_address != NULL) {
            dessert_debug("Removed "MAC" from lost neighbor address list.", EXPLODE_ARRAY6(n_set_address->N_neighbor_addr));
            HASH_DEL(nl_set, nl_set_entry);
            free(nl_set_entry);
        }
    }
}
/* This function implements RFC part 13.2 */
void _link_changed_to_not_symmetric(L_set_tuple_t *l_set_tuple, L_set_t *l_set_entry) {
    N2_set_t                        *n2_set_entry;
    N2_set_tuple_t                  *n2_set_tuple_item, *n2_set_tuple_item_tmp;
    N_set_t                         *n_set_entry, *n_set_entry_tmp, *n_set_tuple;
    N_set_addrlist_t                *n_neighbor_addr, *n_neighbor_addr_tmp;
    L_set_t                         *l_set_entry2, *l_set_entry2_tmp;
    L_set_tuple_t                   *l_set_tuple2, *l_set_tuple2_tmp;
    NL_set_t                        *nl_set_tuple;
    uint8_t                         contained, L_status;

    /* 13.2.1 */
    if(n2_set != NULL) {
        HASH_FIND(hh, n2_set, &l_set_entry->L_local_iface_addr, ETH_ALEN, n2_set_entry);
        if(n2_set_entry != NULL) {
            HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple_item, n2_set_tuple_item_tmp) {
                if(memcmp(&n2_set_tuple_item->N2_neighbor_iface_addr, &l_set_tuple->L_neighbor_iface_addr, ETH_ALEN) == 0) {
                    dessert_debug("    Successfully removed ("MAC"/"MAC") from n2_set", EXPLODE_ARRAY6(n2_set_tuple_item->N2_2hop_addr), EXPLODE_ARRAY6(n2_set_tuple_item->N2_neighbor_iface_addr));
                    HASH_DEL(n2_set_entry->N2_set_tuple_list, n2_set_tuple_item);
                    free(n2_set_tuple_item);
                }
            }
        } else {
            dessert_debug("    "MAC" has no n2_set entry.", EXPLODE_ARRAY6(n2_set_entry->N2_local_iface_addr));
        }
    }

    /* NHDP 13.2.2 */
    /* Get the needed neighborset tuple. */
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_neighbor_addr);
        if(n_neighbor_addr != NULL) {
            n_set_tuple = n_set_entry;
            break;
        } 
    }
    /* NHDP 13.2.2.1 */
    contained = false;
    HASH_ITER(hh, l_set, l_set_entry2, l_set_entry2_tmp) {
        HASH_ITER(hh, l_set_entry2->l_set_tuple_list, l_set_tuple2, l_set_tuple2_tmp) {
            L_status = _lset_get_linkstatus(l_set_tuple2);
            if(L_status == SYMMETRIC) {
                HASH_FIND(hh, n_set_tuple->N_neighbor_addr_list, l_set_tuple2->L_neighbor_iface_addr, ETH_ALEN, n_neighbor_addr);
                if(n_neighbor_addr != NULL) {
                    contained = true;
                    return;
                }
            }
        }
    }
    if(contained == false) {
        n_set_tuple->N_symmetric = false;
        HASH_ITER(hh, n_set_tuple->N_neighbor_addr_list, n_neighbor_addr, n_neighbor_addr_tmp) {
            dessert_debug("    "MAC" added to lost neighbor set.", EXPLODE_ARRAY6(n_neighbor_addr->N_neighbor_addr));
            nl_set_tuple = malloc(sizeof(NL_set_t));
            memcpy(&nl_set_tuple->NL_neighbor_addr, &n_neighbor_addr->N_neighbor_addr, ETH_ALEN);
            gettimeofday(&nl_set_tuple->NL_time, NULL);
            TIMEVAL_ADD(&nl_set_tuple->NL_time, N_HOLD_TIME.tv_sec, N_HOLD_TIME.tv_usec);
            HASH_ADD(hh, nl_set, NL_neighbor_addr, ETH_ALEN, nl_set_tuple);
        }
    }
}

void _pdr_packet_trap_add(float *pdr, uint8_t *address) {
    PDR_global_trap_t  *neighbor_trap;
    PDR_neighbor_trap_t   *neighbor_trap_entry, *neighbor_trap_entry_item, *neighbor_trap_entry_item_tmp;
    struct timeval      now, result, window, tmp;
    uint64_t            hello_interval_ms;
    uint8_t             num_entries;
    
    gettimeofday(&now, NULL);
    // first try to get existing packet trap for that neighbor
    HASH_FIND(hh, pdr_global_trap, address, ETH_ALEN, neighbor_trap);
    if(neighbor_trap == NULL) {
        // there is no packet trap for that neighbor, create one
        neighbor_trap = malloc(sizeof(PDR_global_trap_t));
        memcpy(&neighbor_trap->n_addr, address, ETH_ALEN);
        neighbor_trap->pdr_packet_trap = NULL;
        neighbor_trap_entry = malloc(sizeof(PDR_neighbor_trap_t));
        neighbor_trap_entry->timestamp = now;
        HASH_ADD(hh, neighbor_trap->pdr_packet_trap, timestamp, sizeof(struct timeval), neighbor_trap_entry);
        HASH_ADD(hh, pdr_global_trap, n_addr, ETH_ALEN, neighbor_trap);
    } else {
        neighbor_trap_entry = malloc(sizeof(PDR_neighbor_trap_t));
        neighbor_trap_entry->timestamp = now;
        HASH_ADD(hh, neighbor_trap->pdr_packet_trap, timestamp, sizeof(struct timeval), neighbor_trap_entry);
    }
    window.tv_sec = 0;
    window.tv_usec = 0;
    hello_interval_ms = dessert_timeval2ms(&NHDP_HELLO_INTERVAL);
    hello_interval_ms = hello_interval_ms * (PDR_WINDOW_SIZE - 1);
    _timeval_add_ms(&window, &hello_interval_ms);
    _timeval_subtract(&result, &now, &window);
    HASH_ITER(hh, neighbor_trap->pdr_packet_trap, neighbor_trap_entry_item, neighbor_trap_entry_item_tmp) {
        if(timercmp(&result, &neighbor_trap_entry_item->timestamp, >) == true) {
            HASH_DEL(neighbor_trap->pdr_packet_trap, neighbor_trap_entry_item);
            free(neighbor_trap_entry_item);
        }
    }
    num_entries = HASH_COUNT(neighbor_trap->pdr_packet_trap);
    if(pdr_calculation_started == true && num_entries > 0) {
        *pdr = (float) num_entries / (float) (PDR_WINDOW_SIZE);
    } else if(pdr_calculation_started == true && num_entries == 0) {
        *pdr = (float)0;
    } else {
         *pdr = INITIAL_QUALITY;
    }
}

void _reset_pdr_packet_traps() {
    PDR_global_trap_t   *pdr_trap_item, *pdr_trap_item_tmp;
    PDR_neighbor_trap_t *pdr_packet_trap_item, *pdr_packet_trap_item_tmp;
    
    pdr_calculation_started = false;
    HASH_ITER(hh, pdr_global_trap, pdr_trap_item, pdr_trap_item_tmp) {
        HASH_ITER(hh, pdr_trap_item->pdr_packet_trap, pdr_packet_trap_item, pdr_packet_trap_item_tmp) {
            HASH_DEL(pdr_trap_item->pdr_packet_trap, pdr_packet_trap_item);
            free(pdr_packet_trap_item);
        }
        HASH_DEL(pdr_global_trap, pdr_trap_item);
        free(pdr_trap_item);
    }
    _start_pdr_delay_task();
}

dessert_cb_result nhdp_handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t               *ext;
    N_set_addrlist_t            *removed_address_list, *lost_address_list, *neighbor_address_list, *address_list_item;
    struct nhdp_address_block   *address_block;
    struct nhdp_hello_hdr       *nhdp_hello_hdr;
    uint16_t                    address_block_count, counter, validity_time, ext_size, byte_pos;
    uint8_t                     *ptr, *sending_address, willingness;
    int                         ext_index;
    AB_buffer_t                 *ab_buffer, *ab_buffer_item;     ///< stores all address blocks from the hello
    bool                        reselect_mprs, localif;
    float                       pdr;
    
    if(msg->u8 & NHDP_HELLO) {
        if(activated & USE_NHDP) {
            pthread_mutex_lock(&all_sets_lock);
            logNhdpHelloRx(msg, len, proc, iface);
            dessert_debug("                                                                                 ");
            dessert_debug("INCOMING NHDP HELLO");
            removed_address_list    = NULL;
            lost_address_list       = NULL;
            neighbor_address_list   = NULL;
            ab_buffer               = NULL;
            sending_address         = NULL;

            ext_index = 0;
            if(dessert_msg_getext(msg, &ext, NHDP_HELLO_EXT, ext_index) == 0) {
                dessert_crit("  Error retreiving NHDP extension from Hello. Aborting.");
                pthread_mutex_unlock(&all_sets_lock);
                return DESSERT_MSG_DROP;
            }
            nhdp_hello_hdr = (struct nhdp_hello_hdr*) ext->data;
            address_block_count = nhdp_hello_hdr->num_address_blocks;
            validity_time       = nhdp_hello_hdr->validity_time;
            if(USE_MPRS == true) {
                willingness = nhdp_hello_hdr->willingness;
            } else {
                willingness = WILL_DEFAULT;
            }
            counter = address_block_count;
            /* Create the temporary Neighbor Address List from the HELLO packets address blocks */
            ptr = (uint8_t*) nhdp_hello_hdr;
            ptr += sizeof(struct nhdp_hello_hdr);
            localif = false;
            ext_size = ext->len - 2 - sizeof(struct nhdp_hello_hdr);
            byte_pos = 0;
            while(counter > 0) {
                if(byte_pos >= ext_size) {
                    ext_index++;
                    dessert_msg_getext(msg, &ext, NHDP_HELLO_EXT, ext_index);
                    ext_size = ext->len - 2;
                    byte_pos = 0;
                    address_block = (struct nhdp_address_block*) ext->data;
                    ptr = (uint8_t*) ext->data;
                } else {
                    address_block = (struct nhdp_address_block*) ptr;
                }
                if((address_block->flags >> 4) == LOCAL_IF && localif == false) {
                    localif = true;
                    address_list_item = malloc(sizeof(N_set_addrlist_t));
                    memcpy(&address_list_item->N_neighbor_addr, &address_block->address, ETH_ALEN);
                    HASH_ADD(hh, neighbor_address_list, N_neighbor_addr, ETH_ALEN, address_list_item);
                    dessert_debug("  " MAC " FLAGS: %x (neighbor: TRUE) added to neighbor address list.", EXPLODE_ARRAY6(address_block->address), address_block->flags);
                    if((address_block->flags & 0x0F) == THIS_IF) {
                        sending_address = malloc(ETH_ALEN);
                        memcpy(sending_address, &address_block->address, ETH_ALEN);
                        _pdr_packet_trap_add(&pdr, sending_address);
                    }
                } else if((address_block->flags >> 4) == LOCAL_IF && localif == true) {
                    dessert_debug("  " MAC " FLAGS: %x (neighbor: TRUE) dropped! INVALID. REASON: Multiple LOCAL_IF entries.", EXPLODE_ARRAY6(address_block->address), address_block->flags);
                    _do_cleanup(&ab_buffer, &neighbor_address_list, &removed_address_list, &lost_address_list);
                    pthread_mutex_unlock(&all_sets_lock);
                    return DESSERT_MSG_DROP;
                } else {
                    dessert_debug("  " MAC " FLAGS: %x (neighbor: FALSE) added to ab_buffer.", EXPLODE_ARRAY6(address_block->address), address_block->flags);
                }
                ab_buffer_item = malloc(sizeof(AB_buffer_t));
                memcpy(&ab_buffer_item->AB_addr, &address_block->address, ETH_ALEN);
                ab_buffer_item->AB_flags = (address_block->flags);
                HASH_ADD(hh, ab_buffer, AB_addr, ETH_ALEN, ab_buffer_item);
                counter--;
                byte_pos += sizeof(struct nhdp_address_block);
                ptr += sizeof(struct nhdp_address_block);
            }
            if(localif == false || sending_address == NULL) {
                /* This NHDP_HELLO is invalid. Drop it.*/
                dessert_crit("  Invalid NHDP Hello dropped. localif: %s sending_address: %s",localif ? "true":"false", sending_address == NULL ? "true":"false");
                _do_cleanup(&ab_buffer, &neighbor_address_list, &removed_address_list, &lost_address_list);
                pthread_mutex_unlock(&all_sets_lock);
                return DESSERT_MSG_DROP;
            }
            reselect_mprs = false;
            /* First do all NHDP (RFC6130) updates */
            _do_neighborset_updates(ab_buffer, neighbor_address_list, &removed_address_list, &lost_address_list, &willingness, &reselect_mprs);
            _do_lost_neighborset_updates(lost_address_list);
            _do_linkset_updates(ab_buffer, removed_address_list, sending_address, iface, &validity_time, &reselect_mprs, &pdr);
            _do_n2set_updates(ab_buffer, removed_address_list, neighbor_address_list, sending_address, iface, &validity_time, &reselect_mprs);
            /* Second reselect MPRs if necessary */
            if(USE_MPRS == true && reselect_mprs == true && !reselect_mprs_task) {
                _reselect_mprs(NULL, NULL, NULL);
            } else {
                dessert_debug("  Skipping MPR reselection. REASON: USE_MPRS == %s reselect_mprs == %s", USE_MPRS ? "true":"false", reselect_mprs ? "true":"false");
            }
            _do_cleanup(&ab_buffer, &neighbor_address_list, &removed_address_list, &lost_address_list);
            free(sending_address);
            pthread_mutex_unlock(&all_sets_lock);
        } else {
            dessert_warning("  Received NHDP Hello, but NHDP is disabled!");
            dessert_debug("                                                                                 ");
        }
        dessert_debug("                                                                                 ");
        return DESSERT_MSG_DROP;
    }
    dessert_debug("                                                                                 ");
    return DESSERT_MSG_KEEP;
}

void nhdp_start() {
    _start_nhdp_hello_task();
    _start_nhdp_cleanup_task();
    if(mpr_selectors_logging == true) {
        _start_mpr_selectors_logging_task();
    }
    if(n2_logging == true) {
        _start_n2_logging_task();
    }
    _start_pdr_delay_task();
    _start_mpr_selection_task();
}

void nhdp_stop() {
    _stop_nhdp_hello_task();
    _stop_nhdp_cleanup_task();
    if(mpr_selectors_logging == true) {
        _stop_mpr_selectors_logging_task();
    }
    if(n2_logging == true) {
        _stop_n2_logging_task();
    }
    _stop_mpr_selection_task();
    
}
/* Start/stop tasks.*/
void _start_nhdp_hello_task() {
    if(!nhdp_hello_task) {
        //struct timeval scheduled;
        //gettimeofday(&scheduled, NULL);
        //TIMEVAL_ADD(&scheduled, NHDP_HELLO_INTERVAL.tv_sec, NHDP_HELLO_INTERVAL.tv_usec);
        nhdp_hello_task = dessert_periodic_add((dessert_periodiccallback_t *) nhdp_send_hello, NULL, NULL, &NHDP_HELLO_INTERVAL);
        dessert_notice("Successfully started NHDP HELLO task every %d.%d seconds.", NHDP_HELLO_INTERVAL.tv_sec, NHDP_HELLO_INTERVAL.tv_usec);
    } else {
        dessert_notice("Unable to start NHDP HELLO task. It is already running.");
    }
}
void _stop_nhdp_hello_task() {
    if(nhdp_hello_task) {
        dessert_periodic_del(nhdp_hello_task);
        nhdp_hello_task = NULL;
        dessert_notice("Successfully removed NHDP HELLO task.");
    } else {
        dessert_notice("Unable to remove NHDP HELLO task. It is not running.");
    }
}
void _start_nhdp_cleanup_task() {
    if(!nhdp_cleanup_expired_task) {
        nhdp_cleanup_expired_task = dessert_periodic_add((dessert_periodiccallback_t *) nhdp_cleanup_expired, NULL, NULL, &NHDP_CLEANUP_INTERVAL);
        dessert_notice("Successfully started NHDP CLEANUP task every %d.%d seconds.", NHDP_CLEANUP_INTERVAL.tv_sec, NHDP_CLEANUP_INTERVAL.tv_usec);
    } else {
        dessert_notice("Unable to start NHDP CLEANUP task. It is already running.");
    }
}
void _stop_nhdp_cleanup_task() {
    if(nhdp_cleanup_expired_task) {
        dessert_periodic_del(nhdp_cleanup_expired_task);
        nhdp_cleanup_expired_task = NULL;
        dessert_notice("Successfully removed NHDP CLEANUP task.");
    } else {
        dessert_notice("Unable to remove NHDP CLEANUP task. It is not running.");
    }
}

void _start_mpr_selection_task() {
    if(!reselect_mprs_task) {
        reselect_mprs_task = dessert_periodic_add((dessert_periodiccallback_t *) _reselect_mprs, NULL, NULL, &MPR_RESELECT_INTERVAL);
        dessert_notice("Successfully started MPR SELECTION TASK every %d.%d seconds.", MPR_RESELECT_INTERVAL.tv_sec, MPR_RESELECT_INTERVAL.tv_usec);
    } else {
        dessert_notice("Unable to start MPR SELECTION TASK. It is already running.");
    }
}

void _stop_mpr_selection_task() {
    if(reselect_mprs_task) {
        dessert_periodic_del(reselect_mprs_task);
        reselect_mprs_task = NULL;
        dessert_notice("Successfully remove MPR SELECTION TASK.");
    } else {
        dessert_notice("Unable to stop MPR SELECTION task. It is not running.");
    }
}

void _start_mpr_selectors_logging_task() {
    if(!log_mpr_selectors_task) {
        log_mpr_selectors_task = dessert_periodic_add((dessert_periodiccallback_t *) _log_mpr_selector_string, NULL, NULL, &MPR_SELECTOR_LOGGING_INTERVAL);
        dessert_notice("Successfully started MPR selector logging every %d.%d s...",MPR_SELECTOR_LOGGING_INTERVAL.tv_sec, MPR_SELECTOR_LOGGING_INTERVAL.tv_usec);
    } else {
        dessert_notice("Unable to start MPR selector logging task. It is already running.");
    }
}

void _stop_mpr_selectors_logging_task() {
    if(log_mpr_selectors_task) {
        dessert_periodic_del(log_mpr_selectors_task);
        log_mpr_selectors_task = NULL;
        dessert_notice("Successfully removed MPR selectors logging task.");
    } else {
        dessert_notice("Unable to remove MPR selectors logging task. It is not running.");
    }
}
void _start_n2_logging_task() {
    if(!log_n2_set_task) {
        log_n2_set_task = dessert_periodic_add((dessert_periodiccallback_t *) _log_n2_set_string, NULL, NULL, &N2_LOGGING_INTERVAL);
        dessert_notice("Successfully started N2 logging every %d.%d s...",N2_LOGGING_INTERVAL.tv_sec, N2_LOGGING_INTERVAL.tv_usec);
    } else {
        dessert_notice("Unable to start N2 logging task. It is already running.");
    }
}
void _stop_n2_logging_task() {
    if(log_n2_set_task) {
        dessert_periodic_del(log_n2_set_task);
        log_n2_set_task = NULL;
        dessert_notice("Successfully removed N2 set logging task.");
    } else {
        dessert_notice("Unable to remove N2 set logging task. It is not running.");
    }
}
void _start_pdr_delay_task() {
    if(!pdr_started_task) {
        pdr_started_task = dessert_periodic_add_delayed((dessert_periodiccallback_t *) _set_pdr_started, NULL, PDR_CALC_DELAY.tv_sec);
        dessert_notice("Successfully started PDR delay task. PDR calculation starting in %d.%d s",PDR_CALC_DELAY.tv_sec, PDR_CALC_DELAY.tv_usec);
    } else {
        dessert_notice("Unable to start PDR delay task. It is already running.");
    }
}

// timer called functions
void _set_pdr_started() {
    pdr_calculation_started = true;
    if(pdr_started_task) {
        pdr_started_task = NULL;
        dessert_notice("Successfully removed PDR delay task after running it once.");
    } else {
        dessert_notice("Unable to stop PDR delay task. It is not running.");
    }
}

void _do_cleanup(AB_buffer_t **ab_buffer, N_set_addrlist_t **neighbor_address_list, N_set_addrlist_t **removed_address_list, N_set_addrlist_t **lost_address_list) {
    AB_buffer_t         *ab_buffer_item, *ab_buffer_item_tmp;
    N_set_addrlist_t    *addrlist_item, *addrlist_item_tmp;
    
    HASH_ITER(hh, *ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
        HASH_DEL(*ab_buffer, ab_buffer_item);
        free(ab_buffer_item);
    }

    HASH_ITER(hh, *neighbor_address_list, addrlist_item, addrlist_item_tmp) {
        HASH_DEL(*neighbor_address_list, addrlist_item);
        free(addrlist_item);
    }

    HASH_ITER(hh, *lost_address_list, addrlist_item, addrlist_item_tmp) {
        HASH_DEL(*lost_address_list, addrlist_item);
        free(addrlist_item);
    }

    HASH_ITER(hh, *removed_address_list, addrlist_item, addrlist_item_tmp) {
        HASH_DEL(*removed_address_list, addrlist_item);
        free(addrlist_item);
    }
}

/* ====================== SCHEDULED TASKS ==================================*/
dessert_per_result_t nhdp_cleanup_expired(void* data, struct timeval* scheduled, struct timeval* interval) {
    L_set_t             *l_set_entry, *l_set_entry_tmp, *l_set_entry2, *l_set_entry2_tmp;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp, *l_set_tuple_match;
    N_set_t             *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *n_set_match, *n_neighbor_addr, *n_neighbor_addr_tmp;
    N2_set_t            *n2_set_entry, *n2_set_entry_tmp;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    bool                links_left, reselect_mprs;

    pthread_mutex_lock(&all_sets_lock);
    reselect_mprs = false;
    /* cleanup linkset and neighborset accordingly */
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
            if(_check_expired(&l_set_tuple->L_time) == 1) {
                dessert_debug("Link to "MAC" is expired. Deleting.", EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
                /* Check if there are other links to that neighbor */
                links_left = false;
                HASH_ITER(hh, l_set, l_set_entry2, l_set_entry2_tmp) {
                    if(HASH_COUNT(l_set_entry2->l_set_tuple_list) == 0) {
                        continue;
                    }
                    HASH_FIND(hh, l_set_entry2->l_set_tuple_list, &l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, l_set_tuple_match);
                    if(l_set_tuple_match != NULL) {
                        dessert_debug("  Other links to that neighbour found. Cleanup finished.");
                        links_left = true;
                        break;
                    }
                }
                /* Remove the neighborset entry with no links left now */
                if(links_left == false) {
                    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
                        /* remove all neighborset entries for that neighbor */
                        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, &l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_set_match);
                        if(n_set_match != NULL) {
                            HASH_ITER(hh, n_set_entry->N_neighbor_addr_list, n_neighbor_addr, n_neighbor_addr_tmp) {
                                dessert_debug("  Removing neigbhor address "MAC".", n_neighbor_addr);
                                HASH_DEL(n_set_entry->N_neighbor_addr_list, n_neighbor_addr);
                                free(n_neighbor_addr);
                            }
                            dessert_debug("  Neighbor "MAC" has no links left. Removing.", EXPLODE_ARRAY6(n_set_match->N_neighbor_addr));
                            HASH_DELETE(hh1, n_set, n_set_entry);
                            free(n_set_entry);
                        }
                    }
                }
                if(_lset_get_linkstatus(l_set_tuple) == SYMMETRIC) {
                    reselect_mprs = true;
                }
                HASH_DEL(l_set_entry->l_set_tuple_list, l_set_tuple);
                free(l_set_tuple);
            }
        }
    }
    /* cleanup 2-hop set */
    HASH_ITER(hh, n2_set, n2_set_entry, n2_set_entry_tmp) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            /* remove the tuple from the tuple list, if expired */
            if(_check_expired(&n2_set_tuple->N2_time) == 1) {
                dessert_debug("  Removed n2set tuple ("MAC"/"MAC").", EXPLODE_ARRAY6(n2_set_tuple->N2_2hop_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_neighbor_iface_addr));
                HASH_DEL(n2_set_entry->N2_set_tuple_list, n2_set_tuple);
                free(n2_set_tuple);
            }
            /* additionally remove the whole n2set entry, if no tuples remain */
            if(HASH_COUNT(n2_set_entry->N2_set_tuple_list) == 0) {
                dessert_debug("  Removed n2set entry "MAC".", EXPLODE_ARRAY6(n2_set_entry->N2_local_iface_addr));
                HASH_DEL(n2_set, n2_set_entry);
                free(n2_set_entry);
            }
        }
    }
    //_check_constraints();
    if(USE_MPRS && reselect_mprs == true && reselect_mprs_task == NULL) {
        _reselect_mprs(NULL, NULL, NULL);
    }
    pthread_mutex_unlock(&all_sets_lock);
    return DESSERT_PER_KEEP;
}

bool is_mpr(dessert_msg_t *msg) {
    L_set_t         *l_set_entry, *l_set_entry_tmp;
    L_set_tuple_t   *l_set_tuple;
    u_char          *shost;
    
    shost = msg->l2h.ether_shost;
    dessert_debug("Checking mpr status for address "MAC"", EXPLODE_ARRAY6(shost));

    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_FIND(hh, l_set_entry->l_set_tuple_list, shost, ETH_ALEN, l_set_tuple);
        if(l_set_tuple != NULL && l_set_tuple->L_mpr_selector == true) {
            dessert_debug("This router is MPR for "MAC". Forwarding message.", EXPLODE_ARRAY6(shost));
            return true;
        }
    }
    dessert_debug("This router is NO MPR for "MAC". Discarding message.", EXPLODE_ARRAY6(shost));
    return false;
}

uint8_t _num_neighbors() {
    pthread_mutex_lock(&all_sets_lock);
    return HASH_CNT(hh1, n_set);
    pthread_mutex_unlock(&all_sets_lock);
}

uint8_t _num_symmetric_neighbors() {
    N_set_t *n_set_entry, *n_set_entry_tmp;
    uint8_t sym_neighbors;

    pthread_mutex_lock(&all_sets_lock);
    sym_neighbors = 0;
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        if(n_set_entry->N_symmetric == true) {
            sym_neighbors++;
        }
    }
    pthread_mutex_unlock(&all_sets_lock);
    return sym_neighbors;
}

uint8_t _num_strict_2hop_neighbors() {
    N2_set_t            *n2_set_entry, *n2_set_entry_tmp;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    N_set_addrlist_t    *n2_address_set, *n2_address_set_item, *n2_address_set_item_tmp, *new_n2_address_set_item, *n2_2hop_addr_match;
    uint8_t             result;
    
    result = 0;
    n2_address_set = NULL;
    
    pthread_mutex_lock(&all_sets_lock);
    HASH_ITER(hh, n2_set, n2_set_entry, n2_set_entry_tmp) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            if(_isNeighbor(n2_set_tuple) == true) {
                continue;
            }
            HASH_FIND(hh, n2_address_set, n2_set_tuple->N2_2hop_addr, ETH_ALEN, n2_2hop_addr_match);
            if(n2_2hop_addr_match == NULL) {
                new_n2_address_set_item = malloc(sizeof(N_set_addrlist_t));
                memcpy(&new_n2_address_set_item->N_neighbor_addr, &n2_set_tuple->N2_2hop_addr, ETH_ALEN);
                HASH_ADD(hh, n2_address_set, N_neighbor_addr, ETH_ALEN, new_n2_address_set_item);
            }
        }
    }
    pthread_mutex_unlock(&all_sets_lock);
    result = HASH_COUNT(n2_address_set);
    HASH_ITER(hh, n2_address_set, n2_address_set_item, n2_address_set_item_tmp) {
        HASH_DEL(n2_address_set, n2_address_set_item);
        free(n2_address_set_item);
    }
    return result;
}

uint8_t _num_mprs() {
    N_set_t     *n_set_entry, *n_set_entry_tmp;
    uint8_t counter;

    pthread_mutex_lock(&all_sets_lock);
    counter = 0;
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        if(n_set_entry->N_mpr == true) {
            counter++;
        }
    }
    pthread_mutex_unlock(&all_sets_lock);
    return counter;
}

uint8_t _num_mpr_selectors() {
    L_set_t             *l_set_entry, *l_set_entry_tmp;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp;
    uint8_t             counter;
    
    counter = 0;
    pthread_mutex_lock(&all_sets_lock);
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
            if(l_set_tuple->L_mpr_selector == true) {
                counter++;
            }
        }
    }
    pthread_mutex_unlock(&all_sets_lock);
    return counter;
}

/* Send out hello message on each local mesh interface. */
dessert_per_result_t nhdp_send_hello(void* data, struct timeval* scheduled, struct timeval* interval) {
    dessert_meshif_t            *local_iface;
    struct ether_header         *eth;
    struct nhdp_hello_hdr*      nhdp_hello_hdr;
    dessert_msg_t               *msg;
    dessert_ext_t               *ext;
    uint8_t                     num_address_blocks, block_counter;
    uint16_t                    ext_size;
    AB_buffer_t                 *ab_buffer, *ab_buffer_item, *ab_buffer_item_tmp;
    struct nhdp_address_block   *addr_block;
    void                        *ext_ptr;

    ab_buffer = NULL;
    dessert_debug("                                                                                 ");
    dessert_debug("SENDING HELLOS ON ALL INTERFACES");
    /* Iterate over all local mesh interfaces. */
    local_iface = dessert_meshiflist_get();
    if(local_iface == NULL) {
        dessert_warning("  No mesh interfaces found...");
        return DESSERT_PER_KEEP;
    }
    while(local_iface != NULL) {
        /* create new dessert msg of type NHDP_HELLO. */
        dessert_msg_new(&msg);
        msg->u8 |= NHDP_HELLO;
        msg->ttl = 1;
        addSeq(msg);
        /* add the l25 ethernet header to the packet. */
        dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
        eth = (struct ether_header*) ext->data;
        memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
        memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);
        pthread_mutex_lock(&all_sets_lock);
        _create_local_iface_addrlist(local_iface, &ab_buffer);
        _create_linkset_addrlist(local_iface, &ab_buffer);
        _create_neighbor_addrlist(&ab_buffer);
        pthread_mutex_unlock(&all_sets_lock);
        ext_size = sizeof(struct nhdp_hello_hdr) + HASH_COUNT(ab_buffer) * sizeof(struct nhdp_address_block);
        if(ext_size > DESSERT_MAXEXTDATALEN) {
            num_address_blocks = ((DESSERT_MAXEXTDATALEN - sizeof(struct nhdp_hello_hdr)) / sizeof(struct nhdp_address_block));
            ext_size = num_address_blocks * sizeof(struct nhdp_address_block) + sizeof(struct nhdp_hello_hdr);
        }
        dessert_msg_addext(msg, &ext, NHDP_HELLO_EXT, ext_size);
        /* Append the header and all address blocks to our extension. */
        nhdp_hello_hdr = (struct nhdp_hello_hdr*) ext->data;
        nhdp_hello_hdr->num_address_blocks  = HASH_COUNT(ab_buffer);
        nhdp_hello_hdr->validity_time       = _timeval_get_ms(&H_HOLD_TIME);
        nhdp_hello_hdr->willingness         = WILLINGNESS;
        ext_ptr = (uint8_t*) ext->data;
        ext_ptr += sizeof(struct nhdp_hello_hdr);
        addr_block = (struct nhdp_address_block*) ext_ptr;
        /* add all address blocks from ab_buffer to the nhdp extensions */
        block_counter = 0;
        HASH_ITER(hh, ab_buffer, ab_buffer_item, ab_buffer_item_tmp) {
            if(block_counter == num_address_blocks) {
                ext_size = HASH_COUNT(ab_buffer) * sizeof(struct nhdp_address_block);
                if(ext_size > DESSERT_MAXEXTDATALEN) {
                    num_address_blocks = DESSERT_MAXEXTDATALEN / sizeof(struct nhdp_address_block);
                    ext_size = num_address_blocks * sizeof(struct nhdp_address_block);
                }
                dessert_msg_addext(msg, &ext, NHDP_HELLO_EXT, ext_size);
                block_counter = 0;
                addr_block = (struct nhdp_address_block*) ext->data;
            }
            memcpy(&addr_block->address, &ab_buffer_item->AB_addr, ETH_ALEN);
            addr_block->flags = ab_buffer_item->AB_flags;
            dessert_debug("AB_ADDR: "MAC" FLAGS: %x added.", EXPLODE_ARRAY6(addr_block->address), addr_block->flags);
            addr_block++;
            block_counter++;
            HASH_DEL(ab_buffer, ab_buffer_item);
            free(ab_buffer_item);
        }
        logNhdpHelloTx(msg, 0, NULL, local_iface, _num_symmetric_neighbors(), _num_strict_2hop_neighbors(), _num_mprs(), _num_mpr_selectors());
        dessert_meshsend(msg, local_iface);
        dessert_msg_destroy(msg);
        local_iface = local_iface->next;
        dessert_debug("nhdp hello packet sent.");
        dessert_debug("                                                                                 ");
    }
    return DESSERT_PER_KEEP;
}

/* ============================= CLI =======================================*/
/* nhdp related cli functions */

/* Subtracting functions for timevals. Taken from the GNU C website. */
int _timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y) {
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

/* Add milliseconds to a timeval */
void _timeval_add_ms(struct timeval *tv, uint16_t *milliseconds) {
    tv->tv_sec = tv->tv_sec + (*milliseconds / 1000);
    tv->tv_usec = tv->tv_usec + (((*milliseconds * 1000) % 1000) * 1000);
    if (tv->tv_usec > 1000000) {
        tv->tv_usec -= 1000000;
        tv->tv_sec++;
    }
}

uint16_t _timeval_get_ms(struct timeval *tv) {
    unsigned long result;

    result = tv->tv_sec * 1000;
    result += tv->tv_usec / 1000;
    /* correct overrun */
    if(result > 65535) {
        result = 65535;
    }
    return (uint16_t) result;
}

/* ============================= OLSRv2 MPR ALGORITHM =======================================*/

uint8_t _calculate_D(N_set_t *y, dessert_meshif_t *i) {
    N2_set_t            *n2_set_match;
    N_set_addrlist_t    *address_match;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    uint8_t             found, neighbors, result;

    result = 0;
    found = 0;
    neighbors = 0;
    /* Get this interface's n2 set entry */
    HASH_FIND(hh, n2_set, &i->hwaddr, ETH_ALEN, n2_set_match);
    if(n2_set_match == NULL) {
        return 0;
    }
    /* Iterate over all n2 hop addresses of the given interface I */
    HASH_ITER(hh, n2_set_match->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
        HASH_FIND(hh, y->N_neighbor_addr_list, n2_set_tuple->N2_neighbor_iface_addr, ETH_ALEN, address_match);
        if(address_match != NULL) {
            found++;
            if(_isNeighbor(n2_set_tuple) == true) {
                neighbors++;
            }
        }
        if(address_match != NULL && _isNeighbor(n2_set_tuple) == false) {
            result++;
        }
    }
    dessert_debug("R: found: %d neighbors: %d total: %d", found, neighbors, result);
    return result;
}

uint8_t _calculate_R(N_set_t *y, dessert_meshif_t *i) {
    N_set_addrlist_t    *address_match, *address_match2;
    N2_set_t            *n2_set_match;
    N_set_t             *n_set_entry, *n_set_entry_tmp;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    uint8_t             result;
    bool                match_found;

    result = 0;
    /* Get this interface's n2 set entry */
    HASH_FIND(hh, n2_set, &i->hwaddr, ETH_ALEN, n2_set_match);
    if(n2_set_match == NULL) {
        return 0;
    }
    /* Iterate over all n2 hop addresses of the given interface I */
    HASH_ITER(hh, n2_set_match->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
        HASH_FIND(hh, y->N_neighbor_addr_list, n2_set_tuple->N2_neighbor_iface_addr, ETH_ALEN, address_match);
        if(address_match != NULL && _isNeighbor(n2_set_tuple) == false) {
            /* check if this n2 hop address is connected to i via another router, that already is an mpr */
            match_found = false;
            HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
                if(n_set_entry->N_mpr == false) {
                    continue;
                }
                HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, n2_set_tuple->N2_neighbor_iface_addr, ETH_ALEN, address_match2);
                if(address_match2 != NULL) {
                    match_found = true;
                    break;
                }
            }
            if(match_found == false) {
                result++;
            }
        }
    }
    return result;
}

void _create_mpr_ni_set(MPR_NI_set_t **mpr_ni_set, dessert_meshif_t *local_iface) {
    N_set_t             *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *address_match;
    L_set_t             *l_set_entry;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp;
    MPR_NI_set_t        *mpr_ni_set_entry;
    
    /* get the linkset entry for this local interface address */
    HASH_FIND(hh, l_set, &local_iface->hwaddr, ETH_ALEN, l_set_entry);
    if(l_set_entry == NULL) {
        dessert_debug("    Interface "MAC" has no linkset entry. No possible MPR candidates.", EXPLODE_ARRAY6(local_iface->hwaddr));
        return;
    }
    /* Iterate over all this linkset entries' tuples */
    HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
        /* Iterate over all n_set entries */
        HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
            /* Find all neighborset tuples related to the given linkset entry */
            HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, &l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, address_match);
            /* If we found a matching neighbor tuple, check if its symmetric.*/
            if(address_match != NULL && n_set_entry->N_symmetric == true && n_set_entry->N_willingness != WILL_NEVER && l_set_tuple->L_quality >= mpr_minpdr) {
                /* This neighbor tuple is symmetric. We add it to the mpr_ni_set */
                mpr_ni_set_entry = malloc(sizeof(MPR_NI_set_t));
                mpr_ni_set_entry->n_set_ptr = n_set_entry;
                mpr_ni_set_entry->r_value = _calculate_R(n_set_entry, local_iface);
                /* Not adding routers with R(Y,I) == 0, because they are not valid for mpr selection */
                if(mpr_ni_set_entry->r_value == 0) {
                    continue;
                }
                mpr_ni_set_entry->d_value = _calculate_D(n_set_entry, local_iface);
                HASH_ADD_PTR(*mpr_ni_set, n_set_ptr, mpr_ni_set_entry);
                dessert_debug("    "MAC" D: %d R: %d is possible MPR candidate. (MPR: %d)", EXPLODE_ARRAY6(n_set_entry->N_neighbor_addr_list->N_neighbor_addr), mpr_ni_set_entry->d_value, mpr_ni_set_entry->r_value, mpr_ni_set_entry->n_set_ptr->N_mpr);
            }
        }
    }
}
/* This function removes all entries from the temporary MPR_NI_set and frees memory. */
void _free_mpr_ni_set(MPR_NI_set_t **mpr_ni_set) {
    MPR_NI_set_t    *mpr_ni_set_entry, *mpr_ni_set_entry_tmp;
    
    /* Finally, free the temporary MPR_ni_set. */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
        free(mpr_ni_set_entry);
    }
}

/* This function filters out the best candidate(s) for mpr selection. */
void _filter_mpr_candidates(MPR_NI_set_t **mpr_ni_set) {
    MPR_NI_set_t    *mpr_ni_set_entry, *mpr_ni_set_entry_tmp;
    uint8_t         N_willingness_max, r_value_max, d_value_max;

    /* sort out entries with R(Y,I) <= 0 or entries that already are selected 
     * as MPR. */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if(mpr_ni_set_entry->r_value == 0 || mpr_ni_set_entry->n_set_ptr->N_mpr == true) {
            HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
            free(mpr_ni_set_entry);
        }
    }
    if(HASH_COUNT(*mpr_ni_set) == 0) {
        dessert_debug("    Warning: MPR_NI_set has no entries with R(Y,I) > 0 !");
        return;
    }
    /* Get the maximum willingness. */
    N_willingness_max = 0;
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->n_set_ptr->N_willingness > N_willingness_max) {
            N_willingness_max = (*mpr_ni_set)->n_set_ptr->N_willingness;
        }
    }
    /* Filter out neighbors below the maximum N_willingness */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->n_set_ptr->N_willingness < N_willingness_max) {
            HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
            free(mpr_ni_set_entry);
        }
    }
    if(HASH_COUNT(*mpr_ni_set) == 0) {
        dessert_debug("Empty mpr_ni_set after filtering willingness.");
        return;
    }
    /* Get the maximum R value. */
    r_value_max = 0;
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->r_value > r_value_max) {
            r_value_max = (*mpr_ni_set)->r_value;
        }
    }
    /* Filter out neighbors below the maximum R value */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->r_value < r_value_max) {
            HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
            free(mpr_ni_set_entry);
        }
    }
    if(HASH_COUNT(*mpr_ni_set) == 0) {
        dessert_debug("Empty mpr_ni_set after filtering R value.");
        return;
    }
    /* Get the maximum D value. */
    d_value_max = 0;
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->d_value > d_value_max) {
            d_value_max = (*mpr_ni_set)->d_value;
        }
    }
    /* Filter out neighbors below the maximum D value */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->d_value < d_value_max) {
            HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
            free(mpr_ni_set_entry);
        }
    }
    if(HASH_COUNT(*mpr_ni_set) == 0) {
        dessert_debug("Empty mpr_ni_set after filtering D value.");
        return;
    }
    /* Filter out neighbors with N_mpr_selector = false, if there are any with
     * N_mpr_selector = true */
    HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
        if((*mpr_ni_set)->n_set_ptr->N_mpr_selector == true) {
            HASH_ITER(hh, *mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
                if((*mpr_ni_set)->n_set_ptr->N_mpr_selector == false) {
                    HASH_DEL(*mpr_ni_set, mpr_ni_set_entry);
                    free(mpr_ni_set_entry);
                }
            }
            break;
            
        }
    }
    if(HASH_COUNT(*mpr_ni_set) == 0) {
        dessert_debug("Empty mpr_ni_set after filtering N_mpr_selector == false.");
        return;
    }
    /* Select the remaining router as MPR. If there are multiple choices, take the first from the set. */
    (*mpr_ni_set)->n_set_ptr->N_mpr = true;
    dessert_debug("    "MAC" selected as MPR. Willingness value is: %d.", EXPLODE_ARRAY6((*mpr_ni_set)->n_set_ptr->N_neighbor_addr_list->N_neighbor_addr), (*mpr_ni_set)->n_set_ptr->N_willingness);
}

bool _isNeighbor(N2_set_tuple_t *n2_tuple) {
    N_set_t *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t *address_match;

    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        if(HASH_COUNT(n_set_entry->N_neighbor_addr_list) == 0) {
            dessert_crit("Neighbor address list empty. returning.");
            return true;
        }
        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, n2_tuple->N2_2hop_addr, ETH_ALEN, address_match);
        if(address_match != NULL) {
            return true;
        }
    }
    return false;
}

dessert_per_result_t _reselect_mprs(void* data, struct timeval* scheduled, struct timeval* interval) {
    dessert_meshif_t    *local_iface;
    N_set_t             *n_set_tuple, *n_set_tuple_tmp;
    N_set_addrlist_t    *n_set_addrlist, *address_match;
    N2_set_t            *n2_set_match;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    MPR_NI_set_t        *mpr_ni_set, *mpr_ni_set_entry, *mpr_ni_set_entry_tmp;
    MPR_CONNECTED_set_t *mpr_connected_set, *mpr_connected_entry, *mpr_connected_set_match, *mpr_connected_set_item, *mpr_connected_set_item_tmp;

    mpr_ni_set = NULL;
    mpr_connected_set = NULL;
    dessert_debug("  Reselecting MPRs...");
    /* Set mpr false in all neighbor tuples if they are not symmetric AND have a
     * willingness of WILL_ALWAYS. */
    HASH_ITER(hh1, n_set, n_set_tuple, n_set_tuple_tmp) {
        n_set_tuple->N_mpr = false;
        if(n_set_tuple->N_symmetric == true && n_set_tuple->N_willingness == WILL_ALWAYS) {
            n_set_tuple->N_mpr = true;
        }
    }
    /* Iterate over all local interfaces */
    local_iface = dessert_meshiflist_get();
    while(local_iface != NULL) {
        /* create this interfaces N(I) set (set of symmetric one hop neighbors) */
        _create_mpr_ni_set(&mpr_ni_set, local_iface);
        if(HASH_COUNT(mpr_ni_set) == 0) {
            dessert_debug("    "MAC" (local) skipping (no MPR candidates).", EXPLODE_ARRAY6(local_iface->hwaddr));
            local_iface = local_iface->next;
            continue;
        }
        /* Get the current interface's n2 set */
        HASH_FIND(hh, n2_set, &local_iface->hwaddr, ETH_ALEN, n2_set_match);
        if(n2_set_match == NULL) {
            dessert_debug("    "MAC" (local) skipping (no 2-hop neighbors)", EXPLODE_ARRAY6(local_iface->hwaddr));
            local_iface = local_iface->next;
            continue;
        }
        /* OLSRv2 MPR: A.2.1 */
        /* Iterate through all N2_2hop_addr entries for that interface */
        HASH_ITER(hh, n2_set_match->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            /* If this 2-hop address is also a 1-hop neighbor, ignore it */
            if(_isNeighbor(n2_set_tuple) == true) {
                //dessert_debug("      "MAC" (2-hop) skipping. (neighbor).", EXPLODE_ARRAY6(n2_set_tuple->N2_2hop_addr));
                continue;
            }
            /* first, add the currently selected 2-hop address to the temporary set */
            HASH_FIND(hh, mpr_connected_set, n2_set_tuple->N2_2hop_addr, ETH_ALEN, mpr_connected_set_match);
            if(mpr_connected_set_match == NULL) {
                mpr_connected_entry = malloc(sizeof(MPR_CONNECTED_set_t));
                memcpy(&mpr_connected_entry->n2_addr, &n2_set_tuple->N2_2hop_addr, ETH_ALEN);
                mpr_connected_entry->counter = 0;
                mpr_connected_entry->mpr_ni_set_ptr = NULL;
                HASH_ADD(hh, mpr_connected_set, n2_addr, ETH_ALEN, mpr_connected_entry);
                mpr_connected_set_match = mpr_connected_entry;
            }
            /* Iterate over all mpr candidates */
            HASH_ITER(hh, mpr_ni_set, mpr_ni_set_entry, mpr_ni_set_entry_tmp) {
                n_set_addrlist = (N_set_addrlist_t*) mpr_ni_set_entry->n_set_ptr->N_neighbor_addr_list;
                HASH_FIND(hh, n_set_addrlist, n2_set_tuple->N2_neighbor_iface_addr, ETH_ALEN, address_match);
                if(address_match != NULL) {
                    (mpr_connected_set_match->counter)++;
                    mpr_connected_set_match->mpr_ni_set_ptr = mpr_ni_set_entry;
                }
            }
        }
        // TODO: check what to do with routers not having ANY 2-hop address connected to them
        HASH_ITER(hh, mpr_connected_set, mpr_connected_set_item, mpr_connected_set_item_tmp) {
            if(mpr_connected_set_item->mpr_ni_set_ptr != NULL) {
                if(mpr_connected_set_item->mpr_ni_set_ptr->n_set_ptr->N_mpr == false && mpr_connected_set_item->counter == 1) {
                    mpr_connected_set_item->mpr_ni_set_ptr->n_set_ptr->N_mpr = true;
                    dessert_debug("      "MAC" (1-hop) forced as MPR. (bottleneck to "MAC")", EXPLODE_ARRAY6(mpr_connected_set_item->mpr_ni_set_ptr->n_set_ptr->N_neighbor_addr_list->N_neighbor_addr), EXPLODE_ARRAY6(mpr_connected_set_item->n2_addr));
                }
            }
            HASH_DEL(mpr_connected_set, mpr_connected_set_item);
            free(mpr_connected_set_item);
        }

        /* OLSRv2 MPR: A.2.2 */
        /* clean up the mpr ni set regarding different categories */
        _filter_mpr_candidates(&mpr_ni_set);
        _free_mpr_ni_set(&mpr_ni_set);
        local_iface = local_iface->next;
    }
    dessert_debug("  Reselection of MPRs finished.");
}

/* ============================= CLI =======================================*/

int cli_show_nhdp_linkset(struct cli_def *cli, char *command, char *argv[], int argc) {
    L_set_t *l_set_entry, *l_set_entry_tmp;
    L_set_tuple_t *l_set_tuple, *l_set_tuple_tmp;
    struct timeval current_time, heard_time, sym_time, l_time;
    char *heard_time_str, *sym_time_str, *l_time_str;
    uint8_t size;

    pthread_mutex_lock(&all_sets_lock);

    size = 50;
    cli_print(cli, "+---------------------------------       LINKSET       ---------------------------------------------------------+---------+");
    cli_print(cli, "+-------------------+-------------------+------------+------------+------------+-------+---------+--------------+---------+");
    cli_print(cli, "| LOCAL_IFACE       | N_IFACE_ADDR      | HEARD TIME |  SYM TIME  |  EXP TIME  | LOST  | PENDING | MPR SELECTOR | QUALITY |");
    cli_print(cli, "+-------------------+-------------------+------------+------------+------------+-------+---------+--------------+---------+");
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
            gettimeofday(&current_time, NULL);
            _timeval_subtract(&heard_time, &l_set_tuple->L_HEARD_time, &current_time);
            _timeval_subtract(&sym_time, &l_set_tuple->L_SYM_time, &current_time);
            _timeval_subtract(&l_time, &l_set_tuple->L_time, &current_time);
            heard_time_str = malloc(size);
            sym_time_str = malloc(size);
            l_time_str = malloc(size);
            if(heard_time.tv_sec < 0) {
                snprintf(heard_time_str, size, "EXPIRED");
            } else {
                snprintf(heard_time_str, size, "%3ld.%ld", heard_time.tv_sec, heard_time.tv_usec);
            }
            if(sym_time.tv_sec < 0) {
                snprintf(sym_time_str, size, "EXPIRED");
            } else {
                snprintf(sym_time_str, size, "%3ld.%ld", sym_time.tv_sec, sym_time.tv_usec);
            }
            if(l_time.tv_sec < 0) {
                snprintf(l_time_str, size, "EXPIRED");
            } else {
                snprintf(l_time_str, size, "%3ld.%ld", l_time.tv_sec, l_time.tv_usec);
            }
            cli_print(cli, "| "MAC" | "MAC" | %10s | %10s | %10s | %5s | %7s | %12s | %3f|", EXPLODE_ARRAY6(l_set_entry->L_local_iface_addr),
                                                                              EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr),
                                                                              heard_time_str,
                                                                              sym_time_str,
                                                                              l_time_str,
                                                                              l_set_tuple->L_lost ? "true" : "false",
                                                                              l_set_tuple->L_pending ? "true" : "false",
                                                                              l_set_tuple->L_mpr_selector ? "true":"false",
                                                                              l_set_tuple->L_quality);
            free(heard_time_str);
            free(sym_time_str);
        }
    }
    cli_print(cli, "+-------------------+-------------------+------------+------------+------------+-------+---------+--------------+---------+");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_show_nhdp_neighborset(struct cli_def *cli, char *command, char *argv[], int argc) {
    N_set_t             *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *n_set_tuple, *n_set_tuple_tmp;
    uint8_t             router_id;
    
    pthread_mutex_lock(&all_sets_lock);
    cli_print(cli, "+----------------------    NEIGHBORSET    ------------------------+");
    cli_print(cli, "+-----------+-------------------+-----------+---------+-----------+");
    cli_print(cli, "| ROUTER_ID | NEIGHBOR ADDRESS  | SYMMETRIC |   MPR   | WILLINGN. |");
    cli_print(cli, "+-----------+-------------------+-----------+---------+-----------+");
    router_id = 0;
    HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
        HASH_ITER(hh, n_set_entry->N_neighbor_addr_list, n_set_tuple, n_set_tuple_tmp) {
            cli_print(cli, "| %9d | "MAC" |  %7s  |  %5s  | %9d |",   router_id,
                                                                EXPLODE_ARRAY6(n_set_tuple->N_neighbor_addr),
                                                                n_set_entry->N_symmetric ? "true":"false",
                                                                n_set_entry->N_mpr ? "true":"false",
                                                                n_set_entry->N_willingness);
        }
        router_id++;
    }
    cli_print(cli, "+-----------+-------------------+-----------+---------+-----------+");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_show_nhdp_n2set(struct cli_def *cli, char *command, char *argv[], int argc) {
    N2_set_t        *n2_set_entry, *n2_set_entry_tmp;
    N2_set_tuple_t  *n2_set_tuple, *n2_set_tuple_tmp;
    struct timeval  current_time, validity_time;
    char            *validity_time_str;
    uint8_t         size;

    size = 50;
    pthread_mutex_lock(&all_sets_lock);
    cli_print(cli, "+-----------------------------2-HOP SET-------------------------------------+");
    cli_print(cli, "+-------------------+-------------------+-------------------+---------------+");
    cli_print(cli, "|     LOCAL_IF      | NEIGHBOR ADDRESS  |   2-HOP ADDRESS   | VALIDITY TIME |");
    cli_print(cli, "+-------------------+-------------------+-------------------+---------------+");
    HASH_ITER(hh, n2_set, n2_set_entry, n2_set_entry_tmp) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            gettimeofday(&current_time, NULL);
            _timeval_subtract(&validity_time, &n2_set_tuple->N2_time, &current_time);
            validity_time_str = malloc(size);
            if(validity_time.tv_sec < 0) {
                snprintf(validity_time_str, size, "EXPIRED");
            } else {
                snprintf(validity_time_str, size, "%3ld.%ld", validity_time.tv_sec, validity_time.tv_usec);
            }
            cli_print(cli, "| "MAC" | "MAC" | "MAC" | %13s |", EXPLODE_ARRAY6(n2_set_entry->N2_local_iface_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_neighbor_iface_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_2hop_addr), validity_time_str);
            free(validity_time_str);
        }
    }
    cli_print(cli, "+---------------------------------------------------------------------------+");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_show_nhdp_strict_n2set(struct cli_def *cli, char *command, char *argv[], int argc) {
    N2_set_t        *n2_set_entry, *n2_set_entry_tmp;
    N2_set_tuple_t  *n2_set_tuple, *n2_set_tuple_tmp;
    struct timeval  current_time, validity_time;
    char            *validity_time_str;
    uint8_t         size;

    size = 50;
    pthread_mutex_lock(&all_sets_lock);
    cli_print(cli, "+------------------------- STRICT 2-HOP SET --------------------------------+");
    cli_print(cli, "+-------------------+-------------------+-------------------+---------------+");
    cli_print(cli, "|     LOCAL_IF      | NEIGHBOR ADDRESS  |   2-HOP ADDRESS   | VALIDITY TIME |");
    cli_print(cli, "+-------------------+-------------------+-------------------+---------------+");
    HASH_ITER(hh, n2_set, n2_set_entry, n2_set_entry_tmp) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            if(_isNeighbor(n2_set_tuple) == true) {
                continue;
            }
            gettimeofday(&current_time, NULL);
            _timeval_subtract(&validity_time, &n2_set_tuple->N2_time, &current_time);
            validity_time_str = malloc(size);
            if(validity_time.tv_sec < 0) {
                snprintf(validity_time_str, size, "EXPIRED");
            } else {
                snprintf(validity_time_str, size, "%3ld.%ld", validity_time.tv_sec, validity_time.tv_usec);
            }
            cli_print(cli, "| "MAC" | "MAC" | "MAC" | %13s |", EXPLODE_ARRAY6(n2_set_entry->N2_local_iface_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_neighbor_iface_addr), EXPLODE_ARRAY6(n2_set_tuple->N2_2hop_addr), validity_time_str);
            free(validity_time_str);
        }
    }
    cli_print(cli, "+---------------------------------------------------------------------------+");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_show_mprselectors(struct cli_def *cli, char *command, char *argv[], int argc) {
    L_set_t             *l_set_entry, *l_set_entry_tmp;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp;
    uint8_t             router_id;

    pthread_mutex_lock(&all_sets_lock);
    cli_print(cli, "+----  MPR SELECTORS  ---+");
    cli_print(cli, "+----+-------------------+");
    cli_print(cli, "| ID | NEIGHBOR ADDRESS  |");
    cli_print(cli, "+----+-------------------+");
    router_id = 0;
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
            if(l_set_tuple->L_mpr_selector == true) {
                cli_print(cli, "| %2d | "MAC" |",
                        router_id,
                        EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
                router_id++;
            }
        }
    }
    cli_print(cli, "+----+-------------------+");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_set_nhdp_hi(struct cli_def *cli, char *command, char *argv[], int argc) {
    uint32_t new_hi_ms;
    
    pthread_mutex_lock(&all_sets_lock);
    if(argc != 1) {
        cli_print(cli, "Please give the interval in ms as argument.\n", command);
        return CLI_ERROR;
    }
    new_hi_ms = strtoul(argv[0], NULL, 10);

    NHDP_HELLO_INTERVAL.tv_sec = 0;
    NHDP_HELLO_INTERVAL.tv_usec = new_hi_ms*1000;
    while(NHDP_HELLO_INTERVAL.tv_usec >= 1000*1000) {
        NHDP_HELLO_INTERVAL.tv_sec++;
        NHDP_HELLO_INTERVAL.tv_usec -= 1000*1000;
    }
    _stop_nhdp_hello_task();
    if(dessert_timeval2ms(&NHDP_HELLO_INTERVAL) > 0) {
        _start_nhdp_hello_task();
    }
    _reset_pdr_packet_traps();
    
    print_log(LOG_NOTICE, cli, "setting NHDP_HELLO_INTERVAL to %d ms", (int) (NHDP_HELLO_INTERVAL.tv_sec*1000 + NHDP_HELLO_INTERVAL.tv_usec/1000));
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_set_nhdp_ht(struct cli_def *cli, char *command, char *argv[], int argc) {
    uint32_t interval_ms ;
    
    pthread_mutex_lock(&all_sets_lock);
    if(argc != 1) {
        cli_print(cli, "Please give the interval in ms as argument.\n", command);
        return CLI_ERROR;
    }
    interval_ms = strtoul(argv[0], NULL, 10);

    H_HOLD_TIME.tv_sec = 0;
    H_HOLD_TIME.tv_usec = interval_ms*1000;
    while(H_HOLD_TIME.tv_usec >= 1000*1000) {
        H_HOLD_TIME.tv_sec++;
        H_HOLD_TIME.tv_usec -= 1000*1000;
    }
    N_HOLD_TIME = H_HOLD_TIME;
    L_HOLD_TIME = H_HOLD_TIME;
    print_log(LOG_NOTICE, cli, "Setting NHDP_HT to %d ms.", (int) (H_HOLD_TIME.tv_sec*1000 + H_HOLD_TIME.tv_usec/1000));
    print_log(LOG_NOTICE, cli, "Warning! After changing the HOLD_TIME, NHDP needs some time to be reliable again!");
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_set_minpdr(struct cli_def *cli, char *command, char *argv[], int argc) {
    pthread_mutex_lock(&all_sets_lock);
    if(argc != 1) {
        cli_print(cli, "Please give the pdr value as argument.\n", command);
        return CLI_ERROR;
    }
    mpr_minpdr = atof(argv[0]);
    print_log(LOG_NOTICE, cli, "MIN_PDR now set to %f", mpr_minpdr);
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_set_mpr_ri(struct cli_def *cli, char *command, char *argv[], int argc) {
    int ms;
    
    pthread_mutex_lock(&all_sets_lock);
    if(argc != 1) {
        cli_print(cli, "Please give the interval in ms as argument.\n", command);
        return CLI_ERROR;
    }
    uint32_t interval_ms = strtoul(argv[0], NULL, 10);

    MPR_RESELECT_INTERVAL.tv_sec = 0;
    MPR_RESELECT_INTERVAL.tv_usec = interval_ms*1000;
    while(MPR_RESELECT_INTERVAL.tv_usec >= 1000*1000) {
        MPR_RESELECT_INTERVAL.tv_sec++;
        MPR_RESELECT_INTERVAL.tv_usec -= 1000*1000;
    }

    ms = (int) (MPR_RESELECT_INTERVAL.tv_sec*1000 + MPR_RESELECT_INTERVAL.tv_usec/1000);
    if(ms == 0 && reselect_mprs_task != NULL) {
        _stop_mpr_selection_task();
        print_log(LOG_NOTICE, cli, "MPR selection now event based.");
    } else if (ms == 0 && reselect_mprs_task == NULL) {
        print_log(LOG_NOTICE, cli, "MPR selection is already event based.");
    }
    if(ms > 0 && reselect_mprs_task == NULL) {
        _start_mpr_selection_task();
        print_log(LOG_NOTICE, cli, "MPR selection now triggered every %d ms", ms);
    }
    pthread_mutex_unlock(&all_sets_lock);
    return CLI_OK;
}

int cli_flush_hashes(struct cli_def *cli, char *command, char *argv[], int argc) {
    N_set_t             *n_set_item, *n_set_item_tmp;
    N_set_addrlist_t    *n_set_addrlist_item, *n_set_addrlist_item_tmp;
    N2_set_t            *n2_set_item, *n2_set_item_tmp;
    N2_set_tuple_t      *n2_set_tuple_item, *n2_set_tuple_item_tmp;
    L_set_t             *l_set_item, *l_set_item_tmp;
    L_set_tuple_t       *l_set_tuple_item, *l_set_tuple_item_tmp;
    NL_set_t            *nl_set_item, *nl_set_item_tmp;
    
    pthread_mutex_lock(&all_sets_lock);
    HASH_ITER(hh1, n_set, n_set_item, n_set_item_tmp) {
        HASH_ITER(hh, n_set_item->N_neighbor_addr_list, n_set_addrlist_item, n_set_addrlist_item_tmp) {
            HASH_DEL(n_set_item->N_neighbor_addr_list, n_set_addrlist_item);
            free(n_set_addrlist_item);
        }
        HASH_DELETE(hh1, n_set, n_set_item);
        free(n_set_item);
    }
    
    HASH_ITER(hh, n2_set, n2_set_item, n2_set_item_tmp) {
        HASH_ITER(hh, n2_set_item->N2_set_tuple_list, n2_set_tuple_item, n2_set_tuple_item_tmp) {
            HASH_DEL(n2_set_item->N2_set_tuple_list, n2_set_tuple_item);
            free(n2_set_tuple_item);
        }
        HASH_DEL(n2_set, n2_set_item);
        free(n2_set_item);
    }
    
    HASH_ITER(hh, l_set, l_set_item, l_set_item_tmp) {
        HASH_ITER(hh, l_set_item->l_set_tuple_list, l_set_tuple_item, l_set_tuple_item_tmp) {
            HASH_DEL(l_set_item->l_set_tuple_list, l_set_tuple_item);
            free(l_set_tuple_item);
        }
        HASH_DEL(l_set, l_set_item);
        free(l_set_item);
    }
    
    HASH_ITER(hh, nl_set, nl_set_item, nl_set_item_tmp) {
        HASH_DEL(nl_set, nl_set_item);
        free(nl_set_item);
    }
    pthread_mutex_unlock(&all_sets_lock);
    print_log(LOG_NOTICE, cli, "Successfully flushed all hashes.");
    return CLI_OK;
}

void _check_constraints() {
    L_set_t *l_set_entry, *l_set_entry_tmp;
    N_set_t *n_set_entry, *n_set_entry_tmp;
    N_set_addrlist_t    *n_neighbor_addr;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp;
    dessert_meshif_t    *local_iface;
    struct timeval      current_time;
    bool                ok, heard_not_expired, symmetric;
    uint8_t             L_status;


    dessert_debug("Checking constraints...");
    /* A linkset tuple's address list MUST NOT match with a local interface address. */
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        local_iface = dessert_meshiflist_get();
        while(local_iface != NULL) {
            HASH_FIND(hh, l_set_entry->l_set_tuple_list, &local_iface->hwaddr, ETH_ALEN, l_set_tuple);
            if(l_set_tuple != NULL) {
                dessert_debug("NHDP CONSTRAINT ERROR: Local iface address found in linkset! ("MAC")", EXPLODE_ARRAY6(local_iface->hwaddr));
            }
            local_iface = local_iface->next;
        }
        gettimeofday(&current_time, NULL);
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
           if(_check_expired(&l_set_tuple->L_HEARD_time) == 0) {
               ok = false;
               HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
                   HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_neighbor_addr);
                   if(n_neighbor_addr != NULL) {
                       ok = true;
                   }
               }
               if(ok == false) {
                   dessert_debug("NHDP CONSTRAINT ERROR: "MAC" is missing a neighborset entry.", EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
               }
           }
           if(timercmp(&l_set_tuple->L_HEARD_time, &l_set_tuple->L_time, >) == true) {
               dessert_debug("NHDP CONSTRAINT ERROR: L_HEARD_time > L_time!");
           }
           if(timercmp(&l_set_tuple->L_SYM_time, &l_set_tuple->L_HEARD_time, >) == true) {
               if(_check_expired(&l_set_tuple->L_SYM_time) == 0 && _check_expired(&l_set_tuple->L_HEARD_time) == 0) {
                   dessert_debug("NHDP CONSTRAINT ERROR: L_SYM_time > L_HEARD_time. Both not expired.");
               }
           }
        }
    }

    local_iface = dessert_meshiflist_get();
    while(local_iface != NULL) {
        HASH_ITER(hh1, n_set, n_set_entry, n_set_entry_tmp) {
            HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, local_iface->hwaddr, ETH_ALEN, n_neighbor_addr);
            if(n_neighbor_addr != NULL) {
                dessert_debug("NHDP CONSTRAINT ERROR: Local address "MAC" found in neighborset.", EXPLODE_ARRAY6(local_iface->hwaddr));
            }
            if(n_set_entry->N_symmetric == true) {
                ok = false;
                HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
                    HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
                        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_neighbor_addr);
                        L_status = _lset_get_linkstatus(l_set_tuple);
                        if(n_neighbor_addr != NULL && L_status == SYMMETRIC) {
                            ok = true;
                        }
                    }
                }
                if(ok == false) {
                    dessert_debug("NHDP CONSTRAINT ERROR: Link NOT symmetric in linkset, but symmetric in neighborset.");
                }
            } else if(n_set_entry->N_symmetric == false) {
                symmetric = false;
                heard_not_expired = false;
                HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
                    HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
                        HASH_FIND(hh, n_set_entry->N_neighbor_addr_list, l_set_tuple->L_neighbor_iface_addr, ETH_ALEN, n_neighbor_addr);
                        if(n_neighbor_addr != NULL) {
                            L_status = _lset_get_linkstatus(l_set_tuple);
                            if(L_status == SYMMETRIC) {
                                symmetric = true;
                                break;
                            }
                            if(_check_expired(&l_set_tuple->L_HEARD_time) == 0) {
                                heard_not_expired = true;
                            }
                        }
                    }
                    if(symmetric == true) {
                        break;
                    }
                }
                if(heard_not_expired == false || symmetric == true) {
                    dessert_debug("NHDP CONSTRAINT ERROR: N_symmetric == false, but l_tuple status is: HEARD EXPIRED: %s SYMMETRIC: %s", heard_not_expired ? "true": "false", symmetric ? "true" : "false");
                }
            }
        }
        local_iface = local_iface->next;
    }
}

dessert_per_result_t _log_mpr_selector_string(void* data, struct timeval* scheduled, struct timeval* interval) {
    L_set_t             *l_set_entry, *l_set_entry_tmp;
    L_set_tuple_t       *l_set_tuple, *l_set_tuple_tmp;
    char                *new_mpr_str;
    char                mpr_selector_str[1024], hostname[20];
    FILE                *f;
    
    f = fopen("/proc/sys/kernel/hostname", "r");
    if(f) {
        fscanf(f, "%20s", hostname);
        fclose(f);
    }
    pthread_mutex_lock(&all_sets_lock);
    strcpy(mpr_selector_str, "[mprsel] ");
    HASH_ITER(hh, l_set, l_set_entry, l_set_entry_tmp) {
        HASH_ITER(hh, l_set_entry->l_set_tuple_list, l_set_tuple, l_set_tuple_tmp) {
            if(l_set_tuple->L_mpr_selector == true) {
                new_mpr_str = malloc(19);
                snprintf(new_mpr_str, 19, MAC" ", EXPLODE_ARRAY6(l_set_tuple->L_neighbor_iface_addr));
                strcat(mpr_selector_str, new_mpr_str);
                free(new_mpr_str);
            }
        }
    }
    dessert_info("%s", mpr_selector_str);
    pthread_mutex_unlock(&all_sets_lock);
    return DESSERT_PER_KEEP;
}

dessert_per_result_t _log_n2_set_string(void* data, struct timeval* scheduled, struct timeval* interval) {
    N2_set_t            *n2_set_entry, *n2_set_entry_tmp;
    N2_set_tuple_t      *n2_set_tuple, *n2_set_tuple_tmp;
    N2_set_addrlist_t   *n2_addrlist, *n2_addrlist_entry, *n2_addrlist_item, *n2_addrlist_item_tmp, *result;
    char                *new_n2_str;
    char                n2_str[1024], hostname[20];
    FILE                *f;

    f = fopen("/proc/sys/kernel/hostname", "r");
    if(f) {
        fscanf(f, "%20s", hostname);
        fclose(f);
    }
    
    n2_addrlist = NULL;
    
    pthread_mutex_lock(&all_sets_lock);
    strcpy(n2_str, "[n2] ");
    uint8_t counter = 0;
    HASH_ITER(hh, n2_set, n2_set_entry, n2_set_entry_tmp) {
        HASH_ITER(hh, n2_set_entry->N2_set_tuple_list, n2_set_tuple, n2_set_tuple_tmp) {
            if(_isNeighbor(n2_set_tuple) == true) {
                continue;
            }
            n2_addrlist_entry = malloc(sizeof(N2_set_addrlist_t));
            n2_addrlist_entry->key_ptr = &n2_set_tuple->N2_2hop_addr;
            HASH_FIND(hh, n2_addrlist, &n2_set_tuple->N2_2hop_addr, ETH_ALEN, result);
            if(result == NULL) {
                HASH_ADD_KEYPTR(hh, n2_addrlist, n2_addrlist_entry->key_ptr, ETH_ALEN, n2_addrlist_entry);
            }
        }
    }
    HASH_ITER(hh, n2_addrlist, n2_addrlist_item, n2_addrlist_item_tmp) {
        new_n2_str = malloc(19);
        snprintf(new_n2_str, 19, MAC" ", EXPLODE_ARRAY6(n2_addrlist_item->key_ptr));
        strcat(n2_str, new_n2_str);
        counter++;
        if(counter == 20) {
            dessert_info("%s", n2_str);
            n2_str[0] = "\0";
            strcpy(n2_str, "[n2] ");
            counter = 0;
        }
        free(new_n2_str);
    }
    if(counter < 20) {
        dessert_info("%s", n2_str);
    }
    pthread_mutex_unlock(&all_sets_lock);
    return DESSERT_PER_KEEP;
}