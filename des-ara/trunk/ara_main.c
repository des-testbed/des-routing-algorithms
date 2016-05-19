/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
 Extended and debugged by Bastian Blywis, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by Philipp Schmidt
 at Freie Universitaet Berlin (http://www.fu-berlin.de/) and
 modified by Bastian Blywis
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
#include <signal.h>
#ifndef ANDROID
#include <printf.h>
#endif

/* Handler for SIGTERM
 *
 * \todo Send some special, new ANT to signal that the daemon is about to get shutdown
 *
void ara_sigterm(int signal) {
    dessert_info("received SIGTERM");
}*/

int dessert_msg_ifaceflags_cb_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_sysif_t* iface_in, dessert_frameid_t id) {
    return dessert_msg_ifaceflags_cb(msg, len, proc, (dessert_meshif_t*) iface_in, id);
}

static void ara_register_cb() {
    dessert_debug("registering callbacks");
    dessert_sysrxcb_add(ara_tun2ara, 1);
    dessert_sysrxcb_add(dessert_msg_ifaceflags_cb_sys, 20);
    dessert_sysrxcb_add(ara_makeproc_sys, 40);

    dessert_sysrxcb_add(ara_addext_classification, 45); // add data for path classification

    dessert_sysrxcb_add(ara_getroute_sys, 50); // lookup route, set next hop
    dessert_sysrxcb_add(ara_sendfant, 90); // sends FANT on route fail, packet will be trapped
    dessert_sysrxcb_add(ara_sendbant, 98); // sends a BANT first if there is a route but we never send a BANT before, e.g, got an ARP request instead
    dessert_sysrxcb_add(ara_forward_sys, 99);
    dessert_sysrxcb_add(ara_maintainroute_timestamp, 100); // writes timestamp for route maintenance

    /****************************************************************************/

    /* check & tag */
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 11);
    dessert_meshrxcb_add(ara_makeproc, 12);
    dessert_meshrxcb_add(ara_checkl2dst, 13);
    dessert_meshrxcb_add(ara_handle_ack_request, 14);   // always send responses, even if we receive the request multiple times; the duplplicate/loop detection will take care of anything else

    dessert_meshrxcb_add(dessert_mesh_ipttl, 34);       // to enable traceroute
    dessert_meshrxcb_add(ara_checkloop, 35);            // drops looping packets
    dessert_meshrxcb_add(ara_checkdupe, 36);            // drops or marks packet duplicates

    /* handle ants and special packets */
    dessert_meshrxcb_add(ara_handle_loops, 50);         // deletes routes for returned/looping packets
    dessert_meshrxcb_add(ara_handle_routefail, 51);     // deletes routes for returned packets with ARA_ROUTEFAIL
    dessert_meshrxcb_add(ara_handle_fant, 52);          // sends BANT on FANT reception

    /* route updates */
    dessert_meshrxcb_add(ara_routeupdate_ant, 60);      // Add route or increase ptrail if packet is an ANT
    dessert_meshrxcb_add(ara_routeupdate_data, 61);     // Increases ptrail for data packets

    /* cleanup */
    dessert_meshrxcb_add(ara_dropdupe, 70);             // drop duplicates (no looping packets!)

    /* classification*/
    dessert_meshrxcb_add(ara_updateext_classification, 72);

    /* forward packet */
    dessert_meshrxcb_add(dessert_msg_trace_cb, 75);
    dessert_meshrxcb_add(ara_getroute, 80);
    dessert_meshrxcb_add(ara_noroute, 90);
    dessert_meshrxcb_add(ara_maintainroute_pant, 95);
    dessert_meshrxcb_add(ara_forward, 98);
    dessert_meshrxcb_add(ara_handle_routeproblem, 99);
    dessert_meshrxcb_add(ara_ara2tun, 100);
    dessert_debug("callbacks registered");
}

static void ara_register_tasks() {
    dessert_register_ptr_name(ara_rt_tick, "ara_rt_tick");
    dessert_register_ptr_name(ara_print_rt_periodic, "ara_print_rt_periodic");
    dessert_register_ptr_name(ara_print_cl_periodic, "ara_print_cl_periodic");
    dessert_register_ptr_name(ara_routefail_untrap_packets, "ara_routefail_untrap_packets");
    dessert_register_ptr_name(ara_loopprotect_tick, "ara_loopprotect_tick");
    dessert_register_ptr_name(ara_classification_tick, "ara_classification_tick");
    dessert_register_ptr_name(ara_ack_tick, "ara_ack_tick");
}


int main(int argc, char** argv) {
    FILE* cfg = dessert_cli_get_cfg(argc, argv);
    dessert_init("ARAX", (VERSION_MAJOR * 10) + VERSION_MINOR, DESSERT_OPT_DAEMONIZE);
    dessert_logcfg(DESSERT_LOG_STDERR);
    ara_init_cli();

    ara_rt_init();
    ara_loopprotect_init();
    ara_classification_init();
    ara_ack_init();

    ara_register_cb();
    ara_register_tasks();

    dessert_debug("applying config");
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_debug("config applied");

    //     dessert_signalcb_add(SIGTERM, ara_sigterm);

    dessert_cli_run();
    dessert_run();

    return (0);
}


