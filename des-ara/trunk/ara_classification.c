/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
 Extended and debugged by Bastian Blywis, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by Philipp Schmidt
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
*******************************************************************************/

#include "ara.h"
#include <unistd.h>
#include <utlist.h>


/** 
 * extension containing the path classification info. Added by sender, updated by nodes 
 */
typedef struct __attribute__((__packed__)) clsf_extension {
    uint16_t send_rate;
    uint16_t nexthops;
 } clsf_extension_t;

 enum extensions {
    CLASSIF_EXT = DESSERT_EXT_USER
 };

/** 
  * results of the path classification 
 */
typedef enum _ara_classification_status {
    _P_UNCLASSIFIED = 0,           ///< initial value
    _P_EFFICIENT,                  ///< efficient path, low loss rate
    _P_ACCEPTABLE,                 ///< acceptable path, moderate loss rate 
    _P_LOSSY,                      ///< lossy path, high paket loss
    _P_INTERR = 255                ///< internal error
} _ara_clsf_status_t ;

/**
 * Return values of the path clasification table updates
 */
typedef enum _ara_clsf_update_results {
    ARA_PCT_FAILED = 0x00,
    ARA_PCT_NEW,
    ARA_PCT_UPDATED,
    ARA_PCT_DELETED
} ara_clsf_update_res_t;

/*
 * hash map for classification results. used with sliding window - older entries are overwritten
 */
typedef struct ara_classified {
    uint8_t id;                     ///< clsf counter. After the limit is reached old entries are modified  
    uint16_t rx_packets;            ///< received packets
    uint16_t tx_packets;            ///< number of packets received and forwarded (last iteration)
    uint16_t loss_percent;          ///< packet loss in percent 
    _ara_clsf_status_t status;      ///< actual result of the path clasification
    UT_hash_handle hh;              ///< used by uthash
} ara_classified_t;

/** 
  * node disjoint path representation
 */
typedef struct ara_path {
    ara_address_t src;                  ///< source of the packet
    ara_address_t prevhop;              ///< prevhop for this route
    const dessert_meshif_t* iface;      ///< interface to reach prevhop
    uint16_t sendrate;                  ///< sending speed in packets per second
    uint16_t nexthops;                  ///< number of siblings on the packet way 
    uint16_t rx_packets;                ///< number of received packets
    uint16_t tx_packets;                ///< best expected number of packets
    uint16_t loss_percent;              ///< expected packet loss in percent
    _ara_clsf_status_t status;          ///< actual result of the path clasification
    _ara_clsf_status_t longterm_status; ///< longterm result of the path clasification
    ara_classified_t* classifications;  ///< all path classifications 
    uint8_t sw_index;                   ///< indicates where the sliding window is  
    int16_t nopkts_credit;              ///< lifetime: number of classifications without rx packets
    struct ara_path* prev;              ///< needed for doubly-linked list
    struct ara_path* next;              ///< needed for doubly-linked list
} ara_path_t;


/** 
  *path classification table entry 
 */
typedef struct ara_pcte {
    ara_address_t dst;              ///< packet destination
    ara_path_t* paths;              ///< list of all paths
    int8_t clsf_skiptimes;          ///< entry is not classified while > 0
    int16_t num_paths;              ///< number of known paths  
    UT_hash_handle hh;              ///< used by uthash
} ara_pcte_t;


/* variables */
uint8_t ara_classify = 1;                     ///< enables/disables path classification
uint8_t ara_clsf_longterm = 1;                ///< enables/disables longterm path clsf: all sliding window entries are used 
uint8_t ara_clsf_lossmin = 30;                ///< min loss in percent. Smaller loss means efficient path
uint8_t ara_clsf_lossmax = 70;                ///< max loss in percent. Larger loss means lossy path
uint8_t ara_clsf_tick_interval_s = 3;         ///< how often run the path classification in seconds
uint8_t ara_clsf_skiptimes = 3;               ///< no classification for any path this first number of times
uint8_t ara_clsf_sw_size = 5;                 ///< size of the sliding window for remembered path classifications
uint16_t ara_clsf_sender_rate = 100;          ///< sending speed in packets per second
int16_t nopkts_credit_max = 1;                ///< drop the path entry after x intervals without rx packets
ara_pcte_t* pct = NULL;                                   ///< path classification table
pthread_rwlock_t pctlock = PTHREAD_RWLOCK_INITIALIZER;    ///< path classification table lock


/** Compare two path entries
  *
  * Compare the provided entry entry with (src, prevhop, iface) triple.
  * Every entry set to NULL is assumed to be already known as equal
  * @arg pe path entry
  * @arg src packet source
  * @arg prevhop previous hop
  * @arg iface interface of the prevhop
  * @return 0 if equal, something else otherwise
  */
inline int8_t ara_path_compare(const ara_path_t* pe, const ara_address_t src, const ara_address_t prevhop, const dessert_meshif_t* iface) {

    if(src != NULL
       && memcmp(&(pe->src), src, sizeof(ara_address_t)) != 0) {
        return -1;
    }

    if(prevhop != NULL
       && memcmp(&(pe->prevhop), prevhop, sizeof(ara_address_t)) != 0) {
        return -2;
    } 

    if(iface != NULL
       && pe->iface != iface) {
        return -4;
    }

    return 0;
}


/** 
  * update or add path information received via path extension    
  * @arg dst destination address
  * @arg src source address
  * @arg prevhop previous hop
  * @arg iface interface
  * @arg sendrate sender speed obtained from the extension
  * @arg nhops number of nexthops obtained from the classification extension
  * @return ARA_PCT_NEW if successfully added new entry
  *         ARA_PCT_UPDATED if successfully updated
  *         ARA_PCT_FAILED otherwise
 **/
ara_clsf_update_res_t ara_cl_update(ara_address_t dst, ara_address_t src, ara_address_t prevhop, dessert_meshif_t* iface, uint16_t sendrate, uint16_t nhops) {
    int ret = ARA_PCT_FAILED;
    ara_pcte_t* entry = NULL;  // classification table entry for the destination
    ara_path_t* cur = NULL;    // current path entry
    dessert_debug("Updating PCT entry:\n\tdst=" MAC " src=" MAC " prevhop= " MAC " iface=%s with values sendrate=%d, nhops=%d", EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name, sendrate, nhops); 
    pthread_rwlock_wrlock(&pctlock);
    HASH_FIND(hh, pct, dst, sizeof(ara_address_t), entry);

    /* create an entry for this destination if needed (with empty path list)*/
    if(entry == NULL) {
       entry = malloc(sizeof(ara_pcte_t));

       if(entry == NULL) {
          dessert_err("failed to allocate new path classification table entry");
          pthread_rwlock_unlock(&pctlock);
          return ret;
       }

       memcpy(&(entry->dst), dst, sizeof(ara_address_t));
       entry->paths = NULL;
       entry->clsf_skiptimes = ara_clsf_skiptimes;
       entry->num_paths = 0;
      
       HASH_ADD_KEYPTR(hh, pct, &(entry->dst), sizeof(ara_address_t), entry);
       dessert_debug("new path classification table entry:\n\tdst=" MAC, EXPLODE_ARRAY6(dst));
       ret = ARA_PCT_NEW;
    }

    assert(entry != NULL);

    /* now find the matching entry in the path list */
    if(entry->paths != NULL) {
        cur = entry->paths;
        
        while(cur != NULL && ara_path_compare(cur, src, prevhop, iface) != 0) {
            cur = cur->next;

            if (cur == entry->paths) {
		dessert_crit("infinite loop in path classification table");
            } 
        } 
    } 

    /* create new path entry, if it was not found */
    if(cur == NULL) {
       dessert_debug("new path:\n\tdst=" MAC " src= " MAC " prev=" MAC " iface=%s", EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);
       cur = malloc(sizeof(ara_path_t));
       
       if(cur == NULL) {
            dessert_crit("failed to allocate new path classification table entry");
            pthread_rwlock_unlock(&pctlock);
            return ret;
        }
 
       memcpy(&(cur->src), src, sizeof(ara_address_t));
       memcpy(&(cur->prevhop), prevhop, sizeof(ara_address_t));
       cur->iface = iface;
       cur->sendrate = sendrate;
       cur->nexthops = nhops;
       cur->rx_packets = 1;       // init with this first packet we got       
       cur->tx_packets = 0;       
       cur->loss_percent = 0;       
       cur->status = _P_UNCLASSIFIED;
       cur->longterm_status = _P_UNCLASSIFIED;
       cur->classifications = NULL; 
       cur->sw_index = 0;
       cur->nopkts_credit = nopkts_credit_max;

       cur->next = 0;
       cur->prev = 0;

       DL_APPEND(entry->paths, cur);
       entry->num_paths++;       
    }
    /* found path entry*/
    else {
       	dessert_debug("updating path:\n\tdst=" MAC " src= " MAC " prev=" MAC " iface=%s", EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);
        cur->sendrate = sendrate;
        cur->nexthops = nhops;
        cur->rx_packets++;            
    }

    // we updated something if we are here
    ret = ARA_PCT_UPDATED;

    pthread_rwlock_unlock(&pctlock);
    return ret;
}


/** Delete classification table entry
  * @arg dst destination address
  * @arg src source address
  * @arg prevhop previous hop address
  * @arg iface interface
  * @return ARA_PCT_DELETED if successfully deleted 
  *         ARA_PCT_FAILED otherwise
 **/
ara_clsf_update_res_t ara_cl_delete(ara_address_t dst, ara_address_t src, ara_address_t prevhop, dessert_meshif_t* iface) {
    int ret = ARA_PCT_FAILED;
    ara_pcte_t* entry = NULL;  // classification table entry for the destination
    ara_path_t* cur = NULL;    // current path entry
    dessert_debug("Trying to remove PCT entry:\n\tdst=" MAC " src=" MAC " prev=" MAC " iface=%s",
                  EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);

    pthread_rwlock_wrlock(&pctlock);
    HASH_FIND(hh, pct, dst, sizeof(ara_address_t), entry);

    /* entry was not found */
    if(entry == NULL) {
            dessert_warn("tried to remove unknown classification table entry:\n\tdst=" MAC " src=" MAC " prev=" MAC " iface=%s",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);
            goto ara_pctdelete_out;
    }

    assert(entry != NULL);

    /* find matching entry in linked list; */
    if(entry->paths != NULL) {
        cur = entry->paths;

        while(cur != NULL && ara_path_compare(cur, src, prevhop, iface) != 0) {
            cur = cur->next;

            if(cur == entry->paths) {
                dessert_crit("infinite loop path classification table entry");
            }
        }
    }

    /* was the path found? */
    if(cur == NULL) {
        dessert_warn("tried to remove unknown path:\n\tdst=" MAC " src=" MAC " prev=" MAC " iface=%s",
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);
        goto ara_pctdelete_out;
    }
    /* found path entry */
    else {
        dessert_debug("found path to delete:\n\tdst=" MAC " src=" MAC " prev=" MAC " iface=%s", 
                         EXPLODE_ARRAY6(dst), EXPLODE_ARRAY6(src), EXPLODE_ARRAY6(prevhop), iface->if_name);

        //may not happen
        assert(entry->num_paths != 0);
        entry->num_paths--;

        if (entry->num_paths <= 0) {
                dessert_debug("dropping whole path classification table entry, num_paths=%ld", entry->num_paths);
                free(cur);
                if(entry->hh.next == NULL) { // this is the last entry in the hashmap
                    HASH_DELETE(hh, pct, entry);
                    free(entry);
                }
                else {
                    ara_pcte_t* del = entry;
                    entry = entry->hh.next;
                    HASH_DELETE(hh, pct, del);
                    free(del);
                }
        }
        else { // paths still there
                DL_DELETE(entry->paths, cur)
                free(cur);
                dessert_debug("dropping path, not the last, num_paths=%2d", entry->num_paths);
        }

        // deleted something so far
        ret = ARA_PCT_DELETED;
    }

ara_pctdelete_out:
    pthread_rwlock_unlock(&pctlock);
    return ret;
}



/** Add path classification extension containing sender rate and the nhops number Nh
  * to the packets in the sys pipeline.
  * This function is called by the packet sender to piggyback classification data
  * @arg msg message to add classification extension
  * @arg ap processing buffer
  * @return void   
 **/
void _ara_classext_add(dessert_msg_t* msg, ara_proc_t* ap) {
    clsf_extension_t *cext = NULL;
    dessert_ext_t *ext = NULL;
    int nhops = ara_rt_route_nhopnumber(ap->dst);
    
    if(nhops < 1) { // can happen by the first (trapped) packet. We want to count the packet, nh will be updated by subsequent packets
       nhops = 1;
    }

    // add classification extension if none is there
    int x = dessert_msg_getext(msg, NULL, CLASSIF_EXT, 0);

    if((x < 1) && (nhops > 0)) {
        dessert_msg_addext(msg, &ext, CLASSIF_EXT, sizeof(clsf_extension_t));
        cext = (clsf_extension_t*) &(ext->data);
        cext->send_rate = ara_clsf_sender_rate;
        cext->nexthops = nhops;
        dessert_debug("classification: add sendrate=%d Nh=%d to the packet:\n\tsrc=" MAC ", dst=" MAC ", seq=%d", cext->send_rate, cext->nexthops, EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq);
    }
    else {
        dessert_debug("classification: not piggybacking Nh=%d:\n\tsrc=" MAC " dst=" MAC " seq=%d", nhops, EXPLODE_ARRAY6(ap->src), EXPLODE_ARRAY6(ap->dst), ap->seq);
    }
}

/** Update the classification extension with node data
  * called by intermediate nodes to update the clsf table entry and to update the
  * Nh value of the classification extension in the message 
  * @arg msg message with the classification extension
  * @arg ap processing buffer
  * @return zero on success
  *         nonzero otherwise     
  */
static _ara_classext_update(dessert_msg_t* msg, dessert_meshif_t* iface, ara_proc_t* ap) {
    clsf_extension_t *cext = NULL;
    dessert_ext_t *ext = NULL;
    int nhops = ara_rt_route_nhopnumber(ap->dst);
    
    if(nhops < 1) { // can happen by the first trapped packet. We want to count the packet, nh will be updated by subsequent packets
       nhops = 1;
    }
    // extract the classification extension
    int x = dessert_msg_getext(msg, &ext, CLASSIF_EXT, 0);

    if(x != 1) { 
        // there should be one classification extension
        dessert_debug("classification: wrong # of exts, #=%d", x);
        return -1;
    }
    // update the table with received path information: sender speed and hexthops
    cext = (clsf_extension_t*) &(ext->data);
    dessert_debug("classification: got extension: send_rate=%d Nh=%d", cext->send_rate, cext->nexthops);
    cext->nexthops = cext->nexthops * nhops;
    ara_cl_update(ap->dst, ap->src, ap->prevhop, iface, cext->send_rate, cext->nexthops);     
    return 0;
}

/** Piggyback needed data before sending packets.
 *
 * Piggybacks needed data before sending the packet out to a particular destination.
 * This is used for path classification.
 */
int ara_classification_sender(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(ap != 0);
    
    //pass if the classification is not activated 
    if(ara_classify == 0) {
        return DESSERT_MSG_KEEP;
    }   
 
    // do not update...
    if(proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST  // ...for any broadcast
       || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST  // ...for any multicast
       || proc->lflags & DESSERT_RX_FLAG_L25_DST        // ...if this is the packet destination
      ) { 
        return DESSERT_MSG_KEEP;
    }

    _ara_classext_add(msg, ap);
    return DESSERT_MSG_KEEP;
}


/** Process incoming packets and get piggybacked classification information
 *
 *  Updates the path table with piggybacked information from the packet 
 */
dessert_cb_result ara_classification_node(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    ara_proc_t* ap = ara_proc_get(proc);
    assert(proc != NULL);

    //pass if the classification is not activated 
    if(ara_classify == 0) {
        return DESSERT_MSG_KEEP;
    }

    // do not update...
    if(proc->lflags & DESSERT_RX_FLAG_L25_SRC           // ...for overheard packets sent by this node
       || proc->lflags & DESSERT_RX_FLAG_L2_SRC         // ...for overheard packets forwarded by this node
       || ap->flags & ARA_RT_UPDATE_IGN                 // ...if we are told so (e.g. looping packet going for new route)
       || proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST  // ...for any broadcast
       || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST  // ...for any multicast
       || proc->lflags & DESSERT_RX_FLAG_L2_BROADCAST   // ...for any nexthop broadcast
       || proc->lflags & DESSERT_RX_FLAG_L25_DST        // ...if this is the packet destination 
      ) { 
        return DESSERT_MSG_KEEP;
    }
    
    _ara_classext_update(msg, iface, ap);
    return DESSERT_MSG_KEEP;
}

/** Make a log message about the new classification table entry.
  * @arg entry classification table entry 
  * @arg cur current path entry
  * @return void
 **/
void _clsf_logmessage(ara_pcte_t* entry, ara_path_t* cur) {
    char* res = NULL; 
    switch(cur->status) {
        case _P_LOSSY:
            res = "act-lossy";
            break;
        case _P_ACCEPTABLE:
            res = "act-acceptable";
            break;
        case _P_EFFICIENT:
            res = "act-efficient";
            break;
        case _P_UNCLASSIFIED:
            res = "act-unclassified";
            break;
        case _P_INTERR:
            res = "internal error";
            break;
    }
    dessert_debug("classified path as %s with credit=%d num_paths=%d for:\n\tdst=" MAC " src=" MAC " prev=" MAC, res, cur->nopkts_credit, entry->num_paths, EXPLODE_ARRAY6(entry->dst), EXPLODE_ARRAY6(cur->src),EXPLODE_ARRAY6(cur->prevhop));
    //longterm classification 
    switch(cur->longterm_status) {
        case _P_LOSSY:
            res = "lt-lossy";
            break;
        case _P_ACCEPTABLE:
            res = "lt-acceptable";
            break;
        case _P_EFFICIENT:
            res = "lt-efficient";
            break;
        case _P_UNCLASSIFIED:
            res = "lt-unclassified";
            break;
        case _P_INTERR:
            res = "internal error";
            break;
    }
    dessert_debug("classification: path is longterm %s with num_paths=%d for:\n\tdst=" MAC " src=" MAC " prev=" MAC, res, entry->num_paths, EXPLODE_ARRAY6(entry->dst), EXPLODE_ARRAY6(cur->src),EXPLODE_ARRAY6(cur->prevhop));

}


/** Classify the path basing on the entry information 
 *
 */
_ara_clsf_status_t _ara_classify(ara_path_t* cur) {
    uint16_t loss_pkt = 0;
    /* calculate the optimal expected packet count*/
    if(cur->nexthops == 0) { // should not happen    
        dessert_warn("classification: internal error because of nh=0");
        return _P_INTERR;
    }
        cur->tx_packets = cur->sendrate * ara_clsf_tick_interval_s / cur->nexthops;

    if(cur->tx_packets == 0) { // should not happen
        dessert_warn("classification: internal error because of tx=0");
        return _P_INTERR;
    }

    /* adjust tx value, if needed */
    if(cur->rx_packets < cur->tx_packets) {
        loss_pkt = cur->tx_packets - cur->rx_packets;
    }
    else {
        loss_pkt = 0;
    }
    /* classify the path according to the estimated loss value in percent */
    cur->loss_percent = (loss_pkt * 100) / cur->tx_packets;

    if(cur->loss_percent > ara_clsf_lossmax) {
        return _P_LOSSY;
    }

    if((cur->loss_percent <= ara_clsf_lossmax) && (cur->loss_percent > ara_clsf_lossmin)) {
        return _P_ACCEPTABLE;
    }

    if(cur->loss_percent <= ara_clsf_lossmin) {
        return _P_EFFICIENT;
    }
    // should never happen
    return _P_INTERR;
}


/** Classify the path basing on all entries from the sliding window 
  * Long term classification is based on all entries from the sliding window
  * rx and tx are counted to sums, and then the classification is
  * calculated basing on these general values.  
  *
  */
_ara_clsf_status_t _ara_classify_longterm(ara_path_t* cur) {
    // iterate over classified entries to get summarized values
    ara_classified_t* cl;
    int16_t rx = 0;
    int16_t tx = 0;
    uint16_t loss_pkt = 0;
    uint16_t loss_percent = 0;
    uint8_t i;
    for(i = 0; i < ara_clsf_sw_size; i++) {       
        HASH_FIND(hh, cur->classifications, &(i), sizeof(uint8_t), cl);

        if(cl != NULL) { // hash map entry found
           //todo 
           rx = rx + cl->rx_packets;
           tx = tx + cl->tx_packets;
        }
    }

    // calculate the loss percent
    
    if(tx == 0) { // can happen at the first clsf
        return _P_UNCLASSIFIED;
    }

    /* adjust tx value, if needed */
    if(rx < tx) {
        loss_pkt = tx - rx;
    }
    else {
        loss_pkt = 0;
    }
    /* classify the path according to the estimated loss value in percent */
    loss_percent = (loss_pkt * 100) / tx;

    if(loss_percent > ara_clsf_lossmax) {
        return _P_LOSSY;
    }

    if((loss_percent <= ara_clsf_lossmax) && (loss_percent > ara_clsf_lossmin)) {
        return _P_ACCEPTABLE;
    }

    if(cur->loss_percent <= ara_clsf_lossmin) {
        return _P_EFFICIENT;
    }

    return _P_INTERR;
}


/** Apply the actual path classification to the routing table 
 *
 */
void _apply_path_classification(ara_pcte_t* entry, ara_path_t* cur) {
    // get the routing table entry, if it is there
    double delta_p = 0;
    int8_t res = 0;
    ara_nexthop_t rte;
    res = ara_rt_route_exists(entry->dst, &rte);
    if(res != -1) {
        // get the increasement value for the pheromone value
        switch(ara_ptrail_mode) {
            case ARA_PTRAIL_CLASSIC:
                delta_p = 0; // not possible because we do not have a msg ttl
                break;
            case ARA_PTRAIL_LINEAR:
            case ARA_PTRAIL_CUBIC:
                delta_p = rt_inc;
                break;
            default:
                assert(0); // should never happen
                break;
        }
        // handle the classification by adjusting the pheromone value
        switch(cur->status) {
            case _P_LOSSY:
                // decrease the pheromone value only if there are other paths
                if(cur->nexthops > 1) {
                    ara_rt_update(entry->dst, rte.nexthop, rte.iface, (-2) * delta_p, 0, 0);
                }
                break;
            case _P_ACCEPTABLE:
                //strenghten the pheromone value once
                ara_rt_update(entry->dst, rte.nexthop, rte.iface, delta_p, 0, 0);
                break;
            case _P_EFFICIENT:
                //strenghten the pheromone value twice
                ara_rt_update(entry->dst, rte.nexthop, rte.iface, 2 * delta_p, 0, 0);
                break;
        }
    }
    else { // the whole entry will be deleted because no route is there
        dessert_debug("will delete the destination from the classification table");
        entry->num_paths = 0;
    }
}


/** Classification table tick function
  *   
  *
 **/
dessert_per_result_t ara_classification_tick(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_pcte_t* entry = NULL;

    /* re-add task*/
    scheduled->tv_sec += ara_clsf_tick_interval_s;
    dessert_periodic_add(ara_classification_tick, NULL, scheduled, NULL);
 
    pthread_rwlock_wrlock(&pctlock);
    /* iterate over destination entries */
    for(entry = pct; entry != NULL; /* inc will be done later */) {
        ara_path_t* cur = NULL;
        ara_path_t* tmp = NULL;
        ara_classified_t* cl = NULL;

        /* skip initial classification for new entries some number of times */
        if(entry->clsf_skiptimes > 0) {
           dessert_debug("skipping initial classification for entry: skiptimes=%d, dst=" MAC, entry->clsf_skiptimes, EXPLODE_ARRAY6(entry->dst));
           entry->clsf_skiptimes--;
           entry = entry->hh.next;
           continue;
        }

        uint16_t a = 0;
        /* iterate over path list */
        DL_FOREACH(entry->paths, cur) {

            if(a++ > ARA_TOO_MANY_NEIGHBORS) {
                dessert_warn("have infinite loop or very large neighborhood!");
                a = 0; 
            }

            // classify the path
            cur->status = _ara_classify(cur);
            
            /* add latest classification to the classifications list */
            HASH_FIND(hh, cur->classifications, &(cur->sw_index), sizeof(uint8_t), cl);
            
            if(cl == NULL) { // hash map entry not found - add
                cl = malloc(sizeof(ara_classified_t));

                if(cl == NULL) {
                    dessert_err("failed to allocate new classification map");
                    pthread_rwlock_unlock(&pctlock);
                    return DESSERT_PER_KEEP;
                }
                //initialize
                cl->id = cur->sw_index;
                cl->rx_packets = cur->rx_packets;
                cl->tx_packets = cur->tx_packets;
                cl->loss_percent = cur->loss_percent;
                cl->status = cur->status;
                HASH_ADD_KEYPTR(hh, cur->classifications, &(cur->sw_index), sizeof(uint8_t), cl);
                dessert_debug("classification: new sw entry: id=%d tx=%d rx=%d loss_percent=%d status=%d",cl->id, cl->tx_packets, cl->rx_packets, cl->loss_percent, cl->status);
            } 
            else { // hash map entry found - modify only
                cl->rx_packets = cur->rx_packets;
                cl->tx_packets = cur->tx_packets;
                cl->loss_percent = cur->loss_percent;
                cl->status = cur->status;
                dessert_debug("classification: modified sw entry, id=%d tx=%d rx=%d loss_percent=%d status=%d",cl->id, cl->tx_packets, cl->rx_packets, cl->loss_percent, cl->status);
            }
 
            /* adjust the sliding window index*/
            cur->sw_index++;
            if(cur->sw_index == ara_clsf_sw_size-1) {
                cur->sw_index = 0;    
            }
 	
            // longterm path classification (with latest clsf)
            if ((ara_clsf_longterm == 1) && (entry->num_paths > 0)) {
                cur->longterm_status = _ara_classify_longterm(cur); 
            }

            //apply the path classification and write the log message
            if((entry->num_paths > 0) && (cur->nopkts_credit > 0)) {
                _apply_path_classification(entry, cur);
                _clsf_logmessage(entry, cur);
            }

            /* adjust the nopacket credit and reset the packet counter */
            if(cur->rx_packets == 0) {
                cur->nopkts_credit--;
            }
            else {
                cur->nopkts_credit = nopkts_credit_max;
            }
            cur->rx_packets = 0;

        } // foreach

        /* delete paths with no credit from the table */
        DL_FOREACH_SAFE(entry->paths, cur, tmp) {
            if ((cur->nopkts_credit <= 0) && (entry->num_paths > 0)){
                entry->num_paths--;
                dessert_debug("dropping clasiffication for path dst=" MAC " src= " MAC " prev " MAC ", num_paths=%2d", EXPLODE_ARRAY6(entry->dst), EXPLODE_ARRAY6(cur->src), EXPLODE_ARRAY6(cur->prevhop), entry->num_paths);
                DL_DELETE(entry->paths, cur);
                free(cur);
            }
        }

        /* remove the whole entry, if no paths left */ 
        if(entry->num_paths <= 0) { //remove current entry
            dessert_debug("dropping whole classification table entry " MAC ", num_paths=%2d", EXPLODE_ARRAY6(entry->dst), entry->num_paths);

            if(entry->hh.next == NULL) { // this is the last entry in the hashmap
                HASH_DELETE(hh, pct, entry);
                free(entry);
                entry = NULL; // will end the loop and ara_rt_tick()
            }
            else {
                ara_pcte_t* del = entry;
                entry = entry->hh.next;
                HASH_DELETE(hh, pct, del);
                free(del);
            }
        } 
        else { // go to the next entry in the hashmap
            entry = entry->hh.next;
        } 
    }//for

    pthread_rwlock_unlock(&pctlock);
    return DESSERT_PER_KEEP;
}


/** set up path classification table */
void ara_classification_init() {
    if(ara_classify == 1) {
        dessert_debug("initializing path classification");
        dessert_periodic_add(ara_classification_tick, NULL, NULL, NULL);
        dessert_debug("path classification initialized");
    }
}

/** CLI command - config mode - interface tap $iface, $ipv4-addr, $netmask */
int cli_showclassifictable(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_pcte_t* entry = NULL;

    pthread_rwlock_rdlock(&pctlock);

    for(entry = pct; entry != NULL; entry = entry->hh.next) {
        cli_print(cli, "\ndst=" MAC, EXPLODE_ARRAY6(entry->dst));

        ara_path_t* cur;
        DL_FOREACH(entry->paths, cur) {
            cli_print(cli, "\tsrc=" MAC "\tprevhop=" MAC " iface=%10s sendrate=%d nhops=%d rx_packets=%d status=%d", EXPLODE_ARRAY6(cur->src), EXPLODE_ARRAY6(cur->prevhop), cur->iface->if_name, cur->sendrate, cur->nexthops, cur->rx_packets, cur->status);
        }
    }

    pthread_rwlock_unlock(&pctlock);
    return CLI_OK;
}

/** Flush path classification table
 *
 * Remove all entries from path classification table.
 */
int cli_flushclassifictable(struct cli_def* cli, char* command, char* argv[], int argc) {
    ara_pcte_t* cur = NULL;

    pthread_rwlock_wrlock(&pctlock);

    while(pct) {
        cur = pct;
        HASH_DEL(pct, cur);
        free(cur);
    }

    pthread_rwlock_unlock(&pctlock);

    cli_print(cli, "flushed path classification table");
    dessert_warn("flushed path classification table");

    return CLI_OK;
}

/**
 * Print periodic
 */

dessert_per_result_t ara_print_cl_periodic(void* data, struct timeval* scheduled, struct timeval* interval) {
    ara_pcte_t* entry = NULL;
    char* end = "\n*END*";
    size_t size_left = 4096 - sizeof(end);
    char buf[size_left];
    uint16_t offset = 0;

    if(pct) {
        pthread_rwlock_rdlock(&pctlock);

        for(entry = pct; entry != NULL; entry = entry->hh.next) {
            size_t written = snprintf(buf + offset, size_left, "\n\nd=" MAC " paths=%ld", EXPLODE_ARRAY6(entry->dst), entry->num_paths);

            if(written < 0 || written >= size_left) {
                goto out_of_buffer;
            }

            size_left -= written;
            offset += written;

            ara_path_t* cur;
            DL_FOREACH(entry->paths, cur) {
                written = snprintf(buf + offset, size_left, "\n\tsrc=" MAC " prev=" MAC " i=%s sr=%d nh=%d rx_pkt=%d st=%d", EXPLODE_ARRAY6(cur->src), EXPLODE_ARRAY6(cur->prevhop), cur->iface->if_name, cur->sendrate, cur->nexthops, cur->rx_packets, cur->status);

                if(written < 0 || written >= size_left) {
                    goto out_of_buffer;
                }

                size_left -= written;
                offset += written;
            }
        }
     snprintf(buf + offset, size_left, "%s", end);
    out_of_buffer:
        dessert_info("%s", buf);
        pthread_rwlock_unlock(&pctlock);
    }

    if(ara_print_cl_interval_s > 0) {
        scheduled->tv_sec += ara_print_cl_interval_s;
        dessert_periodic_add(ara_print_cl_periodic, NULL, scheduled, NULL);
    }

    return DESSERT_PER_KEEP;
}


