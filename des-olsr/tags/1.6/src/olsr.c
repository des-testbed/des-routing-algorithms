/******************************************************************************
Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

These sources were developed at the Freie Universitaet Berlin,
Computer Systems and Telematics / Distributed, embedded Systems (DES) group
(http://cst.mi.fu-berlin.de, http://www.des-testbed.net)
-------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/ .
--------------------------------------------------------------------------------
For further information and questions please use the web site
    http://www.des-testbed.net
*******************************************************************************/

#include <dessert.h>

#ifndef ANDROID
#include <printf.h>
#endif

#include "config.h"
#include "cli/olsr_cli.h"
#include "pipeline/olsr_pipeline.h"
#include "database/olsr_database.h"

uint16_t        hello_size              = HELLO_SIZE;
uint16_t        hello_interval_ms       = HELLO_INTERVAL_MS;
uint16_t        tc_size                 = TC_SIZE;
uint16_t        tc_interval_ms          = TC_INTERVAL_MS;
uint16_t        rt_interval_ms          = RT_INTERVAL_MS;
uint16_t        window_size             = WINDOW_SIZE;
uint16_t        max_missed_tc           = TC_HOLD_TIME_COEFF;
uint16_t        max_missed_hello        = LINK_HOLD_TIME_COEFF;
uint8_t         willingness             = WILL_DEFAULT;
olsr_metric_t   rc_metric               = RC_METRIC_ETX;

dessert_periodic_t* periodic_send_hello;
dessert_periodic_t* periodic_send_tc;
dessert_periodic_t* periodic_rt;

static void _register_cli_callbacks() {
    /* cli initialization */
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

    cli_register_command(dessert_cli, dessert_cli_set, "hello_size", cli_set_hello_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet size");
    cli_register_command(dessert_cli, dessert_cli_set, "hello_interval_ms", cli_set_hello_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO interval");
    cli_register_command(dessert_cli, dessert_cli_set, "tc_size", cli_set_tc_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set TC packet size");
    cli_register_command(dessert_cli, dessert_cli_set, "tc_interval_ms", cli_set_tc_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set TC interval");
    cli_register_command(dessert_cli, dessert_cli_set, "rt_interval_ms", cli_set_rt_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set routing table update interval");
    cli_register_command(dessert_cli, dessert_cli_set, "window_size", cli_set_window_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set link quality window size (PDR or ETX)");
    cli_register_command(dessert_cli, dessert_cli_set, "max_miss_tc", cli_set_max_missed_tc, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set limit for missed TCs");
    cli_register_command(dessert_cli, dessert_cli_set, "max_miss_hello", cli_set_max_missed_hello, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set limit for missed HELLOs");
    cli_register_command(dessert_cli, dessert_cli_set, "willingness", cli_set_willingness, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set willingness for MPR selection");
    cli_register_command(dessert_cli, dessert_cli_set, "metric", cli_set_rc_metric, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set metric (PLR | PDR | HC | ETX | ETX-ADD)");

    cli_register_command(dessert_cli, dessert_cli_show, "rt_interval_ms", cli_show_rt_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "show routing table update interval");
    cli_register_command(dessert_cli, dessert_cli_show, "max_miss_tc", cli_show_max_missed_tc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show limit for the max. number of missed TCs");
    cli_register_command(dessert_cli, dessert_cli_show, "max_miss_hello", cli_show_max_missed_hello, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show limit for the max. number of missed HELLOs");
    cli_register_command(dessert_cli, dessert_cli_show, "hello_size", cli_show_hello_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "hello_interval_ms", cli_show_hello_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO interval");
    cli_register_command(dessert_cli, dessert_cli_show, "tc_size", cli_show_tc_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show TC packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "tc_interval_ms", cli_show_tc_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show TC interval");
    cli_register_command(dessert_cli, dessert_cli_show, "ns", cli_show_ns, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show neighbor set table");
    cli_register_command(dessert_cli, dessert_cli_show, "ns_so", cli_show_ns_so, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show neighbor set table (simple output)");
    cli_register_command(dessert_cli, dessert_cli_show, "ls", cli_show_ls, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show link set table");
    cli_register_command(dessert_cli, dessert_cli_show, "2hns", cli_show_2hns, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show 2-hop neighbor set table");
    cli_register_command(dessert_cli, dessert_cli_show, "tc", cli_show_tc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show TC set table");
    cli_register_command(dessert_cli, dessert_cli_show, "rt", cli_show_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing table");
    cli_register_command(dessert_cli, dessert_cli_show, "rt_so", cli_show_rt_so, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing table (simple output)");
    cli_register_command(dessert_cli, dessert_cli_show, "metric", cli_show_rc_metric, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing metric");
}

static void _register_periodics() {
    /* registering periodic tasks */
    struct timeval hello_interval_tv;
    hello_interval_tv.tv_sec = hello_interval_ms / 1000;
    hello_interval_tv.tv_usec = (hello_interval_ms % 1000) * 1000;
    periodic_send_hello = dessert_periodic_add(olsr_periodic_send_hello, NULL, NULL, &hello_interval_tv);
    dessert_register_ptr_name(olsr_periodic_send_hello, "periodic_send_hello");

    struct timeval tc_interval_tv;
    tc_interval_tv.tv_sec = tc_interval_ms / 1000;
    tc_interval_tv.tv_usec = (tc_interval_ms % 1000) * 1000;
    periodic_send_tc = dessert_periodic_add(olsr_periodic_send_tc, NULL, NULL, &tc_interval_tv);
    dessert_register_ptr_name(olsr_periodic_send_tc, "olsr_periodic_send_tc");

    struct timeval build_rt_interval;
    build_rt_interval.tv_sec = rt_interval_ms / 1000;
    build_rt_interval.tv_usec = (rt_interval_ms % 1000) * 1000;
    periodic_rt = dessert_periodic_add(olsr_periodic_build_routingtable, NULL, NULL, &build_rt_interval);
    dessert_register_ptr_name(olsr_periodic_build_routingtable, "olsr_periodic_build_routingtable");
}

static void _register_pipeline() {
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
    dessert_meshrxcb_add(olsr_drop_errors, 30);
    dessert_meshrxcb_add(olsr_handle_hello, 40);
    dessert_meshrxcb_add(olsr_handle_tc, 45);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 75);
    dessert_meshrxcb_add(olsr_fwd2dest, 80);
    dessert_meshrxcb_add(rp2sys, 100);
    dessert_sysrxcb_add(olsr_sys2rp, 10);
}

int main(int argc, char** argv) {
    /* initialize daemon with correct parameters */
    FILE* cfg = NULL;

    if((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
        dessert_info("starting OLSR in non daemonize mode");
        dessert_init("OLSR", 0x02, DESSERT_OPT_NODAEMONIZE);
        char cfg_file_name[] = "/etc/des-olsr.conf";
        cfg = fopen(cfg_file_name, "r");

        if(cfg == NULL) {
            printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
            return EXIT_FAILURE;
        }
    }
    else {
        dessert_info("starting OLSR in daemonize mode");
        cfg = dessert_cli_get_cfg(argc, argv);
        dessert_init("OLSR", 0x02, DESSERT_OPT_DAEMONIZE);
    }

    /* routing table initialization */
    olsr_db_init();

    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

    _register_cli_callbacks();
    _register_pipeline();
    _register_periodics();

    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_cli_run();
    dessert_run();

    return EXIT_SUCCESS;
}
