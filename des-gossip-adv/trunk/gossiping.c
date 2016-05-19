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
#include "build.h"
#ifndef ANDROID
#include <printf.h>
#endif

static void version(int argc, char** argv) {
    char* str = malloc(strlen(argv[0])+1);
    if(!str) {
        dessert_crit("could not allocate memory");
        return;
    }

    strcpy(str, argv[0]);

    if((argc ==2) && (!strcmp ("-v", argv[1]) || !strcmp ("--version", argv[1]))) {
        printf("Version:\t %s\n", VERSION_STR);
        printf("Build Date:\t %s\n", VERSION_DATE);
        printf("Build Name:\t %s\n", VERSION_NAME);
        free(str);
        exit(0);
    }
    free(str);
}

void register_names() {
    dessert_register_ptr_name(handleTrappedPacket, "handleTrappedPacket");
    dessert_register_ptr_name(nhdp_send_hello, "nhdp_send_hello");
    dessert_register_ptr_name(cleanup_neighbors, "cleanup_neighbors");
    dessert_register_ptr_name(send_hello, "send_hello");
#ifndef ANDROID
    dessert_register_ptr_name(gossip13_generate_data, "gossip13_generate_data");
    dessert_register_ptr_name(gossip13_restart, "gossip13_restart");
    dessert_register_ptr_name(gossip13_update_data, "gossip13_update_data");
    dessert_register_ptr_name(gossip13_store_observations, "gossip13_store_observations");
    dessert_register_ptr_name(gossip13_cleanup_observations, "gossip13_cleanup_observations");
#endif
}

int main(int argc, char** argv) {
    version(argc, argv);
    FILE *cfg = dessert_cli_get_cfg(argc, argv);

    srand(time(NULL));
    extern uint16_t seq;
    seq = (uint16_t) random();

    register_names();
    dessert_init("GADV", 0x03, DESSERT_OPT_DAEMONIZE);
    dessert_logcfg(DESSERT_LOG_NOSYSLOG|DESSERT_LOG_NOSTDERR);
    dessert_notice("\n     DES-GOSSIP-ADV[%s] v%s - %s\n", VERSION_NAME, VERSION_STR, VERSION_DATE);
    initLogger();
    init_cli();
    init_nhdp();

    dessert_sysrxcb_add((dessert_sysrxcb_t*) dessert_sys_drop_ipv6, 1);
    dessert_sysrxcb_add(sendToNetwork, 100);

    /* general packet handling */
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 11);
    dessert_meshrxcb_add(drop_zero_mac, 12);
    dessert_meshrxcb_add((dessert_meshrxcb_t*) logRX, 13);

    /* loop/duplicate check, handling of HELLOs */
    dessert_meshrxcb_add((dessert_meshrxcb_t*) checkSeq, 20);             // drops if duplicate or packet sent by this host
    dessert_meshrxcb_add((dessert_meshrxcb_t*) handleHello, 21);          // drops HELLOs if TTL == 0, otherwise forwards them
    dessert_meshrxcb_add((dessert_meshrxcb_t*) nhdp_handle_hello, 22);    // drops all NHDP hellos
#ifndef ANDROID
    dessert_meshrxcb_add((dessert_meshrxcb_t*) gossip13_eval_update, 23); // never drops
#endif

    /* probabilistic forwarding or delivery */
    dessert_meshrxcb_add((dessert_meshrxcb_t*) deliver, 90);              // drops all messages for this host
    dessert_meshrxcb_add((dessert_meshrxcb_t*) floodOnFirstKHops, 92);    // drops if first k hops and forwarded
    dessert_meshrxcb_add((dessert_meshrxcb_t*) forward, 100);             // always drops
    dessert_info("callbacks registered");

    // we need no password; use password: cli_allow_enable(dessert_cli, "gossip");
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_info("configuration applied");

    dessert_cli_run();
    dessert_run();

    return (0);
}

