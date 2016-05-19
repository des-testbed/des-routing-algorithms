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

#define TRAP_SLOTCHUNKSIZE 32

/* local variables */
int trapretry_max = 8;
int trapretry_delay = 10;

/** a singel packet in a trap */
typedef struct trapped_packet {
    /** the message */
    dessert_msg_t    *pkg;
    /** the message buffer length */
    size_t len;
    /** message processing info */
    dessert_msg_proc_t *proc;
    /** the frameid     */
    dessert_frameid_t id;
} trapped_packet_t;

/** packet trap to put packets in */
typedef struct packettrap {
    /** dst_address in unknown format (we don't need to care) */
    ara_address_t    dst;
    /** NULL terminated List of packets waiting - 
      * buflen = TRAP_SLOTCHUNKSIZE*(1+(#waiting/TRAP_SLOTCHUNKSIZE)) */
    trapped_packet_t    *pkgs;
    /** handle for hastable usage */
    UT_hash_handle hh;
} packettrap_t;

packettrap_t *traped_packets = NULL;
pthread_rwlock_t traped_packets_lock = PTHREAD_RWLOCK_INITIALIZER;

/** trap an undeliverable packet 
  * @arg dst destination with discovery waiting
  * @arg pkg packet to trap - pointer will simply be copied, so you MUST NOT free it
  * @returns count of packets now trapped for this dst, -1 if packet was discarded
  */
int trap_packet(ara_address_t dst, dessert_msg_t *pkg, size_t len, dessert_msg_proc_t *proc, dessert_frameid_t id) 
{
    
    ara_proc_t *ap = ara_proc_get(proc);
    packettrap_t *mytrap = NULL;
    trapped_packet_t *pkti = NULL;
    int i = 0;
    
    /* check if the package hat been trapped too often */
    assert(proc != NULL);
    if(++(ap->trapcount) > trapretry_max) {
        #ifdef PACKETTRAP_DEBUG
        dessert_debug("discarding packet %d to %02x:%02x:%02x:%02x:%02x:%02x after %d times in trap",
            ap->seq, dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], (ap->trapcount));
        #endif
        return(-1);
    } else {
        #ifdef PACKETTRAP_DEBUG
        dessert_debug("trapping packet %d to %02x:%02x:%02x:%02x:%02x:%02x for the %d. time",
            ap->seq, dst[0], dst[1], dst[2], dst[3], dst[4], dst[5], (ap->trapcount));
        #endif
    }
    
    /* lock the trap */
    pthread_rwlock_wrlock(&traped_packets_lock);
    
    /* look up dst in hashtable - otherwise build list */
    HASH_FIND(hh, traped_packets, dst, sizeof(dst), mytrap);
    
    /* no packets already trapped - build trap... */
    if(mytrap == NULL) {
        
        /* make the trap */
        mytrap = malloc(sizeof(packettrap_t));
        if(mytrap == NULL) {
            dessert_err("failed to allocate new hash table entry for packet trap");
            goto trap_packet_out;
        }
        
        memcpy((mytrap->dst), dst, sizeof(ara_address_t));
        
        mytrap->pkgs = malloc(TRAP_SLOTCHUNKSIZE*sizeof(trapped_packet_t));
        if(mytrap->pkgs == NULL) {
            dessert_err("failed to allocate new hash table entry for packet trap");
            free(mytrap);
            goto trap_packet_out;
        }
        mytrap->pkgs->pkg = NULL;
        mytrap->pkgs->len = 0;
        mytrap->pkgs->proc = NULL;
        mytrap->pkgs->id = 0;
        
        HASH_ADD_KEYPTR(hh, traped_packets, &(mytrap->dst), sizeof(dst), mytrap);
        
    }
    
    /* look for free slot */
    i = 0;
    for(pkti = mytrap->pkgs; pkti->pkg != NULL; pkti++) i++;
    
    /* do we need to grow ? */
    if(i%TRAP_SLOTCHUNKSIZE == TRAP_SLOTCHUNKSIZE-1) {
        pkti = realloc(mytrap->pkgs, (i+TRAP_SLOTCHUNKSIZE+1)*sizeof(trapped_packet_t));
        if(pkti == NULL) {
            dessert_err("failed to modify hash table entry for packet trap");
            goto trap_packet_out;
        }
        mytrap->pkgs = pkti;
        pkti+=i;
    }
    
    /* copy/insert packet */
    dessert_msg_clone(&(pkti->pkg), pkg, 0);
    dessert_msg_proc_clone(&(pkti->proc), proc);
    pkti->len = len;
    pkti->id = id;
    
    /* fix list */
    pkti++; i++;
    pkti->pkg = NULL;
    pkti->len = 0;
    pkti->proc = NULL;
    pkti->id = 0;
    
    /* done! */
    
    trap_packet_out:
    pthread_rwlock_unlock(&traped_packets_lock);
    return(i);
}

/** untrap packets that have become deliverable
  * @arg dst destination with discovery waiting
  * @arg c callback to give packets to for resend
  * @returns count of packets now trapped for this dst 
  */
int untrap_packets(ara_address_t dst, dessert_meshrxcb_t* c) 
{

    packettrap_t *mytrap = NULL;
    trapped_packet_t *mypkt = NULL;
    int i = 0;
    
    pthread_rwlock_wrlock(&traped_packets_lock);
    HASH_FIND(hh, traped_packets, dst, sizeof(dst), mytrap);
    if(mytrap != NULL) {
        HASH_DELETE(hh, traped_packets, mytrap);
    }
    pthread_rwlock_unlock(&traped_packets_lock);
    
    /* untrap all packets */
    if(mytrap != NULL)
    {
        for(mypkt = mytrap->pkgs; mypkt->pkg != NULL; mypkt++) 
        {
            if(c != NULL) {
                c(mypkt->pkg, mypkt->len, mypkt->proc, NULL, mypkt->id);
            }
            dessert_msg_destroy(mypkt->pkg);
            dessert_msg_proc_destroy(mypkt->proc);
            i++;
        }
        free(mytrap->pkgs);
        free(mytrap);
    }
    
    return(i);
    
}

/** CLI command - config mode - interfcae tap $iface, $ipv4-addr, $netmask */
int cli_showpackettrap(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    packettrap_t *tr = NULL;

    pthread_rwlock_rdlock(&traped_packets_lock);
    
    for(tr = traped_packets; tr != NULL; tr=(tr->hh).next ) {
        
        trapped_packet_t *pkti = NULL;
        int i = 0;
        
        cli_print(cli, "\ndst=%x:%x:%x:%x:%x:%x:", 
            (tr->dst)[0], (tr->dst)[1], (tr->dst)[2],
            (tr->dst)[3], (tr->dst)[4], (tr->dst)[5]);
        
        for(pkti = tr->pkgs; pkti->pkg != NULL; pkti++) 
        {
            ara_proc_t *ap = ara_proc_get(pkti->proc);
            cli_print(cli, "\t#%04d seq=%06d trapcount=%03d",
                i++, ap->seq, ap->trapcount);
        }
        
    }

    pthread_rwlock_unlock(&traped_packets_lock);
    
    return CLI_OK;
}
