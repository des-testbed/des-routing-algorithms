/******************************************************************************
 Copyright 2009, Bastian Blywis, Sebastian Hofmann, Freie Universitaet Berlin
(FUB).
 All rights reserved.

 These sources were originally developed by Bastian Blywis
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

#include "gossiping.h"
#include "gossiping_nhdp.h"

uint8_t logfw = 1;
uint8_t logrx = 1;
uint8_t logtx = 1;
uint8_t lognhdphellotx = 1;
uint8_t lognhdphellorx = 1;
extern dessert_sysif_t* _dessert_sysif;
extern dessert_meshif_t* _dessert_meshiflist;
extern uint8_t restarts;
extern struct timeval gossip13_restart_interval;
char hostname[20];

int cli_settag(struct cli_def *cli, char *command, char *argv[], int argc) {
    char* s = "none";
    if(argc > 0) {
        s = argv[0];
    }
    dessert_info("[tag] host=%s, id=%s, p=%f, p2=%f, k=%d, n=%d, m=%d, timeout=%d, gossip=%d, helloTTL=%d, hello_interval=%d, helloSize=%d, cleanup_interval=%d, T_MAX=%d, p_min=%f, p_max=%f, nhdp_hi=%d, nhdp_ht=%d, mpr_minpdr=%f, restarts=%u, tau=%u", hostname, s, p, p2, k, n, m, timeout.tv_sec*1000+timeout.tv_usec/1000, gossip, helloTTL, hello_interval.tv_sec*1000+hello_interval.tv_usec/1000, hello_size, cleanup_interval.tv_sec*1000+cleanup_interval.tv_usec/1000, T_MAX_ms, p_min, p_max, NHDP_HELLO_INTERVAL.tv_sec*1000+NHDP_HELLO_INTERVAL.tv_usec/1000, H_HOLD_TIME.tv_sec*1000+H_HOLD_TIME.tv_usec/1000, mpr_minpdr, restarts, dessert_timeval2ms(&gossip13_restart_interval));
    return CLI_OK;
}

void initLogger() {
    FILE* f = fopen("/proc/sys/kernel/hostname", "r");
    if(f) {
        fscanf(f, "%20s", hostname);
        fclose(f);
    }
    cli_register_command(dessert_cli, dessert_cli_set, "tag", cli_settag, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set a tag");
}

int logRX(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(logrx && !((msg->u8 & HELLO) || (msg->u8 & NHDP_HELLO))) {
        u_char* l25_shost = dessert_msg_getl25ether(msg)->ether_shost;
        u_char* l25_dhost = dessert_msg_getl25ether(msg)->ether_dhost;
        u_char* l2_shost  = msg->l2h.ether_shost;
        uint16_t seq = msg->u16;

        uint8_t hops = 0;
        dessert_ext_t* ext = NULL;
        if(dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
            gossip_ext_hops_t* str = (gossip_ext_hops_t*) ext->data;
            hops = str->hops;
            if(gossip == gossip_13) {
                seq = str->seq2;
            }
        }
#ifndef ANDROID
        avg_node_result_t rssi = dessert_rssi_avg(l2_shost, iface);
        dessert_info("[rx] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x, hops=%d, rssi=%d",
                hostname, EXPLODE_ARRAY6(iface->hwaddr), iface->if_name, EXPLODE_ARRAY6(l2_shost), EXPLODE_ARRAY6(l25_shost), EXPLODE_ARRAY6(l25_dhost), msg->ttl, seq, msg->u8, hops, rssi.avg_rssi);
#else
        dessert_info("[rx] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x, hops=%d",
                hostname, EXPLODE_ARRAY6(iface->hwaddr), iface->if_name, EXPLODE_ARRAY6(l2_shost), EXPLODE_ARRAY6(l25_shost), EXPLODE_ARRAY6(l25_dhost), msg->ttl, seq, msg->u8, hops);
#endif

    }
    return DESSERT_MSG_KEEP;
}

void logTX(dessert_msg_t* msg, dessert_meshif_t *iface) {
    if(logtx) {
        extern dessert_sysif_t* _dessert_sysif;
        u_char* shost = _dessert_sysif->hwaddr;
        u_char* dhost = dessert_msg_getl25ether(msg)->ether_dhost;
        uint16_t seq = msg->u16;

        dessert_ext_t* ext = NULL;
        if(gossip == gossip_13 && dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
            gossip_ext_hops_t* str = (gossip_ext_hops_t*) ext->data;
            seq = str->seq2;
        }

        if(iface != NULL) { // single interface
            u_char* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[tx] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x",
                hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(shost),
                EXPLODE_ARRAY6(dhost), msg->ttl, seq, msg->u8);
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                u_char* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[tx] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x",
                    hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(shost),
                    EXPLODE_ARRAY6(dhost), msg->ttl, seq, msg->u8);
            }
        }
    }
}

int logForwarded(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface) {
    if(logfw) {
        u_char* l25_shost = dessert_msg_getl25ether(msg)->ether_shost;
        u_char* l25_dhost = dessert_msg_getl25ether(msg)->ether_dhost;
        uint16_t seq = msg->u16;

        dessert_ext_t* ext = NULL;
        if(gossip == gossip_13 && dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
            gossip_ext_hops_t* str = (gossip_ext_hops_t*) ext->data;
            seq = str->seq2;
        }

        if(iface != NULL) { // single interface
            uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[fw] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x",
                          hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l25_shost),
                          EXPLODE_ARRAY6(l25_dhost), msg->ttl, seq, msg->u8);
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[fw] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x",
                             hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l25_shost),
                             EXPLODE_ARRAY6(l25_dhost), msg->ttl, seq, msg->u8);
            }
        }
    }
    return DESSERT_MSG_KEEP;
}

int logHelloFW(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface) {
    if(logtx) {
        u_char* l25_shost = dessert_msg_getl25ether(msg)->ether_shost;
        u_char* l2_shost = msg->l2h.ether_shost;
        if(iface != NULL) { // single interface
            uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[fh] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", ttl=%d, seq=%d, flags=%#x",
                         hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l2_shost),
                         EXPLODE_ARRAY6(l25_shost), msg->ttl, msg->u16, msg->u8);
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[fh] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", ttl=%d, seq=%d, flags=%#x",
                             hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l2_shost),
                             EXPLODE_ARRAY6(l25_shost), msg->ttl, msg->u16, msg->u8);
            }
        }
    }
    return DESSERT_MSG_KEEP;
}

int logHello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface) {
    if(logtx) {
        if(iface != NULL) { // single interface
            uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[he] host=%s, if=" MAC " (%s), ttl=%d, seq=%d, flags=%#x",
                         hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, msg->ttl, msg->u16, msg->u8);
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[he] host=%s, if=" MAC " (%s), ttl=%d, seq=%d, flags=%#x",
                             hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, msg->ttl, msg->u16, msg->u8);
            }
        }
    }
    return DESSERT_MSG_KEEP;
}

int logadaptivep(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface, double probability) {
    if(logtx) {
        u_char* l25_shost = dessert_msg_getl25ether(msg)->ether_shost;
        u_char* l25_dhost = dessert_msg_getl25ether(msg)->ether_dhost;

        if(iface != NULL) { // single interface
            uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[ap] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x, p=%f",
                         hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l25_shost),
                         EXPLODE_ARRAY6(l25_dhost), msg->ttl, msg->u16, msg->u8, probability);
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[ap] host=%s, if=" MAC " (%s), src=" MAC ", dst=" MAC ", ttl=%d, seq=%d, flags=%#x, p=%f",
                             hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, EXPLODE_ARRAY6(l25_shost),
                             EXPLODE_ARRAY6(l25_dhost), msg->ttl, msg->u16, msg->u8, probability);
            }
        }
    }
    return DESSERT_MSG_KEEP;
}

void logNhdpHelloTx(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface, uint8_t num_neighbors, uint8_t num_2hop_neighbors, uint8_t num_mprs, uint8_t num_mpr_selectors) {
    if(lognhdphellotx) {
        if(iface != NULL) { // single interface
            uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
            dessert_info("[nhdp_he] host=%s, if=" MAC " (%s), ttl=%d, seq=%d, flags=%#x, n=%d, n2=%d, mpr=%d, mprs=%d, size=%d",
                    hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, msg->ttl, msg->u16, msg->u8,
                    num_neighbors, num_2hop_neighbors, num_mprs, num_mpr_selectors, ntohs(msg->hlen) + ntohs(msg->plen));
        }
        else { // all interfaces
            dessert_meshif_t* iface;
            for(iface = _dessert_meshiflist; iface != NULL; iface=iface->next) {
                uint8_t* ifaddr = (uint8_t*) iface->hwaddr;
                dessert_info("[nhdp_he] host=%s, if=" MAC " (%s), ttl=%d, seq=%d, flags=%#x, n=%d, n2=%d, mpr=%d, mprs=%d size=%d",
                        hostname, EXPLODE_ARRAY6(ifaddr), iface->if_name, msg->ttl, msg->u16, msg->u8,
                        num_neighbors, num_2hop_neighbors, num_mprs, num_mpr_selectors, msg->hlen + msg->plen);
            }
        }
    }
}

void logNhdpHelloRx(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t *iface) {
    if(lognhdphellorx) {
        u_char* shost;
        u_char* l2_shost;
        extern char hostname[20];
        avg_node_result_t rssi;
        uint8_t hops;
        dessert_ext_t* ext;

        shost = dessert_msg_getl25ether(msg)->ether_shost;;
        l2_shost = msg->l2h.ether_shost;
        rssi = dessert_rssi_avg(l2_shost, iface);
        hops = 0;
        ext = NULL;
        if(dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
            gossip_ext_hops_t* str = (gossip_ext_hops_t*) ext->data;
            hops = str->hops;
        }
        dessert_info("[nhdp_rh] host=%s, if=" MAC " (%s), prev=" MAC ", src=" MAC ", ttl=%d, seq=%d, flags=%#x, hops=%d, rssi=%d",
            hostname,
            EXPLODE_ARRAY6(iface->hwaddr),
            iface->if_name,
            EXPLODE_ARRAY6(l2_shost),
            EXPLODE_ARRAY6(shost),
            msg->ttl, msg->u16, msg->u8, hops, rssi.avg_rssi);
    }
}
