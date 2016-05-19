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

/* local variables */
ara_seq_t _ara_nextseq = 0;
pthread_mutex_t _ara_nextseq_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t ara_trace_broadcastlen = 0;
uint8_t ara_defttl = 32;


/** add a sequence number to a new ara packet */
ara_seq_t ara_seq_next() 
{
    ara_seq_t r;
    pthread_mutex_lock(&_ara_nextseq_mutex);
    r = _ara_nextseq++;
    pthread_mutex_unlock(&_ara_nextseq_mutex);
    return(r);
}

/** add a sequence number to a new ara packet */
void ara_addseq(dessert_msg_t *msg) 
{
    msg->u16 = htons(ara_seq_next());
}



/** dump packets via tun interface **/
int ara_tun2ara (struct ether_header *eth, size_t len, dessert_msg_proc_t *proc, dessert_tunif_t *tunif, dessert_frameid_t id)
{   
    dessert_msg_t *msg;
    int ret = 0;
    ara_proc_t *ap = ara_proc_get(proc);
    
    /* encapsulate frame */
    dessert_msg_ethencap(eth, len, &msg);
    ara_addseq(msg);
    ap->flags |= ARA_ORIG_LOCAL;
    
    /* add trace header if packet iss small broadcast/multicast packet 
       this helps to abuse such packets for alternate path route maintanence */
    if( (eth->ether_dhost[0]&0x01) && len < ara_trace_broadcastlen) {
        dessert_msg_trace_initiate(msg, DESSERT_MSG_TRACE_HOST);
    }
    
    ret = dessert_meshrxcb_runall(msg, DESSERT_MAXFRAMEBUFLEN, proc, NULL ,id);
    dessert_msg_destroy(msg);
    return ret;
    
}

/** forward packets recvieved via ara to tun interface **/
int ara_ara2tun(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    struct ether_header *eth;
    size_t eth_len;
    
    assert(proc != 0);
    
    /* packet for myself - forward to tun */
    if( ap->flags & ARA_LOCAL ) {
        eth_len = dessert_msg_ethdecap(msg, &eth);
        dessert_tunsend(eth, eth_len);
        free(eth);
    }
    
    return DESSERT_MSG_KEEP;
}



/** drop accidentally captured packets which would loop otherwise */
int ara_checkl2dst(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    
    assert(proc != NULL);
    
    if((proc->lflags & (DESSERT_LFLAG_NEXTHOP_SELF | DESSERT_LFLAG_NEXTHOP_BROADCAST)) == 0) {
        #ifdef ARA_LOOPHANLDERS_DEBUG
            dessert_debug("dropping accidentally captured packet - you better disable promisc-mode"); 
        #endif
        return (DESSERT_MSG_DROP);
    }
    
    return DESSERT_MSG_KEEP;
}

/** add ara processing info */
int ara_makeproc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    
    struct ether_header *l25h;
    ara_proc_t *ap = ara_proc_get(proc);
    dessert_ext_t *ext;
        
    /* get ap */
    if(proc == NULL) {   
        return (DESSERT_MSG_NEEDMSGPROC);
    }
        
    /* get l2.5 header if possible */
    l25h = dessert_msg_getl25ether(msg);
    if(l25h != NULL) {
        memcpy(ap->src, l25h->ether_shost, ETHER_ADDR_LEN);
        memcpy(ap->dst, l25h->ether_dhost, ETHER_ADDR_LEN);
        ap->flags |= ARA_VALID;
        /* flag local generated messages */
        if(ap->flags & ARA_ORIG_LOCAL || memcmp(ap->src, l25h->ether_shost, ETHER_ADDR_LEN)==0) {
            ap->flags |= ARA_ORIG_LOCAL;
        }
    } else {
        /* drop invalid packages */
        return (DESSERT_MSG_DROP);
    }
    ap->iface_in = iface_in;
    memcpy(ap->prevhop, msg->l2h.ether_shost, ETHER_ADDR_LEN);
    ap->seq = (ara_seq_t) ntohs(msg->u16);
    
    /* get loop protection data if routefail set */
    if(msg->u8 & ARA_ROUTEFAIL && dessert_msg_getext(msg, &ext, ARA_EXT_ROUTEFAIL, 0) == 1) 
    {
        ara_ext_routefail_t *rf = (ara_ext_routefail_t *) ext->data;
        ap->routefail_seq = ntohs(rf->seq);
        memcpy(&(ap->routefail_src), l25h->ether_shost, ETHER_ADDR_LEN);
    }
    else if(msg->u8 & ARA_ROUTEFAIL && dessert_msg_getext(msg, NULL, ARA_EXT_ROUTEFAIL, 0) != 1)
    {
        dessert_err("got corrupt routefail packet");
        return (DESSERT_MSG_DROP);
    }
    
    return (DESSERT_MSG_KEEP);
    
}


/** handle and generate packets with msg->u8 & ARA_LOOPING_PACKET */
int ara_handle_loops(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    
    assert(proc != NULL);
    
    /* shortcut */
    if(ap->flags & ARA_LOOPING && proc->lflags & DESSERT_LFLAG_PREVHOP_SELF) {
        #ifdef ARA_LOOPHANLDERS_DEBUG
             dessert_debug("ignoring packet sent by myself src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                  (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                  (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                  (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                  ap->seq, (iface_in!=NULL)?(iface_in->if_name):"NULL");
         #endif
        return (DESSERT_MSG_DROP);
    }

    /* looping flood packet */
    else if( ap->flags & ARA_LOOPING && (
             proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
             proc->lflags & DESSERT_LFLAG_DST_MULTICAST   ))
    {
        #ifdef ARA_LOOPHANLDERS_DEBUG
             dessert_debug("droping looping broadcast/multicast packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                  (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                  (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                  (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                  ap->seq, (iface_in!=NULL)?(iface_in->if_name):"NULL");
         #endif
        /* drop silently */
        return (DESSERT_MSG_DROP);
    }

    /* reply to looping packet */
    else if(msg->u8 & ARA_LOOPING_PACKET)
    {
        #ifdef ARA_LOOPHANLDERS_DEBUG
            dessert_debug("trying to resend packet with ARA_LOOPING_PACKET-flag src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                 (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                 (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                 (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                 ap->seq, (iface_in!=NULL)?(iface_in->if_name):"NULL");
        #endif
        
        /* delete route entry */
        ara_rt_update(ap->dst, ap->prevhop, iface_in, -1, ap->seq);
        
        /* fix message and ap flags */
        msg->u8 &= ~ARA_LOOPING_PACKET;
        ap->flags &= ~ARA_LOOPING;
        ap->flags |= ARA_RT_UPDATE_IGN;
        
        /* try to resent it */
        return (DESSERT_MSG_KEEP);        
    }

    /* looping route-fail packet */
    else if(ap->flags & ARA_LOOPING && msg->u8 & ARA_ROUTEFAIL)
    {
        #ifdef ARA_LOOPHANLDERS_DEBUG
             dessert_debug("droping looping route-fail packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                  (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                  (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                  (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                  ap->seq, (iface_in!=NULL)?(iface_in->if_name):"NULL");
         #endif
        /* drop silently */
        return (DESSERT_MSG_DROP);        
    }

    /* looping non-flood packet */
    else if( ap->flags & ARA_LOOPING && !(
             proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
             proc->lflags & DESSERT_LFLAG_DST_MULTICAST   ))
    {
        
        #ifdef ARA_LOOPHANLDERS_DEBUG
             dessert_debug("returning looping unicast packet to prevhop src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                  (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                  (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                  (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                  ap->seq, (iface_in!=NULL)?(iface_in->if_name):"NULL");
         #endif 
        
        /* send route fail */
        msg->u8 |= ARA_LOOPING_PACKET;
        ara_address_t tmp;
        memcpy(tmp, (msg->l2h.ether_dhost), sizeof(ETHER_ADDR_LEN));
        memcpy((msg->l2h.ether_dhost), (msg->l2h.ether_shost), sizeof(ETHER_ADDR_LEN)); 
        memcpy((msg->l2h.ether_shost), tmp, sizeof(ETHER_ADDR_LEN)); 
        msg->ttl--;
       
        if(msg->ttl>0) {
            dessert_meshsend_fast(msg, ap->iface_out);
        }
        
        return (DESSERT_MSG_DROP);
        
    }
    
    return (DESSERT_MSG_KEEP);        
}


/** drop packets tagged duplicate */
int ara_dropdupe(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id)
{
    ara_proc_t *ap = ara_proc_get(proc);
    
    assert(proc != NULL);
    
    if(ap->flags & ARA_DUPLICATE) {
        #ifdef ARA_LOOPHANLDERS_DEBUG
            dessert_debug("dropping duplicate packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x prevhop=%02x:%02x:%02x:%02x:%02x:%02x seq=%06d iface=%s", 
                 (ap->src)[0], (ap->src)[1], (ap->src)[2], (ap->src)[3], (ap->src)[4], (ap->src)[5],
                 (ap->dst)[0], (ap->dst)[1], (ap->dst)[2], (ap->dst)[3], (ap->dst)[4], (ap->dst)[5],
                 (ap->prevhop)[0], (ap->prevhop)[1], (ap->prevhop)[2], (ap->prevhop)[3], (ap->prevhop)[4], (ap->prevhop)[5],
                 ap->seq, (iface!=NULL)?(iface->if_name):"NULL");
        #endif 
        return(DESSERT_MSG_DROP);   
    }
        
    return(DESSERT_MSG_KEEP);
}


/** send packet over the network if route exists */
int ara_forward(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id) 
{

    ara_proc_t *ap = ara_proc_get(proc);
    
    assert(proc != NULL);

    /* check if route exists */
    if ( ap->flags & ARA_DELIVERABLE &&
        (ap->flags & ARA_FLOOD ||
         ap->flags & ARA_FORWARD)) 
    {
        /* we have a route - send */
        ara_address_t tmp;
        memcpy(tmp, (msg->l2h.ether_dhost), sizeof(ara_address_t));
        memcpy((msg->l2h.ether_dhost), (ap->nexthop), sizeof(ara_address_t)); 
        msg->ttl--;
        
        if(msg->ttl>0) {
            dessert_meshsend_fast(msg, ap->iface_out);
        }
        
        msg->ttl++;
        memcpy((msg->l2h.ether_dhost), tmp, sizeof(ara_address_t)); 
    }
    
    return (DESSERT_MSG_KEEP);

}

/** handle route fails */
int ara_routefail(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id) 
{
    ara_proc_t *ap = ara_proc_get(proc);

    assert(proc != NULL);

    /* only handle certain packet */
    if( ap->flags & ARA_DELIVERABLE ) {
        return (DESSERT_MSG_KEEP);
    }

    /* packet is local generated */
    if(ap->flags & ARA_ORIG_LOCAL) {
        int wp = 0;
        /* route does not exists - we can trap the packet */
        wp = trap_packet(ap->dst, msg, len, proc, id);
        /* schedule route discovery and cleanup if we not alredy scheduled one */
        if(wp == 1) {
            
            void *perdata;
            dessert_msg_t *fant = NULL;
            dessert_ext_t *ext = NULL;
            struct ether_header *eth;
            #ifdef ARA_ANT_DEBUG
            char buf[1024];
            #endif
            
            /* start discovery */
            dessert_msg_new(&fant);
            ara_addseq(fant);
            fant->ttl = ara_defttl;
            fant->u8 |= ARA_ANT;
            
            dessert_msg_addext(fant, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
            eth = (struct ether_header*) ext->data;
            memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
            memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);
            
            dessert_msg_addext(fant, &ext, ARA_EXT_FANT, ETHER_ADDR_LEN+4);
            memcpy(ext->data, ap->dst, sizeof(ara_address_t));
            memcpy(ext->data+ETHER_ADDR_LEN, "FANT", 4);
            
            /* for loop vs duplicate detection */
            dessert_msg_trace_initiate(fant, DESSERT_MSG_TRACE_HOST);
            
            #ifdef ARA_ANT_DEBUG
            dessert_msg_dump(fant, DESSERT_MAXFRAMELEN, buf, 1024);
            dessert_debug("sending fant:\n%s", buf);
            #endif
            
            dessert_meshsend_fast(fant, NULL);
            dessert_msg_destroy(fant);
            
            /* schedule "cleanup/retry" */
            perdata  = malloc(sizeof(ara_address_t)); // free in ara_routefail_untrap_packets
            memcpy(perdata, &(ap->dst), sizeof(ara_address_t));
            dessert_periodic_add_delayed(ara_routefail_untrap_packets, perdata, trapretry_delay);
        }
        return (DESSERT_MSG_DROP);
    } 
    else if(msg->u8 & ARA_ROUTEFAIL) 
    /* route fail to flood in */
    {
         msg->ttl--;

         if(msg->ttl>0) {
             dessert_meshsend_fast(msg, NULL);
         }
         return (DESSERT_MSG_DROP);         
    }
    else
    /* generate new route fail */
    {
        /* send route error */
        dessert_msg_t *fmsg;
        dessert_ext_t *ext = NULL;
        ara_ext_routefail_t *rf = NULL;
        struct ether_header *eth;
        
        dessert_msg_clone(&fmsg, msg, 0);
        
        fmsg->u8 |= ARA_ROUTEFAIL;

        eth = dessert_msg_getl25ether(fmsg);
        memcpy((fmsg->l2h.ether_dhost), ether_broadcast, sizeof(ETHER_ADDR_LEN)); 

        /* add routefail extension (for loop protection) */
        dessert_msg_addext(fmsg, &ext, ARA_EXT_ROUTEFAIL, sizeof(ara_ext_routefail_t));
        rf = (ara_ext_routefail_t *) ext->data;
        rf->seq = htons(ara_seq_next());
        memcpy(&(rf->src), dessert_l25_defsrc, sizeof(ara_address_t));

        dessert_meshsend_fast(fmsg, NULL);

        return (DESSERT_MSG_DROP);      
    }
        
}


/** task to untrap all packets for a specific use by ara_routefail */
int ara_routefail_untrap_packets(void *data, struct timeval *scheduled, struct timeval *interval) 
{
    ara_address_t *dst = (ara_address_t *) data;
    untrap_packets(*dst, ara_retrypacket);
    free(data);
    return(0);   
}


/** call ara_getroute/ara_forward/ara_routefail */
int ara_retrypacket(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id) 
{
    
    int r;
    
    r = ara_getroute(msg, len, proc, iface_in, id);
    if(r <= DESSERT_MSG_DROP) return(r);
    
    r = ara_forward(msg, len, proc, iface_in, id);
    if(r <= DESSERT_MSG_DROP) return(r);
    
    r = ara_routefail(msg, len, proc, iface_in, id);
    if(r <= DESSERT_MSG_DROP) return(r);
    
    return(r);
    
} 


/** handle incoming forward ant or packet with ARA_ROUTEPROBLEM set */
int ara_handle_fant(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface_in, dessert_frameid_t id) 
{
    ara_proc_t *ap = ara_proc_get(proc);
    dessert_ext_t  *ext_in;
    dessert_ext_t  *ext;
    dessert_msg_t* bant;
    struct ether_header *eth;
    int res;
    #ifdef ARA_ANT_DEBUG
    char buf[1024];
    #endif
    
    res = dessert_msg_getext(msg, &ext_in, ARA_EXT_FANT, 0);
    if(res < 1 && (ap->flags & ARA_ROUTEPROBLEM) == 0) {
        /* no FANT, no routeproblem */
        return (DESSERT_MSG_KEEP);
    }
    else if ( (ap->flags & ARA_ROUTEPROBLEM) && 
                (proc->lflags & DESSERT_LFLAG_DST_BROADCAST ||
                 proc->lflags & DESSERT_LFLAG_DST_MULTICAST )) 
    {
        dessert_warn("recvieved multicast or broadcast packet with ARA_ROUTEPROBLEM set - this should never happen - droping");
        return (DESSERT_MSG_DROP);
    }
    else if (((ap->flags & ARA_LOCAL) && (ap->flags & ARA_ROUTEPROBLEM)) ||
               (res == 1 && (memcmp(dessert_l25_defsrc, ext_in->data, ETHER_ADDR_LEN) == 0))) {
        
        dessert_msg_new(&bant);
        ara_addseq(bant);
        bant->ttl = ara_defttl;
        bant->u8 |= ARA_ANT;
        
        dessert_msg_addext(bant, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
        eth = (struct ether_header*) ext->data;
        memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
        memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);
        
        dessert_msg_addext(bant, &ext, ARA_EXT_BANT, ETHER_ADDR_LEN+4);
        memcpy(ext->data, ap->src, sizeof(ara_address_t));
        memcpy(ext->data+ETHER_ADDR_LEN, "BANT", 4);
        
        /* for loop vs duplicate detection */
        dessert_msg_trace_initiate(bant, DESSERT_MSG_TRACE_HOST);
        
        #ifdef ARA_ANT_DEBUG
        dessert_msg_dump(bant, DESSERT_MAXFRAMELEN, buf, 1024);
        dessert_debug("sending bant:\n%s", buf);
        #endif
        
        dessert_meshsend_fast(bant, NULL);
        dessert_msg_destroy(bant);
        
    }
    return (DESSERT_MSG_KEEP);
}

/** CLI command - config mode - interfcae tap $iface, $ipv4-addr, $netmask */
int cli_cfgtapif(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    char buf[255];
    int i;
    
    if(argc != 3) {
        cli_print(cli, "usage %s [tap-interface] [ip-address] [netmask]\n", command);
        return CLI_ERROR;
    }
    dessert_info("initalizing tap interface");
    dessert_tunif_init(argv[0], DESSERT_TAP|DESSERT_MAKE_DEFSRC);
    sprintf(buf, "ifconfig %s %s netmask %s mtu 1300 up", argv[0], argv[1], argv[2]);
    i = system(buf);
    dessert_info("running ifconfig on tap interface returned %d", i);
    return (i==0?CLI_OK:CLI_ERROR);
}

/** CLI command - config mode - interfcae mesh $iface */
int cli_addmeshif(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    char buf[255];
    int i;
    
    if(argc != 1) {
        cli_print(cli, "usage %s [mesh-interface]\n", command);
        return CLI_ERROR;
    }
    dessert_info("initalizing mesh interfcae %s", argv[0]);
    dessert_meshif_add(argv[0], DESSERT_IF_NOPROMISC);
    sprintf(buf, "ifconfig %s up", argv[0]);
    i = system(buf);
    dessert_info("running ifconfig on mesh interface %s returned %d",argv[0], i);
    return (i==0?CLI_OK:CLI_ERROR);
}

