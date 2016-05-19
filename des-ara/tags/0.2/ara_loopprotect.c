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
#include <unistd.h>

/** loop protection table */
typedef struct ara_lpte {
    ara_address_t src;
    /* last sequence number seen */
    ara_seq_t seq;
    /* bit-array of seen packets */
    uint64_t seen;
    /** time to live for this entry */
    int ttl;
    UT_hash_handle hh;
} ara_lpte_t;

long loopprotect_prevhop_self = 0;
long loopprotect_seen = 0;
long loopprotect_late = 0;
long loopprotect_looping = 0;
long loopprotect_duplicate = 0;

/** the allmighty routing table */
ara_lpte_t *lptt = NULL;
pthread_rwlock_t lplock = PTHREAD_RWLOCK_INITIALIZER;

/** how often run cleanup */
int loopprotect_tick_interval=10;
/** how long should an entry stay here? */
int loopprotect_ttl=30;



/** check if a packet iss a dupe (e.g. from broadcast) of a looping packet
  * @arg msg message to check
  * @returns 1 if packet has trace header and not locally porcessed
  * @returns 0 if packet has no trace header or was locally porcessed
  **/
int _ara_loopprotect_checkdupe(dessert_msg_t* msg)
{
    dessert_ext_t  *ext;
    int x = dessert_msg_getext(msg, NULL, DESSERT_EXT_TRACE, 0);
    int i;

    if(x<1)
        return 0;

    for(i=0; i<x; i++) {
        dessert_msg_getext(msg, &ext, DESSERT_EXT_TRACE, i);
        if(memcmp(ext->data, dessert_l25_defsrc, ETHER_ADDR_LEN) == 0) {
            return 0;
        }
    }
    return 1;
}

/** check if a packet iss looping */
#define _LP_OK 0
#define _LP_LATE 1
#define _LP_LOOPING 2
#define _LP_INTERR 255
int _ara_loopprotect_checkloop(ara_address_t src, ara_seq_t seq) {
    
    ara_lpte_t *lp = NULL;
    ara_seq_t ds;
    
    int ret = _LP_INTERR;
    
    pthread_rwlock_wrlock(&lplock);
    HASH_FIND(hh, lptt, src, sizeof(ara_address_t), lp);
    
    if(lp == NULL) {
        lp = malloc(sizeof(ara_lpte_t));
        if(lp == NULL) {
            dessert_err("faled to allocate memory");
            ret = _LP_INTERR;
            goto _ara_loopprotect_checkloop_out;
        }
        memcpy(&(lp->src), src, sizeof(ara_address_t));        
        lp->seq = seq-1;
        lp->seen = 0x0000000000000000;
        HASH_ADD_KEYPTR(hh, lptt, &(lp->src), sizeof(lp->src), lp);
    }
    
    /* update ttl */
    lp->ttl = loopprotect_ttl;
    
    /* no loop */
    ds = lp->seq - seq; 
    if(lp->seq < seq || ara_seq_overflow(lp->seq, seq))
    /* new packet */
    {
        uint64_t nseen = (lp->seen)<<(seq - lp->seq);
        nseen |= 0x0000000000000001;
        
        #ifdef ARA_LOOPPROTECT_DEBUG
        dessert_debug("new packet: seq=%d, lp->seq=%d, (seq - lp->seq)=%d, lp->seen=%ld, nseen=%d",
           seq, (lp->seq), (seq - lp->seq), (long) (lp->seen), (long) nseen);
        #endif
        
        lp->seq = seq;
        lp->seen = nseen;
        lp->seen |= 0x0000000000000001;
        ret = _LP_OK;
    } 
    else if(ds<64 && ((lp->seen) & ((uint64_t) 0x0000000000000001<<ds)) == 0)
    /* late packet not seen yet */
    {
        uint64_t nseen = lp->seen | ((uint64_t) 0x0000000000000001<<ds);
        
        #ifdef ARA_LOOPPROTECT_DEBUG
        dessert_debug("late packet: seq=%d, lp->seq=%d, (lp->seq - seq)=%d, lp->seen=%ld, nseen=%d",
           seq, (lp->seq), (lp->seq - seq), (long) (lp->seen), (long) nseen);
        #endif
        
        (lp->seen) = nseen;
        ret = _LP_LATE;
    }
    else
    /* looping packet */
    {
        #ifdef ARA_LOOPPROTECT_DEBUG
        dessert_debug("looping packet: seq=%d, lp->seq=%d,lp->seen=%ld",
           seq, (lp->seq), (long) (lp->seen));
        #endif
        
        ret = _LP_LOOPING;
    }
        
    _ara_loopprotect_checkloop_out:
    pthread_rwlock_unlock(&lplock);
    
    return(ret);
}


/** tag looping packets */
int ara_checkloop(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    int res;
    
    assert(proc != NULL);
    
    loopprotect_seen++;
    
    /* shortcut */
    if(proc->lflags & DESSERT_LFLAG_PREVHOP_SELF) {
        ap->flags |= ARA_LOOPING;
        loopprotect_prevhop_self++;
        return(DESSERT_MSG_DROP);
    }
    
    res = _ara_loopprotect_checkloop(ap->src, ap->seq);
    
    if(res == _LP_INTERR) 
    {
        return(DESSERT_MSG_DROP);
    } 
    else if(res ==  _LP_OK) 
    {
        return(DESSERT_MSG_KEEP);
    }
    else if(res ==  _LP_LATE) 
    {
        loopprotect_late++;
        return(DESSERT_MSG_KEEP);
    }
    else if(ap->flags & ARA_ROUTEFAIL)
    /* packets with routefail flag/ext need special treatment */
    {
        res = _ara_loopprotect_checkloop(ap->routefail_src, ap->routefail_seq);
        if(res == _LP_INTERR) 
        {
            return(DESSERT_MSG_DROP);
        } 
        else if(res ==  _LP_OK) 
        {
            return(DESSERT_MSG_KEEP);
        }
        else if(res ==  _LP_LATE) 
        {
            loopprotect_late++;
            return(DESSERT_MSG_KEEP);
        } else if(res ==  _LP_LOOPING) {
            #ifdef ARA_LOOPPROTECT_DEBUG
            dessert_debug("looping routefail packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                 (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                 (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                 (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                 ap->seq, (iface!=NULL)?(iface->if_name):"NULL");
            #endif
            ap->flags |= ARA_LOOPING;
            loopprotect_looping++;
        } else {
            /* this never happens */
            assert(0);
        }
    }
    else if(_ara_loopprotect_checkdupe(msg))
    {
        #ifdef ARA_LOOPPROTECT_DEBUG
        dessert_debug("duplicate packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
             (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
             (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
             (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
             ap->seq, (iface!=NULL)?(iface->if_name):"NULL");
        #endif
        ap->flags |= ARA_DUPLICATE;
        loopprotect_duplicate++;
    }
    else if(res ==  _LP_LOOPING)
    {
        #ifdef ARA_LOOPPROTECT_DEBUG
        dessert_debug("looping packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
             (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
             (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
             (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
             ap->seq, (iface!=NULL)?(iface->if_name):"NULL");
        #endif
        ap->flags |= ARA_LOOPING;
        loopprotect_looping++;
    } else {
        /* this never happens */
        assert(0);
    }
 
    return(DESSERT_MSG_KEEP);
    
}

/** age loop proctaction table */
int ara_loopprotect_tick(void *data, struct timeval *scheduled, struct timeval *interval) 
{
    ara_lpte_t *lp;
    int i=0;
    
    /* re-add task */
    scheduled->tv_sec+=loopprotect_tick_interval;
    dessert_periodic_add(ara_loopprotect_tick, NULL, scheduled, NULL);
    
    /* tick it */
    pthread_rwlock_wrlock(&lplock);
    for(lp=lptt; lp != NULL; /* must be done below */) {
        if(--(lp->ttl) <= 0) {
            ara_lpte_t *tmp = lp;
            lp=(lp->hh).next;
            HASH_DELETE(hh, lptt, tmp);
            free(tmp);
            i++;
        } else {
            lp=(lp->hh).next;
        }
    }
    pthread_rwlock_unlock(&lplock);
    
    #ifdef ARA_LOOPPROTECT_DEBUG
    if(i>0)
        dessert_debug("deleted %d old loopprotect entrys", i);
    #endif
        
    return(0);    
}

/** set up loop protection table */
void ara_loopprotect_init()
{
    
    /* add periodic task */
    dessert_periodic_add(ara_loopprotect_tick, NULL, NULL, NULL);
}

/** CLI command - config mode - interfcae tap $iface, $ipv4-addr, $netmask */
int cli_showloopprotect_table(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    ara_lpte_t *lp = NULL;

    pthread_rwlock_rdlock(&lplock);
    
    for(lp=lptt; lp != NULL; lp=(lp->hh).next ) {
        
        char x[65];
        int i;
        
        for(i = 0; i < 64; i++)
            x[63-i] = (((uint64_t)((uint64_t) 0x0000000000000001<<i)&(lp->seen)) == 0)?'_':'x';
        x[64] = 0x00;
        
        cli_print(cli, "\n%x:%x:%x:%x:%x:%x %s..%06d", 
            (lp->src)[0], (lp->src)[1], (lp->src)[2], (lp->src)[3], (lp->src)[4], (lp->src)[5], x, lp->seq);
        
    }

    pthread_rwlock_unlock(&lplock);
    return CLI_OK;
}

/** CLI command - config mode - interfcae tap $iface, $ipv4-addr, $netmask */
int cli_showloopprotect_statistics(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    cli_print(cli, "\nloopprotect_statistics:");
    cli_print(cli, "\tseen:         %10ld", loopprotect_seen);
    cli_print(cli, "\tlate:         %10ld", loopprotect_late);
    cli_print(cli, "\tprevhop_self: %10ld", loopprotect_prevhop_self);
    cli_print(cli, "\tlooping:      %10ld", loopprotect_looping);
    cli_print(cli, "\tduplicate:    %10ld", loopprotect_duplicate);
    
    return CLI_OK;
}
