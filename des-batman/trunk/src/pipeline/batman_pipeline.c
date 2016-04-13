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
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include "batman_pipeline.h"
#include "../helper.h"
#include "../database/batman_database.h"
#include "../database/rl_seq_t/rl_seq.h"

// ------------------------------------- help functions -----------------------------------

/**
 * Check whether I am in precursor list of this OGM.
 * (in that case silently DROP without processing)
 */
int _check_precursors(struct batman_msg_ogm* ogm_ext) {
    int i;

    for(i = ogm_ext->precursors_count - 1; i >= 0; i--) {
        // start to search from !END! It is very presumably that i am the last entry in the list
        uint8_t* el_start_address = ogm_ext->precursor_list + ETH_ALEN * i;

        if(memcmp(el_start_address, dessert_l25_defsrc, ETH_ALEN) == 0) {
            return true;
        }
    }

    return false;
}

void _add_myself_to_precursors(struct batman_msg_ogm* ogm_ext) {
    if(ogm_ext->precursors_count < OGM_PREC_LIST_SIZE) {
        // increment size and write my default address to end of list
        uint8_t* el_start_address = ogm_ext->precursor_list + ETH_ALEN * ogm_ext->precursors_count;
        memcpy(el_start_address, dessert_l25_defsrc, ETH_ALEN);
        ogm_ext->precursors_count++;
    }
    else {   // precursor list is full
        // delete first element and move all elements to begin of the list.
        // add my default address to end of list.
        if(OGM_PREC_LIST_SIZE > 1) {
            memmove(ogm_ext->precursor_list, ogm_ext->precursor_list + ETH_ALEN, ETH_ALEN *(OGM_PREC_LIST_SIZE - 1));
        }

        memcpy(ogm_ext->precursor_list + (OGM_PREC_LIST_SIZE - 1)*ETH_ALEN, dessert_l25_defsrc, ETH_ALEN);
    }
}

pthread_rwlock_t rlseqlock = PTHREAD_RWLOCK_INITIALIZER;

// ------------------------------------- callbacks ----------------------------------------

dessert_cb_result batman_drop_errors(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    // drop packets sent by myself.
    if(proc->lflags & DESSERT_RX_FLAG_L2_SRC) {
        return DESSERT_MSG_DROP;
    }

    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
        dessert_ext_t* ogm_ext;

        if(dessert_msg_getext(msg, &ogm_ext, OGM_EXT_TYPE, 0) != 0) {
            struct batman_msg_ogm* ogm = (struct batman_msg_ogm*) ogm_ext->data;

            if(ogm->flags & BATMAN_OGM_DFLAG
                && (memcmp(msg->l2h.ether_dhost, iface->hwaddr, ETH_ALEN) == 0)) {
                // This is an OGM rebroadcastet by my 1hop neighbor for this interface
                // -> link to this neighbor is bidirectional -> capture, then DROP
                batman_db_wlock();
                batman_db_cap2Dneigh(msg->l2h.ether_shost, iface);
                batman_db_unlock();
            }
        }

        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

dessert_cb_result batman_handle_ogm(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* ogm_ext;

    if(dessert_msg_getext(msg, &ogm_ext, OGM_EXT_TYPE, 0) != 0) {
        struct batman_msg_ogm* ogm = (struct batman_msg_ogm*) ogm_ext->data;
        struct ether_header* l25h = dessert_msg_getl25ether(msg);

        dessert_debug("--- OGM%u from " MAC , ogm->sequence_num, EXPLODE_ARRAY6(l25h->ether_shost));

        // if unidirectional flag is set ->
        // switch to directed flag and send this OGM back to originator
        if(ogm->flags & BATMAN_OGM_UFLAG) {
            ogm->flags = (ogm->flags & ~BATMAN_OGM_UFLAG) | BATMAN_OGM_DFLAG;
            memcpy(msg->l2h.ether_dhost, msg->l2h.ether_shost, ETH_ALEN);
            dessert_meshsend(msg, iface);
            // reset dhost to broadcast
            memset(msg->l2h.ether_dhost, 255, ETH_ALEN);
        }

        // delete Dflag in all ways
        ogm->flags = ogm->flags & ~BATMAN_OGM_DFLAG;

        // if not from bidirectional neighbor -> silently drop!
        batman_db_wlock();
        int bd = batman_db_check2Dneigh(msg->l2h.ether_shost, iface);
        batman_db_unlock();

        if(bd != true) {
            dessert_debug("--- OGM%u drop as not over bidirectional connection (" MAC ")", ogm->sequence_num, EXPLODE_ARRAY6(msg->l2h.ether_shost));
            return DESSERT_MSG_DROP;
        }

        batman_db_wlock();
        int last_seq_num = batman_db_getroutesn(l25h->ether_shost);

        // if reset flag is set -> delete old destination
        if(ogm->flags & BATMAN_OGM_RFLAG) {
            if(last_seq_num >= 0 && (last_seq_num - ogm->sequence_num >= OGM_RESET_COUNT)) {
                dessert_debug("--- " MAC " - was restarted", EXPLODE_ARRAY6(l25h->ether_shost));
                batman_db_deleteroute(l25h->ether_shost);
            }
        }

        batman_db_unlock();

        if(ogm_precursor_mode == false) {  // precursor list not used.
            // capture and re-send OGM only if incoming OGM is newer
            // HINT: Process only OGM with greater seq_num to avoid routing loops.
            if((last_seq_num == -1) || hf_seq_comp_i_j(last_seq_num, ogm->sequence_num) < 0) {
                // add or change route to destination (l25h->ether_shost)
                batman_db_wlock();
                batman_db_captureroute(l25h->ether_shost, iface, time(0), msg->l2h.ether_shost, ogm->sequence_num);
                batman_db_unlock();
            }
        }
        else {
            // capture all OGM not processed by me.
            // We assume that the OGM with known sequence number but
            // not containing my default address in precursor list were not processed.
            // Re-send OGMs only with newer seq_num to avoid routing loops
            if((_check_precursors(ogm) == false)
                && ((hf_seq_comp_i_j(ogm->sequence_num, last_seq_num) >= 0) || (last_seq_num == -1))) {
                // also: don't process old OGM
                // add or change route to destination (l25h->ether_shost)
                batman_db_wlock();
                batman_db_captureroute(l25h->ether_shost, iface, time(0), msg->l2h.ether_shost, ogm->sequence_num);
                batman_db_unlock();
                _add_myself_to_precursors(ogm);
            }
        }

        // check re-broadcast conditions
        int allow_rebroadcast = false;

        if(resend_ogm_always == true) {
            allow_rebroadcast = true;
        }
        else {
            // if OGM received over best next hop toward OGM originator -> re-broadcast
            uint8_t ether_best_prev_hop[ETH_ALEN];
            dessert_meshif_t* local_iface_towards_originator;
            batman_db_rlock();
            int result = batman_db_getbestroute(l25h->ether_shost, &local_iface_towards_originator, ether_best_prev_hop);
            batman_db_unlock();

            if((result == true)
                && (memcmp(ether_best_prev_hop, msg->l2h.ether_shost, ETH_ALEN) == 0)
                && (local_iface_towards_originator == iface)) {
                allow_rebroadcast = true;
            }
        }

        if(allow_rebroadcast == true) {
            batman_db_wlock();
            int not_rebroadcastet = batman_db_addogmseq(l25h->ether_shost, ogm->sequence_num);
            batman_db_unlock();

            if(not_rebroadcastet == true) {
                if(ogm_precursor_mode == true) {
                    _add_myself_to_precursors(ogm);
                }

                dessert_meshsend_fast_randomized(msg);
            }
        }

        return DESSERT_MSG_DROP;
    }
    else {
        return DESSERT_MSG_KEEP;
    }
}

dessert_cb_result batman_fwd2dest(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    struct ether_header* l25h = dessert_msg_getl25ether(msg);
    dessert_ext_t* rl_ext = NULL;
    uint32_t rl_seq_num = 0;
    uint8_t rl_hop_count = 0;

    if(dessert_msg_getext(msg, &rl_ext, RL_EXT_TYPE, 0) != 0) {
        struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
        rl_seq_num = rl_data->seq_num;
        pthread_rwlock_wrlock(&rlseqlock);
        int pk = rl_check_seq(l25h->ether_shost, l25h->ether_dhost, rl_seq_num, time(0));
        pthread_rwlock_unlock(&rlseqlock);

        if(pk == true) {
            // this packet was already processed
            dessert_debug("DUP! from " MAC " to " MAC " hops %i", EXPLODE_ARRAY6(l25h->ether_shost), EXPLODE_ARRAY6(l25h->ether_dhost), rl_data->hop_count + 1);
            return DESSERT_MSG_DROP;
        }

        pthread_rwlock_wrlock(&rlseqlock);
        rl_add_seq(l25h->ether_shost, l25h->ether_dhost, rl_seq_num, time(0));
        pthread_rwlock_unlock(&rlseqlock);

        if(rl_data->hop_count != 255) {
            rl_hop_count = ++rl_data->hop_count;
        }
    }

    if(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
        || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) { // BROADCAST
        // re-send broadcast only if not previously re-broadcastet
        dessert_ext_t* brc_ext;

        if(dessert_msg_getext(msg, &brc_ext, BROADCAST_EXT_TYPE, 0) != 0) {
            struct batman_msg_brc* brc_data = (struct batman_msg_brc*)brc_ext->data;
            batman_db_wlock();
            int result = batman_db_addbrcid(l25h->ether_shost, brc_data->id);
            batman_db_unlock();

            if(result == true) {
                dessert_meshsend_fast(msg, NULL);
                return DESSERT_MSG_KEEP;
            }
            else {
                return DESSERT_MSG_DROP;
            }
        }

        return DESSERT_MSG_KEEP;
    }
    else if(((proc->lflags & DESSERT_RX_FLAG_L2_DST && !(proc->lflags & DESSERT_RX_FLAG_L2_OVERHEARD)) ||
             proc->lflags & DESSERT_RX_FLAG_L2_BROADCAST) &&
            !(proc->lflags & DESSERT_RX_FLAG_L25_DST)) { // NOT BROADCAST
        // route packets addressed direct to me(and for this interface),
        // or if next hop unknown(broadcast)
        // AND
        // not for me
        uint8_t ether_best_next_hop[ETH_ALEN];
        dessert_meshif_t* output_iface;
        // find and set NEXT HOP and output interface for message towards destination
        batman_db_rlock();
        int result = false;

        if(ogm_precursor_mode != true) {
            result = batman_db_getbestroute(l25h->ether_dhost, &output_iface, ether_best_next_hop);
        }
        else {
            struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
            result = batman_db_getbestroute_arl(l25h->ether_dhost, &output_iface, ether_best_next_hop, rl_data->precursor_if_list, &rl_data->prec_iface_count);
        }

        batman_db_unlock();

        if(result == true) {
            memcpy(msg->l2h.ether_dhost, ether_best_next_hop, ETH_ALEN);
            dessert_meshsend_fast(msg, output_iface); // HINT no lock need since no change in iftable after setup
        }

        // packet addressed not to me -> drop
        return DESSERT_MSG_DROP;
    }

    return DESSERT_MSG_KEEP;
}

// --------------------------- TUN ----------------------------------------------------------

uint16_t broadcast_id = 0;
pthread_rwlock_t brclock = PTHREAD_RWLOCK_INITIALIZER;

dessert_cb_result batman_sys2rp(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* sysif, dessert_frameid_t id) {
    uint8_t ether_best_next_hop[ETH_ALEN];
    dessert_meshif_t* output_iface;
    // find and set (if found) NEXT HOP towards destination
    struct ether_header* l25h = dessert_msg_getl25ether(msg);

    if(memcmp(l25h->ether_dhost, ether_broadcast, ETH_ALEN) == 0) {
        // next hop broadcast
        dessert_ext_t* brc_ext;
        dessert_msg_addext(msg, &brc_ext, BROADCAST_EXT_TYPE, sizeof(struct batman_msg_brc));
        struct batman_msg_brc* brc_id_data = (struct batman_msg_brc*) brc_ext->data;
        pthread_rwlock_wrlock(&brclock);
        brc_id_data->id = broadcast_id++;
        pthread_rwlock_unlock(&brclock);
        dessert_meshsend_fast(msg, NULL);
    }
    else {
        dessert_ext_t* rl_ext;
        dessert_msg_addext(msg, &rl_ext, RL_EXT_TYPE, sizeof(struct rl_seq));
        struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
        rl_data->hop_count = 0;
        rl_data->prec_iface_count = 0;

        batman_db_rlock();
        int result = false;

        if(ogm_precursor_mode != true) {
            result = batman_db_getbestroute(l25h->ether_dhost, &output_iface, ether_best_next_hop);
        }
        else {
            result = batman_db_getbestroute_arl(l25h->ether_dhost, &output_iface, ether_best_next_hop, rl_data->precursor_if_list, &rl_data->prec_iface_count);
        }

        batman_db_unlock();

        if(result == true) {
            uint32_t seq_num = 0;
            pthread_rwlock_wrlock(&rlseqlock);
            seq_num = rl_get_nextseq(dessert_l25_defsrc, l25h->ether_dhost, time(0));
            pthread_rwlock_unlock(&rlseqlock);
            rl_data->seq_num = seq_num;

            memcpy(msg->l2h.ether_dhost, ether_best_next_hop, ETH_ALEN);
            // forward to the next hop
            dessert_meshsend_fast(msg, output_iface);
        }
    }

    return DESSERT_MSG_DROP;
}

// ----------------- common callbacks ---------------------------------------------------

/**
 * Forward packets addressed to me to tun pipeline
 */
dessert_cb_result rp2sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    if((proc->lflags & DESSERT_RX_FLAG_L25_DST && !(proc->lflags & DESSERT_RX_FLAG_L25_OVERHEARD)) ||
       proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST ||
       proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
        struct ether_header* l25h = dessert_msg_getl25ether(msg);
        dessert_ext_t* rl_ext;
        uint32_t rl_seq_num = 0;
        uint8_t rl_hop_count = 0;

        if(dessert_msg_getext(msg, &rl_ext, RL_EXT_TYPE, 0) != 0) {
            struct rl_seq* rl_data = (struct rl_seq*) rl_ext->data;
            pthread_rwlock_wrlock(&rlseqlock);
            rl_add_seq(dessert_l25_defsrc, l25h->ether_shost, rl_data->seq_num, time(0));
            pthread_rwlock_unlock(&rlseqlock);
            rl_seq_num = rl_data->seq_num;
            rl_hop_count = rl_data->hop_count;
        }

        dessert_syssend_msg(msg);
    }

    return DESSERT_MSG_DROP;
}
