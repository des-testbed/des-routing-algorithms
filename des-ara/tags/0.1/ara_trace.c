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

struct cli_def *pingcons;

/** handle ping, pong and traceroute for gossiping **/
int ara_pingpong(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) 
{
    dessert_ext_t *ext;
    dessert_msg_proc_t nproc;
    ara_proc_t *ap;
    
    struct ether_header *l25h;
    u_char temp[ETHER_ADDR_LEN];
    char text[DESSERT_MAXEXTDATALEN+1];
    
    l25h = dessert_msg_getl25ether(msg);
    
    if(l25h == NULL) {
        /* dont' know what to do - just ignore */
        return DESSERT_MSG_KEEP;
    }
    /* answer a ping with a pong */
    else if (proc->lflags & DESSERT_LFLAG_DST_SELF && dessert_msg_getext(msg, &ext, ARA_PING, 0)) 
    {
        memset(text, 0x0, DESSERT_MAXEXTDATALEN+1);
        memcpy(text, ext->data, dessert_ext_getdatalen(ext));
        
        dessert_debug("got ping packet from %x:%x:%x:%x:%x.%x (%s)- sending pong",
            l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
            l25h->ether_shost[3], l25h->ether_shost[4], l25h->ether_shost[5],
            text);
            
        /* swap l2.5 src/dest */
        memcpy(temp, l25h->ether_shost, ETHER_ADDR_LEN);
        memcpy(l25h->ether_shost, l25h->ether_dhost, ETHER_ADDR_LEN);
        memcpy(l25h->ether_dhost, temp, ETHER_ADDR_LEN);
        /* make ping to pong */
        ext->type = ARA_PONG;
        
        ap = ara_proc_get(&nproc);
        memset(&nproc, 0x0, DESSERT_MSGPROCLEN);
        ara_addseq(msg);
        ap->flags |= ARA_ORIG_LOCAL;
        dessert_meshrxcb_runall(msg, DESSERT_MAXFRAMEBUFLEN, &nproc, NULL ,id);
        
        /* we're doen with this packet */
        return DESSERT_MSG_DROP;
    }
    /* process pong */
    else if (proc->lflags & DESSERT_LFLAG_DST_SELF && dessert_msg_getext(msg, &ext, ARA_PONG, 0)) 
    {
        char buf[1024];
        int i;
        
        memset(buf, 0x0, 1024);
        memset(text, 0x0, DESSERT_MAXEXTDATALEN+1);
        memcpy(text, ext->data, dessert_ext_getdatalen(ext));
        
        i = dessert_msg_trace_dump(msg, buf, 1024);
        
        if(i < 1) {
            dessert_debug("got pong packet from %x:%x:%x:%x:%x.%x (%s)",
                l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
                l25h->ether_shost[3], l25h->ether_shost[4], l25h->ether_shost[5],
                text);
            if(pingcons != NULL)
                cli_print(pingcons, "\ngot pong packet from %x:%x:%x:%x:%x.%x (%s)",
                        l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
                        l25h->ether_shost[3], l25h->ether_shost[4], l25h->ether_shost[5],
                        text);
        } else {
            dessert_debug("got pong packet from %x:%x:%x:%x:%x.%x (%s)\n%s",
                l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
                l25h->ether_shost[3], l25h->ether_shost[4], l25h->ether_shost[5],
                text, buf);
            if(pingcons != NULL)
                cli_print(pingcons, "\ngot pong packet from %x:%x:%x:%x:%x.%x (%s)\n%s",
                    l25h->ether_shost[0], l25h->ether_shost[1], l25h->ether_shost[2],
                    l25h->ether_shost[3], l25h->ether_shost[4], l25h->ether_shost[5],
                    text, buf);
        }
        
        /* we're doen with this packet */
        return DESSERT_MSG_DROP;
    }
    /* no ping - no pong - a packet for someone else */
    else 
        return DESSERT_MSG_KEEP;
}


/** CLI command - traceroute */
int cli_traceroute(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    u_char ether_trace[ETHER_ADDR_LEN];
    dessert_msg_t *msg;
    dessert_msg_proc_t proc;
    ara_proc_t *ap;
    dessert_ext_t *ext;
    struct ether_header *l25h;
    
    if( argc<1 || argc >2 ||
        sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
            &ether_trace[0], &ether_trace[1], &ether_trace[2],
            &ether_trace[3], &ether_trace[4], &ether_trace[5]) != 6
    ) {
        cli_print(cli, "usage %s [mac-address in xx:xx:xx:xx:xx:xx notation] ([i])\n", command);
        return CLI_ERROR;
    }
    cli_print(cli, "sending trace packet to %x:%x:%x:%x:%x:%x...\n", 
         ether_trace[0], ether_trace[1], ether_trace[2],
         ether_trace[3], ether_trace[4], ether_trace[5]);
    dessert_info("sending trace packet to %x:%x:%x:%x:%x:%x", 
          ether_trace[0], ether_trace[1], ether_trace[2],
          ether_trace[3], ether_trace[4], ether_trace[5]);
    
    dessert_msg_new(&msg);
    
    /* add ethernet l2.5 header */
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    l25h = (struct ether_header *) ext->data;
    memcpy(l25h->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(l25h->ether_dhost, ether_trace, ETHER_ADDR_LEN);
    l25h->ether_type = htons(0x0000);
    
    /* add ping ext */
    l25h = NULL;
    dessert_msg_addext(msg, &ext, ARA_PING, 10);
    memcpy(ext->data, "traceroute", 10);
    
    /* add trace header */
    if(argc == 2 && argv[1][0] == 'i') {
        dessert_msg_trace_initiate(msg, DESSERT_MSG_TRACE_IFACE);
    } else {
        dessert_msg_trace_initiate(msg, DESSERT_MSG_TRACE_HOST);
    }
    

    /* prepair to send normally and send */
    ap = ara_proc_get(&proc);
    memset(&proc, 0x0, DESSERT_MSGPROCLEN);
    ara_addseq(msg);
    ap->flags |= ARA_ORIG_LOCAL;
    dessert_meshrxcb_runall(msg, DESSERT_MAXFRAMEBUFLEN, &proc, NULL , dessert_newframeid());
    dessert_msg_destroy(msg);
    
    pingcons = cli;
    
    return CLI_OK;
}

/** CLI command - ping */
int cli_ping(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    u_char ether_trace[ETHER_ADDR_LEN];
    dessert_msg_t *msg;
    dessert_msg_proc_t proc;
    ara_proc_t *ap;
    dessert_ext_t *ext;
    struct ether_header *l25h;
    
    if( argc<1 || argc >2 ||
        sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
            &ether_trace[0], &ether_trace[1], &ether_trace[2],
            &ether_trace[3], &ether_trace[4], &ether_trace[5]) != 6
    ) {
        cli_print(cli, "usage %s [mac-address in xx:xx:xx:xx:xx:xx notation] ([text])\n", command);
        return CLI_ERROR;
    }
    cli_print(cli, "sending ping packet to %x:%x:%x:%x:%x:%x...\n", 
         ether_trace[0], ether_trace[1], ether_trace[2],
         ether_trace[3], ether_trace[4], ether_trace[5]);
    dessert_info("sending ping packet to %x:%x:%x:%x:%x:%x", 
          ether_trace[0], ether_trace[1], ether_trace[2],
          ether_trace[3], ether_trace[4], ether_trace[5]);
    
    dessert_msg_new(&msg);
    
    /* add ethernet l2.5 header */
    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    l25h = (struct ether_header *) ext->data;
    memcpy(l25h->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(l25h->ether_dhost, ether_trace, ETHER_ADDR_LEN);
    l25h->ether_type = htons(0x0000);
    
    /* set ping string */
    if(argc == 2) {
        int len = strlen(argv[1]);
        len = len>DESSERT_MAXEXTDATALEN?DESSERT_MAXEXTDATALEN:len;
        dessert_msg_addext(msg, &ext, ARA_PING, len);
        memcpy(ext->data, argv[1], len);
    } else {
        dessert_msg_addext(msg, &ext, ARA_PING, 10);
        memcpy(ext->data, "ping?pong!", 10);
    }
    
    /* add ping ext */
    l25h = NULL;
    
    /* prepair to send normally and send */
    ap = ara_proc_get(&proc);
    memset(&proc, 0x0, DESSERT_MSGPROCLEN);
    ara_addseq(msg);
    ap->flags |= ARA_ORIG_LOCAL;
    dessert_meshrxcb_runall(msg, DESSERT_MAXFRAMEBUFLEN, &proc, NULL , dessert_newframeid());
    dessert_msg_destroy(msg);
    
    pingcons = cli;
    
    return CLI_OK;
}
