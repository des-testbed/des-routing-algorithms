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

#include <string.h>
#ifndef ANDROID
#include <printf.h>
#endif
#include <unistd.h>
#include <libgen.h>

#include "config.h"
#include "cli/aodv_cli.h"
#include "pipeline/aodv_pipeline.h"
#include "database/aodv_database.h"

uint16_t hello_size = HELLO_SIZE;
uint16_t hello_interval = HELLO_INTERVAL;
uint16_t rreq_size = RREQ_SIZE;
double gossip_p = GOSSIP_P;
bool dest_only = DEST_ONLY;
bool ring_search = RING_SEARCH;
/* gossip uses the ttl field specially, it should not be used together with ring_search. */
aodv_gossip_t gossip_type = GOSSIP_NONE;
aodv_metric_t metric_type = AODV_METRIC_RFC;
uint16_t metric_startvalue = AODV_METRIC_STARTVAL;
uint16_t rreq_interval = RREQ_INTERVAL;
int8_t signal_strength_threshold = AODV_SIGNAL_STRENGTH_THRESHOLD;
uint16_t tracking_factor = PDR_TRACKING_FACTOR;

dessert_periodic_t* send_hello_periodic;
dessert_periodic_t* send_rreq_periodic;

static void register_names() {
    dessert_register_ptr_name((void*)aodv_periodic_send_hello, "aodv_periodic_send_hello");
    dessert_register_ptr_name((void*)aodv_periodic_cleanup_database, "aodv_periodic_cleanup_database");
    dessert_register_ptr_name((void*)aodv_periodic_scexecute, "aodv_periodic_scexecute");
    dessert_register_ptr_name((void*)aodv_periodic_send_rreq, "aodv_periodic_send_rreq");
}

int main(int argc, char** argv) {
    uint16_t init_flags = DESSERT_OPT_DAEMONIZE;
    int used = 0;
    int size = 2;
    FILE **config_files = NULL;
    char **config_names = malloc(sizeof(char *) * size);
    char path[100];
    uint16_t logcfg_flags = 0;

    int c;
    while((c = getopt (argc, argv, "nc:")) != -1) {
        switch(c) {
            case 'n':
                init_flags = DESSERT_OPT_NODAEMONIZE;
                logcfg_flags |= DESSERT_LOG_STDERR;
                break;
            case 'c':
                if(used == size) {
                    config_names = realloc(config_names, sizeof(char *) * (size *= 2));
                }
                config_names[used++] = optarg;
                break;
            default:
                exit(EXIT_FAILURE);
                break;
        }
    }
    //no options given -- assume old style invocation
    if(optind == 1) {
        if(argc > optind) {
            config_names[used++] = argv[1];
        }
        else {
            snprintf(path, sizeof(path), "/etc/%s.conf", basename(argv[0]));
            config_names[used++] = path;
        }
    }
    else if(used == 0) {
        config_names[used++] = "./des-aodv.cli";
    }

    //open config files before (possibly) daemonizing
    config_files = malloc(sizeof(FILE *) * used);
    int i;
    for(i = 0; i < used; ++i) {
        config_files[i] = fopen(config_names[i], "r");
        if(!config_files[i]) {
            dessert_err("could not open config file %s\n", config_names[i]);
            exit(EXIT_FAILURE);
        }
    }
    free(config_names);

    if(init_flags & DESSERT_OPT_DAEMONIZE) {
        dessert_notice("starting AODV in daemonize mode");
    }
    else {
        dessert_notice("starting AODV in non daemonize mode");
    }
    dessert_init("AODV", 0x03, init_flags);

    /* routing table initialization */
    aodv_db_init();

    /* initalize logging */
    dessert_logcfg(logcfg_flags);

    /* cli initialization */
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

    cli_register_command(dessert_cli, dessert_cli_set, "hello_size", cli_set_hello_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "hello_size", cli_show_hello_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet size");

    cli_register_command(dessert_cli, dessert_cli_set, "hello_interval", cli_set_hello_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet interval");
    cli_register_command(dessert_cli, dessert_cli_show, "hello_interval", cli_show_hello_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet interval");

    cli_register_command(dessert_cli, dessert_cli_set, "rreq_size", cli_set_rreq_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set RREQ packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "rreq_size", cli_show_rreq_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show RREQ packet size");

    cli_register_command(dessert_cli, dessert_cli_set, "pdr_tracking_factor", cli_set_tracking_factor, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set factor for pdr tracking interval length");
    cli_register_command(dessert_cli, dessert_cli_show, "pdr_tracking_factor", cli_show_tracking_factor, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show factor for pdr tracking interval length");

    cli_register_command(dessert_cli, dessert_cli_set, "metric", cli_set_metric, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set metric");
    cli_register_command(dessert_cli, dessert_cli_show, "metric", cli_show_metric, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show metric");

    cli_register_command(dessert_cli, dessert_cli_set, "gossip", cli_set_gossip, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set gossip");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip", cli_show_gossip, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip");

    cli_register_command(dessert_cli, dessert_cli_set, "periodic_rreq_interval", cli_set_periodic_rreq_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set periodic rreq interval");
    cli_register_command(dessert_cli, dessert_cli_show, "periodic_rreq_interval", cli_show_periodic_rreq_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show periodic rreq interval");

    cli_register_command(dessert_cli, dessert_cli_set, "signal_strength_threshold", cli_set_preemptive_rreq_signal_strength_threshold, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set preemptive rreq signal strength threshold");
    cli_register_command(dessert_cli, dessert_cli_show, "signal_strength_threshold", cli_show_preemptive_rreq_signal_strength_threshold, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show preemptive rreq signal strength threshold");

    cli_register_command(dessert_cli, dessert_cli_set, "gossip_p", cli_set_gossip_p, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set p for gossip  p in [0.0,...,1.0]");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip_p", cli_show_gossip_p, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show p for gossip p in [0.0,...,1.0]");

    cli_register_command(dessert_cli, dessert_cli_set, "dest_only", cli_set_dest_only, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set destonly mode");
    cli_register_command(dessert_cli, dessert_cli_set, "ring_search", cli_set_ring_search, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ring_search  On/Off");

    cli_register_command(dessert_cli, dessert_cli_show, "rt", cli_show_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing table");
    cli_register_command(dessert_cli, dessert_cli_show, "pdr_nt", cli_show_pdr_nt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show pdr tracking table");

    cli_register_command(dessert_cli, dessert_cli_show, "neighbor_timeslot", cli_show_neighbor_timeslot, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show neighbor table timeslot");
    cli_register_command(dessert_cli, dessert_cli_show, "packet_buffer_timeslot", cli_show_packet_buffer_timeslot, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show packet buffer timeslot");
    cli_register_command(dessert_cli, dessert_cli_show, "data_seq_timeslot", cli_show_data_seq_timeslot, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show data seq timeslot");

    cli_register_command(dessert_cli, NULL, "send_rreq", cli_send_rreq, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "send RREQ to destination");

    /* registering callbacks */
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
    dessert_meshrxcb_add(aodv_drop_errors, 30);
    dessert_meshrxcb_add(aodv_handle_hello, 40);
    dessert_meshrxcb_add(aodv_handle_rreq, 50);
    dessert_meshrxcb_add(aodv_handle_rerr, 60);
    dessert_meshrxcb_add(aodv_handle_rrep, 70);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 75);
    dessert_meshrxcb_add(aodv_forward_broadcast, 80);
    dessert_meshrxcb_add(aodv_forward_multicast, 81);
    dessert_meshrxcb_add(aodv_forward, 90);
    dessert_meshrxcb_add(aodv_local_unicast, 100);

    dessert_sysrxcb_add(dessert_sys_drop_ipv6, 1);
    dessert_sysrxcb_add(aodv_sys_drop_multicast, 3);
    dessert_sysrxcb_add(aodv_sys2rp, 10);

    /* registering periodic tasks */
    struct timeval hello_interval_t;
    hello_interval_t.tv_sec = hello_interval / 1000;
    hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
    send_hello_periodic = dessert_periodic_add(aodv_periodic_send_hello, NULL, NULL, &hello_interval_t);
    dessert_notice("setting HELLO interval to %" PRIu16 " ms", hello_interval);

    struct timeval cleanup_interval;
    cleanup_interval.tv_sec = DB_CLEANUP_INTERVAL / 1000;
    cleanup_interval.tv_usec = (DB_CLEANUP_INTERVAL % 1000) * 1000;
    dessert_periodic_add(aodv_periodic_cleanup_database, NULL, NULL, &cleanup_interval);

    struct timeval schedule_check_interval;
    schedule_check_interval.tv_sec = SCHEDULE_CHECK_INTERVAL / 1000;
    schedule_check_interval.tv_usec = (SCHEDULE_CHECK_INTERVAL % 1000) * 1000;
    dessert_periodic_add(aodv_periodic_scexecute, NULL, NULL, &schedule_check_interval);

    /* running cli & daemon */
    for(i = 0; i < used; ++i) {
        cli_file(dessert_cli, config_files[i], PRIVILEGE_PRIVILEGED, MODE_CONFIG);
        fclose(config_files[i]);
    }
    free(config_files);
    register_names();
    dessert_cli_run();
    dessert_run();

    return EXIT_SUCCESS;
}
