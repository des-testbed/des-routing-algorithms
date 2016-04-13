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

#include "config.h"
#include "cli/batman_cli.h"
#include "pipeline/batman_pipeline.h"
#include "database/batman_database.h"

uint16_t ogm_interval       = OGM_INTERVAL_MS;
uint16_t ogm_size           = OGM_SIZE;
uint8_t window_size         = WINDOW_SIZE;
bool ogm_precursor_mode     = USE_PRECURSOR_LIST;
bool resend_ogm_always      = RESEND_OGM_ALWAYS;

static void _register_cli() {
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

    cli_register_command(dessert_cli, dessert_cli_set, "ogm_size", cli_set_ogm_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set OGM packet size");
    cli_register_command(dessert_cli, dessert_cli_set, "ogm_interval", cli_set_ogm_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set OGM interval");
    cli_register_command(dessert_cli, dessert_cli_set, "window_size", cli_set_window_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set sliding window size [0..255]");
    cli_register_command(dessert_cli, dessert_cli_set, "resend_ogm_always", cli_set_ogm_resend_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "forward all OGMs [0=false,1=true]");
    cli_register_command(dessert_cli, dessert_cli_set, "ogm_precursor_mode", cli_set_ogm_precursor_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable OGM precursor mode [0=off, 1=on]");

    cli_register_command(dessert_cli, dessert_cli_show, "ogm_size", cli_show_ogm_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show OGM packet size");
    cli_register_command(dessert_cli, dessert_cli_show, "ogm_interval", cli_show_ogm_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show OGM interval");
    cli_register_command(dessert_cli, dessert_cli_show, "rt", cli_show_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show routing table");
}

static void _register_cb() {
    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
    dessert_meshrxcb_add(batman_drop_errors, 30);
    dessert_meshrxcb_add(batman_handle_ogm, 40);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 45);
    dessert_meshrxcb_add(batman_fwd2dest, 50);
    dessert_meshrxcb_add(rp2sys, 100);

    dessert_sysrxcb_add(batman_sys2rp, 10);
}

static void _register_tasks() {
    struct timeval db_cleanup_interval;
    db_cleanup_interval.tv_sec = DB_CLEANUP_INTERVAL / 1000;
    db_cleanup_interval.tv_usec = (DB_CLEANUP_INTERVAL % 1000) * 1000;
    dessert_periodic_add(batman_periodic_cleanup_database, NULL, NULL, &db_cleanup_interval);

    batman_periodic_register_send_ogm(ogm_interval);
}

static void _register_names() {
    dessert_register_ptr_name(batman_periodic_cleanup_database, "batman_periodic_cleanup_database");
    dessert_register_ptr_name(batman_periodic_send_ogm, "batman_periodic_send_ogm");
    dessert_register_ptr_name(batman_periodic_log_rt, "batman_periodic_log_rt");
}

int main(int argc, char** argv) {
    /* initialize daemon with correct parameters */
    FILE* cfg = NULL;

    if((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
        dessert_info("starting B.A.T.M.A.N in non daemonize mode");
        dessert_init("BATMAN", 0x04, DESSERT_OPT_NODAEMONIZE);
        char cfg_file_name[] = "/etc/des-batman.conf";
        cfg = fopen(cfg_file_name, "r");

        if(cfg == NULL) {
            printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
            return EXIT_FAILURE;
        }
    }
    else {
        dessert_info("starting B.A.T.M.A.N in daemonize mode");
        cfg = dessert_cli_get_cfg(argc, argv);
        dessert_init("BATMAN", 0x04, DESSERT_OPT_DAEMONIZE);
    }

    batman_db_init(); // routing table initialization
    dessert_logcfg(DESSERT_LOG_STDERR);
    _register_cli();
    _register_cb();
    _register_tasks();
    _register_names();
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    dessert_cli_run();
    dessert_run();

    return EXIT_SUCCESS;
}
