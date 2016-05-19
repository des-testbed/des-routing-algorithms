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

struct cli_def *cons;
struct cli_command *cli_cfg_set;
struct cli_command *cli_show_loopprotect;

char ara_forw_mode = ARA_FORW_B;


/* PARAMETER getters and setters */

int cli_set_trapretry_delay(struct cli_def *cli, char *command, char *argv[], int argc) {
    int i;
    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }
    i = (int) strtol(argv[0], NULL, 10);
    if(i <= 0 ) {
        cli_print(cli, "trapretry_delay must be > 0");
        return CLI_ERROR;
    }
    trapretry_delay = i;
    dessert_info("setting trapretry_delay to %d", trapretry_delay);
    return CLI_OK;    
} 

int cli_set_trapretry_max(struct cli_def *cli, char *command, char *argv[], int argc) {
    int i;
    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }
    i = (int) strtol(argv[0], NULL, 10);
    if(i <= 0 ) {
        cli_print(cli, "trapretry_max must be > 0");
        return CLI_ERROR;
    }
    trapretry_max = i;
    dessert_info("setting trapretry_max to %d", trapretry_max);
    return CLI_OK;
}

int cli_set_ara_forw_mode(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1 ) {
        cli_print(cli, "usage %s (BEST|WEIGHTED)", command);
        return CLI_ERROR;
    }
        
    switch(argv[0][0]) {
        case 'B':
            ara_forw_mode = ARA_FORW_B;
            cli_print(cli, "set ara_forw_mode to BEST");
            return CLI_OK;
        case 'W':
            ara_forw_mode = ARA_FORW_W;
            cli_print(cli, "set ara_forw_mode to WEIGHTED");
            return CLI_OK;
        default:
            cli_print(cli, "usage %s (B|W)", command);
            return CLI_ERROR;
    }
    /* this never happens */
    return CLI_ERROR;
}

int cli_set_rt_min_pheromone(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;
    if(argc != 1) {
        cli_print(cli, "usage %s [float]", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 ) {
        cli_print(cli, "rt_min_pheromone must be > 0");
        return CLI_ERROR;
    }
    rt_min_pheromone = d;
    dessert_info("setting threshold to %f", rt_min_pheromone);
    return CLI_OK;
}

int cli_set_rt_delta_q(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;
    if(argc != 1) {
        cli_print(cli, "usage %s [float]", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 || d >= 1) {
        cli_print(cli, "rt_delta_q must be in (0:1)");
        return CLI_ERROR;
    }
    rt_delta_q = d;
    dessert_info("setting rt_delta_q to %f", rt_delta_q);
    return CLI_OK;
}

int cli_set_rt_tick_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
    int i;
    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }
    i = (int) strtol(argv[0], NULL, 10);
    if(i <= 0 ) {
        cli_print(cli, "rt_tick_interval must be > 0");
        return CLI_ERROR;
    }
    rt_tick_interval = i;
    dessert_info("setting rt_tick_interval to %d", rt_tick_interval);
    return CLI_OK;
}

int cli_set_ara_trace_broadcastlen(struct cli_def *cli, char *command, char *argv[], int argc) {
    int i;
    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }
    i = (int) strtol(argv[0], NULL, 10);
    if(i < 0 ) {
        cli_print(cli, "ara_trace_broadcastlen must be >= 0");
        return CLI_ERROR;
    }
    ara_trace_broadcastlen = i;
    dessert_info("setting ara_trace_broadcastlen to %d", ara_trace_broadcastlen);
    return CLI_OK;
}

int main (int argc, char** argv)
{
    FILE *cfg;
        
    if(argc !=2 ) {
        cfg = fopen("/etc/des-ara.conf", "r");
        if(cfg == NULL) {
            dessert_err("usage %s configfile or %s if /etc/des-ara.conf ist present", argv[0], argv[0]);
            exit(1);
        }
    } else {
        cfg = fopen(argv[1], "r");
        if(cfg == NULL) {
            dessert_err("failed to open configfile %s", argv[1]);
            exit(2);
        }
    }
    
    
    /* initalize dessert framework */
    dessert_init("ARA", 0x02, DESSERT_OPT_DAEMONIZE|DESSERT_OPT_PID, "/var/run/des-ara.pid");
    
    
    /* initalize logging */
    dessert_logcfg(DESSERT_LOG_DEBUG|DESSERT_LOG_STDERR);
    
    
    /* build up cli */
    dessert_debug("initalizing cli");
    cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
    cli_register_command(dessert_cli, cli_cfg_set, "trapretry_delay", cli_set_trapretry_delay, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set trapretry_delay");
    cli_register_command(dessert_cli, cli_cfg_set, "trapretry_max", cli_set_trapretry_max, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set trapretry_max");
    cli_register_command(dessert_cli, cli_cfg_set, "ara_forw_mode", cli_set_ara_forw_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_forw_mode");
    cli_register_command(dessert_cli, cli_cfg_set, "rt_min_pheromone", cli_set_rt_min_pheromone, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_min_pheromone");
    cli_register_command(dessert_cli, cli_cfg_set, "rt_delta_q", cli_set_rt_delta_q, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_delta_q");
    cli_register_command(dessert_cli, cli_cfg_set, "rt_tick_interval", cli_set_rt_tick_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_tick_interval");
    cli_register_command(dessert_cli, cli_cfg_set, "ara_trace_broadcastlen", cli_set_ara_trace_broadcastlen, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_trace_broadcastlen");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "tap", cli_cfgtapif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initalize tap interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", cli_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initalize mesh interface");
    cli_register_command(dessert_cli, NULL, "packetdump", cli_packetdump, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "start packet dump");
    cli_register_command(dessert_cli, dessert_cli_cfg_no, "packetdump", cli_nopacketdump, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "stop packet dump");
    cli_register_command(dessert_cli, dessert_cli_show, "packettrap", cli_showpackettrap, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show packet trap");
    cli_register_command(dessert_cli, dessert_cli_show, "routing table", cli_showroutingtable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the routing table");
    cli_show_loopprotect = cli_register_command(dessert_cli, dessert_cli_show, "loopprotect", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect history");
    cli_register_command(dessert_cli, cli_show_loopprotect, "table", cli_showloopprotect_table, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect history");
    cli_register_command(dessert_cli, cli_show_loopprotect, "statistics", cli_showloopprotect_statistics, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect statistics");
    cli_register_command(dessert_cli, NULL, "ping", cli_ping, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "ping other host");
    cli_register_command(dessert_cli, NULL, "traceroute", cli_traceroute, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "traceroute other host");
    
    
    dessert_debug("initalizing done");
    
    /* initalize routing table */
    dessert_debug("initalizing routing table");
    ara_rt_init();
    dessert_debug("initalizing routing table done");
    
    /* initalize routing table */
    dessert_debug("initalizing loop prtection");
    ara_loopprotect_init();
    dessert_debug("initalizing loop prtection done");
    
    /* initalize callbacks */
    dessert_debug("initalizing callbacks");
    
        dessert_tunrxcb_add(ara_tun2ara, 100);
    
        /* check & tag */
        dessert_meshrxcb_add(dessert_msg_check_cb, 10);
        dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 15);
        dessert_meshrxcb_add(ara_checkl2dst, 20);
        dessert_meshrxcb_add(ara_makeproc, 20);
        dessert_meshrxcb_add(ara_checkloop, 30);

        /* handle ants and special packest and update routing table*/
        dessert_meshrxcb_add(ara_routeupdate_routefail, 40);
        dessert_meshrxcb_add(ara_handle_loops, 50);
        dessert_meshrxcb_add(ara_handle_fant, 60);
        dessert_meshrxcb_add(ara_routeupdate_ant, 60);
        dessert_meshrxcb_add(ara_routeupdate_rflow, 60);
        
        /* forward packet */
        dessert_meshrxcb_add(ara_dropdupe, 70);        
        dessert_meshrxcb_add(dessert_msg_trace_cb, 70);
        dessert_meshrxcb_add(ara_pingpong, 80);
        dessert_meshrxcb_add(ara_getroute, 80);
        dessert_meshrxcb_add(ara_routefail, 90);
        dessert_meshrxcb_add(ara_maintainroute_pant, 100);
        dessert_meshrxcb_add(ara_forward, 100);
        dessert_meshrxcb_add(ara_ara2tun, 100);
    
    dessert_debug("initalizing callbacks done");
    
    /* read config */
    dessert_debug("applying config");
    // we need no password - cli_allow_enable(dessert_cli, "gossip");
    cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
    
    dessert_debug("applying config done");
    
    /* add periodic task */
    
    /* mainloop.... */
    dessert_cli_run(2023);
    dessert_run();
    
    return (0);
    
}


