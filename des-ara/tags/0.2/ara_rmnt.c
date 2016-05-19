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
typedef struct ara_rusage {
    ara_address_t dst;
    struct timeval t;
    /** handle for hastable usage */
    UT_hash_handle hh;
} ara_usage_t;

/** the allmighty usage table */
ara_usage_t *ut = NULL;
pthread_rwlock_t ullock = PTHREAD_RWLOCK_INITIALIZER;

int ara_pant_interval = 5;

/** send pant if i recvieve a packet from a dst I haven't set a packet to for ara_pant_interval */
int ara_maintainroute_pant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id) 
{
    ara_proc_t *ap = ara_proc_get(proc);
    struct timeval now;
    ara_usage_t *last;
    
    assert(proc != NULL);
    
    /* some packets aren't of interest */
    if( ap->flags & ARA_FLOOD ||
        (ap->flags & ARA_DELIVERABLE) == 0 ||
        !(ap->flags & ARA_LOCAL || ap->flags & ARA_ORIG_LOCAL ) ) 
    {
        return (DESSERT_MSG_KEEP);
    }
    
    gettimeofday(&now, NULL);
    pthread_rwlock_rdlock(&ullock);
    
    
    if (ap->flags & ARA_ORIG_LOCAL) 
    {
        HASH_FIND(hh, ut, &(ap->dst), sizeof(ara_address_t), last);
        if(last == NULL) {
            last = malloc(sizeof(ara_usage_t));
            memcpy(&(last->dst), &(ap->dst), sizeof(ara_address_t));
            HASH_ADD_KEYPTR(hh, ut, &(last->dst), sizeof(ara_address_t), last);
        }
        
        /* update */
        last->t.tv_usec  = now.tv_sec;
        last->t.tv_usec = now.tv_usec;
    }
    else if(ap->flags & ARA_LOCAL) 
    {
        HASH_FIND(hh, ut, &(ap->src), sizeof(ara_address_t), last);
        if(last == NULL) {
            last = malloc(sizeof(ara_usage_t));
            memcpy(&(last->dst), &(ap->src), sizeof(ara_address_t));
            HASH_ADD_KEYPTR(hh, ut, &(last->dst), sizeof(ara_address_t), last);
            last->t.tv_sec = 0;
            last->t.tv_usec = 0;
        }
        
        /* need to send pant ?*/
        if((last->t.tv_sec)+ara_pant_interval < now.tv_sec)
        {
            dessert_msg_t *pant;
            dessert_ext_t *ext;
            struct ether_header *eth;
            
            /* update */
            last->t.tv_sec = now.tv_sec;
            last->t.tv_usec = now.tv_usec;

            /* send pant */

            dessert_msg_new(&pant);
            ara_addseq(pant);
            pant->ttl = ara_defttl;
            pant->u8 |= ARA_ANT;

            dessert_msg_addext(pant, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
            eth = (struct ether_header*) ext->data;
            memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
            memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

            dessert_msg_addext(pant, &ext, ARA_EXT_BANT, ETHER_ADDR_LEN+4);
            memcpy(ext->data, ap->src, sizeof(ara_address_t));
            memcpy(ext->data+ETHER_ADDR_LEN, "BANT", 4);

            /* for loop vs duplicate detection */
            dessert_msg_trace_initiate(pant, DESSERT_MSG_TRACE_HOST);

            #ifdef ARA_ANT_DEBUG
            char buf[1024];
            dessert_msg_dump(pant, DESSERT_MAXFRAMELEN, buf, 1024);
            dessert_debug("sending pant:\n%s", buf);
            #endif

            dessert_meshsend_fast(pant, NULL);
            dessert_msg_destroy(pant);
        }
    }
    else
    { 
         /* this never happens */
        assert(0);
    }
    
    pthread_rwlock_unlock(&ullock);
    return (DESSERT_MSG_KEEP);
}
