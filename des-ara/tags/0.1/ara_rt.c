/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
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

/** routing table entry */
typedef struct ara_rte {
    /** destination address */ 
    ara_address_t dst;
    /** nexthop for this route */
    ara_address_t nexthop;
    /** interface to reach nexthop */
    const dessert_meshif_t *iface;
    /** current pheromone value */
    double pheromone;
    /** sequence number of the last update by the nexthop */
    ara_seq_t seq;
    /** for double linked list of entrys with same dst */
    struct ara_rte *prev;
    /** for double linked list of entrys with same dst */
    struct ara_rte *next;
    /** handle for hastable usage - only head of list is in hashtable */
    UT_hash_handle hh;
} ara_rte_t;

/** the allmighty routing table */
ara_rte_t *rt = NULL;
pthread_rwlock_t rtlock = PTHREAD_RWLOCK_INITIALIZER; /* ToDo: Lockingproblem finden */

/* parameters */
double rt_min_pheromone = 0.1;
double rt_delta_q = 0.75;
int rt_tick_interval = 20;

/* internal functions */
int ara_rt_get_weighted(ara_address_t dst, ara_rte_t *rrte);
int ara_rt_get_best(ara_address_t dst, ara_rte_t *rrte);

/** compair an ara route table entry with dst/nexthop/iface tripel
  * every entry set to NULL is assumed to be alredy known as equal
  * @arg rte route table entry
  * @arg dst destination address
  * @arg nexthop next hop
  * @arg iface interface
  * @return 0 if equal, something else otherwise
  */
inline int ara_rte_compair(const ara_rte_t *rte, const ara_address_t dst, const ara_address_t nexthop, const dessert_meshif_t *iface)
{
    
    if( dst != NULL && 
        memcmp(&(rte->dst), dst, sizeof(ara_address_t)) != 0)
    {
            return -1;
    }
    
    if( nexthop != NULL && 
        memcmp(&(rte->nexthop), nexthop, sizeof(ara_address_t)) != 0)
    {
            return -2;
    }
    
    if( iface != NULL && 
        rte->iface != iface)
    {
            return -4;
    }
    
    return 0;
}

/** get best route for a specific destination */ 
int ara_rt_get_best(ara_address_t dst,ara_rte_t *rrte)  
{
    
    ara_rte_t *rte = NULL;
    int ret = -1;
    
    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, rt, dst, sizeof(ara_address_t), rte);
    
    if(rte == NULL) {
        #ifdef ARA_RT_GET_DEBUG
        dessert_debug("no route found for dst=%02x:%02x:%02x:%02x:%02x:%02x", 
            dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        #endif
        ret = -1;
    } else {
        #ifdef ARA_RT_GET_DEBUG
        dessert_debug("route found for dst=%02x:%02x:%02x:%02x:%02x:%02x nexthop=%02x:%02x:%02x:%02x:%02x:%02x", 
            dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
            (rte->nexthop)[0], (rte->nexthop)[1], (rte->nexthop)[2],
            (rte->nexthop)[3], (rte->nexthop)[4], (rte->nexthop)[5]);
        #endif
        memcpy(rrte, rte, sizeof(ara_rte_t));
        ret = 0;
    }
    
    pthread_rwlock_unlock(&rtlock);
    
    return(ret);
    
}

/** get some route (at random but weighted by pheromone) for a specific destination */ 
int ara_rt_get_weighted(ara_address_t dst, ara_rte_t *rrte) 
{
    
    ara_rte_t *rte = NULL;
    ara_rte_t *cur;
    double psum=0;
    double target=0;
    int ret = -1;
    
    /* lookup in routing table */
    pthread_rwlock_rdlock(&rtlock);
    HASH_FIND(hh, rt, dst, sizeof(ara_address_t), rte);
    if(rte == NULL) {
        #ifdef ARA_RT_GET_DEBUG
        dessert_debug("no route found for dst=%02x:%02x:%02x:%02x:%02x:%02x", 
            dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        #endif
        goto ara_rtget_weighted_out;
    }
    
    /* get pheromone sum */
    for(cur = rte; cur != NULL; cur=cur->next) 
    {
        target+=cur->pheromone;
    }
    
    /* roll dice weighted dice how to forward */
    target *= ((long double) random())/((long double) RAND_MAX);
    for(cur = rte; cur != NULL; cur=cur->next) 
    {
        psum+=cur->pheromone;
        if(psum>=target) {
            #ifdef ARA_RT_GET_DEBUG
            dessert_debug("route found for dst=%02x:%02x:%02x:%02x:%02x:%02x nexthop=%02x:%02x:%02x:%02x:%02x:%02x", 
                dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
                (rte->nexthop)[0], (rte->nexthop)[1], (rte->nexthop)[2],
                (rte->nexthop)[3], (rte->nexthop)[4], (rte->nexthop)[5]);
            #endif
            goto ara_rtget_weighted_out;
        }
    }
    
    /* this never happens */
    dessert_err("dice roll failed...");
    
    ara_rtget_weighted_out:
    
    if(rte != NULL) {
        memcpy(rrte, rte, sizeof(ara_rte_t));
        ret = 0;
    }
    
    pthread_rwlock_unlock(&rtlock);
    
    return(ret);
}

/** IFACE_CALLBACK: add routing table entry to processing info */
int ara_getroute(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    
    int res;
    ara_rte_t rte;
    ara_proc_t *ap = ara_proc_get(proc);
    
    assert(proc != NULL);
    
    /* we are only interested in certan packets... */
    if( proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
        proc->lflags & DESSERT_LFLAG_DST_MULTICAST ) 
    {
        /* flood multicat and broadcast */
        ap->flags |= ARA_DELIVERABLE;
        ap->flags |= ARA_FLOOD;
        ap->flags |= ARA_LOCAL;
        
        /* fake route table entry */
        memcpy(ap->nexthop, ether_broadcast, sizeof(ara_address_t));
        ap->iface_out = NULL;
        
        #ifdef ARA_RT_GET_DEBUG
        dessert_debug("route found for dst=%02x:%02x:%02x:%02x:%02x:%02x - will flood packet",
            (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5]);
        #endif   
    }
    else if(proc->lflags & DESSERT_LFLAG_DST_SELF)
    {
        ap->flags |= ARA_DELIVERABLE;
        ap->flags |= ARA_LOCAL;
        
        #ifdef ARA_RT_GET_DEBUG
        dessert_debug("route found for dst=%02x:%02x:%02x:%02x:%02x:%02x - packet is for me",
            (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5]);
        #endif
    }
    else
    {
        /* deliver other things */
        ap->flags |= ARA_FORWARD;
        if(ara_forw_mode == ARA_FORW_B) {
            res = ara_rt_get_best(ap->dst, &rte);
        } else if(ara_forw_mode == ARA_FORW_W){
            res = ara_rt_get_weighted(ap->dst, &rte);
        } else {
            dessert_err("ara_forw_mode has invalid type %d - not forwarding packets!", ara_forw_mode);
            res = -1;
        }
        
        /* we have an entry */
        if(res >= 0) {
            ap->flags |= ARA_DELIVERABLE;
            memcpy(ap->nexthop, rte.nexthop, sizeof(ara_address_t));            
            ap->iface_out = rte.iface;
        }
    }
    
    /* handle routefail flagged messages that became deliverable */
    if(ap->flags & (ARA_DELIVERABLE|ARA_ROUTEFAIL))
    {
        msg->u8 &= ~ARA_ROUTEFAIL;
        msg->u8 |= ARA_ROUTEPROBLEM;
    }
    
    return (DESSERT_MSG_KEEP);
    
}


/** update or add the seq and paheromon for a dst/nexthop/iface tripel 
  * the table is only updated of seq is newer or has overflown
  * @arg dst destination address
  * @arg nexthop next hop
  * @arg iface interface
  * @arg delta_pheromone pheromone change to applay
  * @arg seq sequence number of the packet triggering the update
  * @return ARA_RT_NEW if the route is new (completly),
  *         ARA_RT_UPDATED if the route is updated,
  *         ARA_RT_OLD if the route is not updated because packet is old,
  *         ARA_RT_FAILED otherwise
  */
int ara_rt_update(ara_address_t dst, ara_address_t nexthop, const dessert_meshif_t *iface, double delta_pheromone, ara_seq_t seq )
{
    
    int ret = ARA_RT_FAILED;
    
    ara_rte_t *rte = NULL;   // entry to add/modify
    ara_rte_t *first = NULL; // entry in hashtable
    ara_rte_t *cur = NULL;   // entry currently work on
    
    #ifdef ARA_RT_UPDATE_DEBUG
    dessert_debug("update for dst=%02x:%02x:%02x:%02x:%02x:%02x nexthop=%02x:%02x:%02x:%02x:%02x:%02x iface=%s dp=%lf", 
        dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
        nexthop[0], nexthop[1], nexthop[2], nexthop[3], nexthop[4], nexthop[5],
        iface->if_name,delta_pheromone, sizeof(ara_address_t));
    #endif
    
    /* lookup in routing table */
    pthread_rwlock_wrlock(&rtlock);
    HASH_FIND(hh, rt, dst, sizeof(ara_address_t), first);
    rte = first;
    
    /* build routing table entry for dst if not alrady present */
    if(rte == NULL) 
    {
        /* do not add routes with (delta_pheromone<=0) */
        if (delta_pheromone<=0) {
            ret = ARA_RT_FAILED;
            goto ara_rtupdate_out;
        }
        
        rte = malloc(sizeof(ara_rte_t));
        if(rte == NULL) {
            dessert_err("failed to allocate new routing table entry");
            goto ara_rtupdate_out;
        }
        
        memcpy(&(rte->dst), dst, sizeof(ara_address_t));
        memcpy(&(rte->nexthop), nexthop, sizeof(ara_address_t));
        rte->prev = NULL;
        rte->next = NULL;
        rte->pheromone = delta_pheromone;
        rte->iface = iface;
        rte->seq = seq;
        
        HASH_ADD_KEYPTR(hh, rt, &(rte->dst), sizeof(ara_address_t), rte);
        
        ret = ARA_RT_NEW;
        goto ara_rtupdate_out;
        
    }
    
    /* already have routing table entry for dst - find the right one in linked list */
    
        
    /* find matching entry in linked list */
    cur = rte;
    while(cur->next != NULL && ara_rte_compair(cur, NULL, nexthop, iface) != 0) 
    {
        cur=cur->next;
        if(cur == first) {
            dessert_err("infinite loop in routing table entry - time to tune gdb in");
        }
    }
    
    /* not found ? */
    if(ara_rte_compair(cur, NULL, nexthop, iface) != 0) 
    {
    
        assert(cur->next == NULL); 
                    
        /* insert new one */
        rte = malloc(sizeof(ara_rte_t));
        if(rte == NULL) {
            dessert_err("failed to allocate new routing table entry");
            goto ara_rtupdate_out;
        }
    
        memcpy(&(rte->dst), dst, sizeof(ara_address_t));
        memcpy(&(rte->nexthop), nexthop, sizeof(ara_address_t));
    
        rte->pheromone = (delta_pheromone>=0)?delta_pheromone:0;
        rte->iface = iface;
        rte->seq = seq;
        
        rte->prev = cur;
        rte->next = NULL;
        
        cur->next = rte;
                    
    } 
    /* found */
    else
    {
        
        rte = cur;
        assert(ara_rte_compair(rte, NULL, nexthop, iface) == 0);
        
        /* is the update overflowed/late? */
        if(unlikely(ara_seq_overflow(rte->seq, seq))) {
            #ifdef ARA_RT_UPDATE_DEBUG
            dessert_debug("seq overflow");
            #endif
        } else if (rte->seq >= seq) {
            #ifdef ARA_RT_UPDATE_DEBUG
            dessert_debug("late update");
            #endif
            goto ara_rtupdate_out;
        }
                    
        /* update entry */
        rte->pheromone = (delta_pheromone>=0)?((rte->pheromone)+delta_pheromone):0;
        rte->seq = seq;
        
    }
    
    /* we updated something so far */
    ret = ARA_RT_UPDATED;
    
    /* do we need to sort ? */
    if( (rte->prev == NULL || (rte->prev)->pheromone >  rte->pheromone) &&
        (rte->next == NULL || (rte->next)->pheromone <= rte->pheromone)    ) 
    {
        /* no need to sort */
        goto ara_rtupdate_out;
    }
    
    /* splice out rte */
    if(rte->prev != NULL) {
        (rte->prev)->next = rte->next;
    }
    if(rte->next != NULL) {
        (rte->next)->prev = rte->prev;
    }
    
    /* will we become new head ? */
    assert(first != NULL);
    if(rte->pheromone >= first->pheromone) 
    {
        HASH_DELETE(hh, rt, first);
        rte->prev = NULL;
        rte->next = first;
        first->prev = rte;
        HASH_ADD_KEYPTR(hh, rt, &(rte->dst), sizeof(ara_address_t), rte);
        goto ara_rtupdate_out;
    }
    
    /* may we loose head status ? */
    if(rte == first && delta_pheromone < 0) 
    {
        HASH_DELETE(hh, rt, first);
    }
    
    /* no - find element before right position for new entry */
    cur = rte->prev;
    assert(cur != NULL);
    while(delta_pheromone >= 0 && cur->prev != NULL && cur->pheromone <= rte->pheromone)
    {
        cur = cur->prev;
    }
    while(delta_pheromone < 0 && cur->next != NULL && cur->pheromone > rte->pheromone)
    {
        cur = cur->next;
    }
    assert(cur->pheromone > rte->pheromone);
    assert(cur->next == NULL || (cur->next)->pheromone <= rte->pheromone);
    
    /* splice in rte */
    rte->prev = cur;
    rte->next = cur->next;
    cur->next = rte;
    if(rte->next != NULL) {
        (rte->next)->prev = rte;
    }

    /* if we lost head status - find new head and insert it */
    if(rte == first && delta_pheromone < 0) 
    {
        for(/* first = rte */; first->prev != NULL; first=first->prev);
        HASH_ADD_KEYPTR(hh, rt, &(first->dst), sizeof(ara_address_t), first);
        goto ara_rtupdate_out;
    }
    
    #ifndef NDEBUG
        /* check list is still valid */
        ara_rte_t *last;
        for(cur = first; cur->next != NULL; cur=cur->next)
            assert(cur->next != first);
        last = cur;
        for(/* at the end */; cur->prev != NULL; cur=cur->prev)
            assert(cur->prev != last);
    #endif
        
    ara_rtupdate_out:
    
    pthread_rwlock_unlock(&rtlock);
    return(ret);    
    
}

/** IFACE_CALLBACK: update routing table entry from an ant */
int ara_routeupdate_ant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    dessert_ext_t  *ext;
    double delta_p = 0;
    
    assert(proc != NULL);
    
    /* some packets are not useable for me */
    if(iface_in == NULL || proc->lflags & DESSERT_LFLAG_SRC_SELF || proc->lflags & DESSERT_LFLAG_PREVHOP_SELF)
        return (DESSERT_MSG_KEEP);
    if(ap->flags & ARA_RT_UPDATE_IGN || (msg->u8 & ARA_ANT) == 0)
        return (DESSERT_MSG_KEEP);
    
    /* only update if it is an ant */
    if( dessert_msg_getext(msg, &ext, ARA_EXT_FANT, 0) == 1 ||
        dessert_msg_getext(msg, &ext, ARA_EXT_BANT, 0) == 1   ) 
    {
        delta_p = ((double) (msg->ttl));
        ara_rt_update(ap->src, ap->prevhop, iface_in, delta_p, ap->seq);
    } else {
        dessert_info("got ant without ant - you'd better run!");
    }
    
    return (DESSERT_MSG_KEEP);
}

/** IFACE_CALLBACK: update routing table entry from a reverse flow */
int ara_routeupdate_rflow(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    double delta_p;
    
    assert(proc != NULL);
    
    /* local generated or frwarded packes are not useable for me */
    if(iface_in == NULL || proc->lflags & DESSERT_LFLAG_SRC_SELF || proc->lflags & DESSERT_LFLAG_PREVHOP_SELF)
        return (DESSERT_MSG_KEEP);
    
    /* we dont' want to update problematic packest or ants */
    if(ap->flags & ARA_RT_UPDATE_IGN || msg->u8 & ARA_ANT)
        return (DESSERT_MSG_KEEP);

    
    /* get delta p */
    delta_p = ((double) (msg->ttl))/((double) 10);
        
    /* update routing table */
    ara_rt_update(ap->src, ap->prevhop, iface_in, delta_p, ap->seq);
    
    /* done */
    return (DESSERT_MSG_KEEP);
}

/** IFACE_CALLBACK: update routing table entry from a packet with ARA_ROUTEFAIL set */
int ara_routeupdate_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    
    assert(proc != NULL);
    
    /* local generated packes are not useable for me */
    if(iface_in == NULL || proc->lflags & DESSERT_LFLAG_DST_SELF)
        return (DESSERT_MSG_KEEP);
    
    /* we only want route fails */
    if((msg->u8 & ARA_ROUTEFAIL) == 0)
        return (DESSERT_MSG_KEEP);

    /* update routing table */
    ara_rt_update(ap->dst, ap->prevhop, iface_in, -1, ap->seq);
    
    /* done */
    return (DESSERT_MSG_KEEP);
}


/** age pheromone values and delete old entrys */
int ara_rt_tick(void *data, struct timeval *scheduled, struct timeval *interval) 
{

    ara_rte_t *rtd = NULL;    
    ara_rte_t *rte = NULL;
    ara_rte_t *last_rte = NULL;
    
    /* re-add task */
    scheduled->tv_sec+=rt_tick_interval;
    dessert_periodic_add(ara_rt_tick, NULL, scheduled, NULL);
        
    pthread_rwlock_wrlock(&rtlock);
    
    /* iterate over route table entrys by destination*/
    for(rtd = rt; rtd != NULL; /* must be done in if/else */ ) {
        
        /* iterate over route table entrys for one destination */
        int a = 0;
        for(rte = rtd; rte != NULL; rte = rte->next) {
            if(a++ > 1024) {
                dessert_warn("have infinite loop or gigantic neighbourship!");
                a = 0;
            }
            rte->pheromone *= rt_delta_q;
            last_rte = rte;
        }
        
        /* iterate over route table entrys for one destination again *
         * in backward direction and process head separately         */
        for(rte = last_rte; rte != rtd ; /* must be done in if/else */ ) {
                        
            /* we don't process head entry here */
            assert(rte != NULL && rte->prev != NULL);
            
            /* list must still be sorted by pheromone */
            assert((rte->prev)->pheromone >= rte->pheromone);
         
            /* is this entry obsolete */
            if(rte->pheromone < rt_min_pheromone) {
                rte = rte->prev;
                free(rte->next);
                rte->next = NULL; 
            } else {
                /* the other entrys have higher pheromone *
                 * we don't need to scan them            */
                goto rt_tick_rteremove_done;
            }
        }
        
        rt_tick_rteremove_done:
        
        /* all the list has been eaten up - also eat rte=rtd? */
        if(rte==rtd && rtd->pheromone < rt_min_pheromone) {
            rtd=(rtd->hh).next;
            HASH_DELETE(hh, rt, rte);
            free(rte);
        } else {
            rtd=(rtd->hh).next;            
        }
        
    }
    
    pthread_rwlock_unlock(&rtlock);
    
    return(0);
}


void ara_rt_init() {
    
//   /* test routing table */
//   ara_address_t src, nh;
//   src[0] = 0x00; src[1] = 0x11; src[2] = 0x00; 
//   src[3] = 0x00; src[4] = 0x11; src[5] = 0x00; 
//   
//   nh[0] = 0x00; nh[1] = 0x22; nh[2] = 0x00; 
//   nh[3] = 0x00; nh[4] = 0x22; nh[5] = 0x00;
//   
//   ara_rt_update(src, nh, NULL, 23, 1);
//   
//   nh[0] = 0x00; nh[1] = 0x33; nh[2] = 0x00; 
//   nh[3] = 0x00; nh[4] = 0x33; nh[5] = 0x00;
//   
//   ara_rt_update(src, nh, NULL, 21, 1);
//
//   ara_rt_update(src, nh, NULL, 21, 2);
//
//   src[0] = 0x00; src[1] = 0x41; src[2] = 0x00; 
//   src[3] = 0x00; src[4] = 0x41; src[5] = 0x00; 
//   
//   nh[0] = 0x00; nh[1] = 0x23; nh[2] = 0x00; 
//   nh[3] = 0x00; nh[4] = 0x23; nh[5] = 0x00;
//   
//   ara_rt_update(src, nh, NULL, 17, 1);
    
    
    /* add periodic task */
    dessert_periodic_add(ara_rt_tick, NULL, NULL, NULL);
}

/** CLI command - config mode - interfcae tap $iface, $ipv4-addr, $netmask */
int cli_showroutingtable(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    ara_rte_t *rtd = NULL;    
    ara_rte_t *rte = NULL;
        
    pthread_rwlock_rdlock(&rtlock);
    
    for(rtd = rt; rtd != NULL; rtd=(rtd->hh).next ) {
        char *best = "(best)";
        cli_print(cli, "\ndst=%02x:%02x:%02x:%02x:%02x:%02x:", 
            (rtd->dst)[0], (rtd->dst)[1], (rtd->dst)[2],
            (rtd->dst)[3], (rtd->dst)[4], (rtd->dst)[5]);
            
        for(rte = rtd; rte != NULL; rte = rte->next) {
            cli_print(cli, "\tnexthop=%02x:%02x:%02x:%02x:%02x:%02x iface=%s seq=%06d pheromone=%04.02lf %s", 
                (rte->nexthop)[0], (rte->nexthop)[1], (rte->nexthop)[2],
                (rte->nexthop)[3], (rte->nexthop)[4], (rte->nexthop)[5], 
                ((rte->iface)==NULL)?"NULL":(rte->iface)->if_name, rte->seq, rte->pheromone, best);
            best = "";
        }
    }
    
    pthread_rwlock_unlock(&rtlock);
    
    return CLI_OK;
}
