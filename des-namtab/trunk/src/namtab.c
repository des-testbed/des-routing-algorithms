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

#include "cli/batman_cli.h"
#include "pipeline/batman_pipeline.h"
#include "database/batman_database.h"
#include "config.h"
#include <string.h>

#ifndef ANDROID
#include <printf.h>
#endif

int be_verbose = false;
int ogm_precursor_mode = USE_PRECURSOR_LIST;
char* routing_log_file = NULL;

int main(int argc, char** argv) {
    FILE* cfg = NULL;

    if((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
        dessert_info("starting NAMTAB (B.A.T.M.A.N Mod) in non daemonize mode");
        dessert_init("NAMTAB", 0x02, DESSERT_OPT_NODAEMONIZE);
        char cfg_file_name[] = "/etc/des-namtab.conf";
        cfg = fopen(cfg_file_name, "r");

        if(cfg == NULL) {
            printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
            return EXIT_FAILURE;
        }
    }
    else {
        cfg = dessert_cli_get_cfg(argc, argv);
        dessert_info("starting NAMTAB (B.A.T.M.A.N Mod) in daemonize mode");
        dessert_init("NAMTAB", 0x02, DESSERT_OPT_DAEMONIZE);
    }

    // routing table initialisation
    batman_db_init();

    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_STDERR);

    // cli initialization
    struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");
    cli_register_command(dessert_cli, cli_cfg_set, "ogmint", batman_cli_change_ogmint, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set OGM interval in sec");
    cli_register_command(dessert_cli, cli_cfg_set, "verbose", batman_cli_beverbose, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "be more verbose");
    cli_register_command(dessert_cli, cli_cfg_set, "ogmprecmode", batman_cli_ogmprecursormode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable OGM precursor mode [0=off,1=on]");
    cli_register_command(dessert_cli, cli_cfg_set, "routinglog", cli_setrouting_log, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set path to routing log file");
    struct cli_command* cli_command_print =
        cli_register_command(dessert_cli, NULL, "print", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print table");
    cli_register_command(dessert_cli, cli_command_print, "brt", batman_cli_print_brt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print backup routing table (classic B.A.T.M.A.N.)");
    cli_register_command(dessert_cli, cli_command_print, "irt", batman_cli_print_irt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print inverted routing table");
    cli_register_command(dessert_cli, cli_command_print, "rt", batman_cli_print_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "print routing table");

    dessert_meshrxcb_add(dessert_msg_check_cb, 10);
    dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
    dessert_meshrxcb_add(batman_drop_errors, 30);
    dessert_meshrxcb_add(batman_handle_ogm, 40);
    dessert_meshrxcb_add(dessert_mesh_ipttl, 45);
    dessert_meshrxcb_add(batman_fwd2dest, 50);
    dessert_meshrxcb_add(rp2sys, 100);

    dessert_sysrxcb_add(batman_sys2rp, 10);

    // register periodic tasks
    batman_periodic_register_send_ogm(ORIG_INTERVAL);

    struct timeval db_cleanup_interval;
    db_cleanup_interval.tv_sec = DB_CLEANUP_INTERVAL;
    db_cleanup_interval.tv_usec = 0;
    dessert_periodic_add(batman_periodic_cleanup_database, NULL, NULL, &db_cleanup_interval);

    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);

    dessert_cli_run();
    dessert_run();

    return EXIT_SUCCESS;
}
