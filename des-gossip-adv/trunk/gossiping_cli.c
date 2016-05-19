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

uint8_t activated = 0x00;

static int cli_sethellosize(struct cli_def *cli, char *command, char *argv[], int argc) {
    uint16_t min_size = sizeof(dessert_msg_t)+sizeof(struct ether_header)+2;
    if(argc != 1) {
        label_out_usage:
        cli_print(cli, "usage %s [%d..1500]\n", command, min_size);
        return CLI_ERROR;
    }
    uint16_t psize = (uint16_t) strtoul(argv[0], NULL, 10);
    if(psize < min_size
        || psize > 1500) {
        goto label_out_usage;
    }
    hello_size = psize;
    print_log(LOG_NOTICE, cli, "setting HELLO size to %d", hello_size);
    return CLI_OK;
}

static int cli_showhellosize(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "Hello size = %d bytes\n", hello_size);
    return CLI_OK;
}

static int cli_settimeout(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..32767]\n", command);
        return CLI_ERROR;
    }
    long int usec = strtoul(argv[0], NULL, 10) * 1000;
    long int sec = 0;
    while(usec >= 1000*1000) {
        sec += 1;
        usec -= 1000*1000;
    }
    timeout.tv_usec = usec;
    timeout.tv_sec = sec;
    print_log(LOG_NOTICE, cli, "setting timeout to %ld s + %ld us", sec, usec);
    return CLI_OK;
}

static int cli_showtimeout(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "timout = %ld s + %ld ms\n", timeout.tv_sec, timeout.tv_usec/1000);
    return CLI_OK;
}

static int cli_showtmax(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "T_MAX is set to %d ms\n", T_MAX_ms);
    return CLI_OK;
}

static int cli_settmax(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..32767]\n", command);
        return CLI_ERROR;
    }
    T_MAX_ms = strtoul(argv[0], NULL, 10);
    print_log(LOG_NOTICE, cli, "setting T_MAX to %d ms", T_MAX_ms);
    return CLI_OK;
}

static int cli_setk(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]\n", command);
        return CLI_ERROR;
    }
    k = (uint8_t) strtoul(argv[0], NULL, 10);
    print_log(LOG_NOTICE, cli, "setting k to %d", k);
    return CLI_OK;
}

static int cli_showk(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p=1.0 for k=%d hops\n", k);
    return CLI_OK;
}

static int cli_showforwarder(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(forwarder) {
        cli_print(cli, "true\n");
    }
    else {
        cli_print(cli, "false\n");
    }
    return CLI_OK;
}

static int cli_setforwarder(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0, 1]\n", command);
        return CLI_ERROR;
    }
    forwarder = 0;
    uint8_t i = (uint8_t) strtoul(argv[0], NULL, 10);
    if(i) {
        forwarder = 1;
    }
    cli_showforwarder(cli, command, argv, argc);
    return CLI_OK;
}

static int cli_setp2(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]\n", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 || d > 1) {
        cli_print(cli, "p2 must be in (0:1]\n");
        return CLI_ERROR;
    }
    p2 = d;
    print_log(LOG_NOTICE, cli, "setting p2 to %f", p2);
    return CLI_OK;
}

static int cli_showp2(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p2=%f instead of p=%f if less than n=%d neighbors\n", p2, p, n);
    return CLI_OK;
}

static int cli_setn(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
            cli_print(cli, "usage %s [0..255]\n", command);
            return CLI_ERROR;
    }
    n = (uint8_t) strtoul(argv[0], NULL, 10);
    print_log(LOG_NOTICE, cli, "setting n to %d", n);
    return CLI_OK;
}

static int cli_shown(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p2=%f instead of p=%f if less than n=%d neighbors\n", p2, p, n);
    return CLI_OK;
}

static int cli_setm(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
            cli_print(cli, "usage %s [0..255]\n", command);
            return CLI_ERROR;
    }
    m = (uint8_t) strtoul(argv[0], NULL, 10);
    print_log(LOG_NOTICE, cli, "setting m to %d", m);
    return CLI_OK;
}

static int cli_showm(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "forward packet if less than m=%d duplicates are received\n", m);
    return CLI_OK;
}

static int cli_setpmin(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]\n", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 || d > 1) {
        cli_print(cli, "p_min must be in (0:1]\n");
        return CLI_ERROR;
    }
    p_min = d;
    print_log(LOG_NOTICE, cli, "setting p_min to %f", p_min);
    return CLI_OK;
}

static int cli_showpmin(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p_min=%f \n", p_min);
    return CLI_OK;
}

static int cli_setpmax(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]\n", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 || d > 1) {
        cli_print(cli, "p_max must be in (0:1]\n");
        return CLI_ERROR;
    }
    p_max = d;
    print_log(LOG_NOTICE, cli, "setting p_max to %f", p_max);
    return CLI_OK;
}

static int cli_showpmax(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p_max=%f\n", p_max);
    return CLI_OK;
}

static void startAll(){
    if(activated & USE_HELLOS){
        start_neighborMon();
    }
    else if(activated & USE_NHDP){
        nhdp_start();
    }
#ifndef ANDROID
    else if(activated & USE_GOSSIP13) {
        gossip13_start();
    }
#endif
    else {
        dessert_notice("No neighborhood monitoring activated.");
    }
}

static void resetAndStopAll() {
    stop_neighborMon();
    resetPacketTrap();
    resetSeqLog();
    reset_hello_counter();
    nhdp_stop();
#ifndef ANDROID
    gossip13_stop();
#endif
}

static int cli_setgossip(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..%d]\n", command, gossip_unused-1);
        return CLI_ERROR;
    }

    activated = 0x00;
    resetAndStopAll();
    gossip = (uint8_t) strtoul(argv[0], NULL, 10);

    switch(gossip) {
        case gossip_0:
            activated = 0x00;
            break;
        case gossip_1:
            activated = USE_KHOPS;
            break;
        case gossip_2:
            activated = USE_KHOPS | USE_HELLOS;
            break;
        case gossip_3:
            activated = USE_KHOPS;
            break;
        case gossip_4:
            activated = USE_KHOPS;
            break;
        case gossip_5:
            activated = USE_KHOPS | USE_HELLOS;
            break;
        case gossip_6:
            activated = 0x00;
            break;
        case gossip_7:
            activated = 0x00;
            break;
        case gossip_8:
            activated = USE_HELLOS;
            break;
        case gossip_9:
            activated = USE_HELLOS;
            break;
        case gossip_11:
            activated = USE_NHDP;
            break;
        case gossip_12: // MCDS mode
            activated = 0x00;
            break;
#ifndef ANDROID
        case gossip_13:
            activated = USE_GOSSIP13;
            break;
#endif
         case gossip_14:
            activated = 0x00;
            break;
        default:
            activated |= 0x00;
            break;
    }
    startAll();
    print_log(LOG_NOTICE, cli, "setting gossip variant to %d", gossip);
    return CLI_OK;
}
/* Currently not used?!. TODO: use/remove
static int cli_stopneighbormon(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 0) {
        cli_print(cli, "usage %s \n", command);
        return CLI_ERROR;
    }
    dessert_notice("stop neighbor monitoring");
    stop_neighborMon();
    return CLI_OK;
}
*/

static int cli_showgossip(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "gossip mode is set to %d\n", gossip);
    return CLI_OK;
}

static int cli_setlogtx(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]\n", command);
        return CLI_ERROR;
    }
    if(strcmp("on", argv[0]) == 0) { // match
        logtx = 1;
        print_log(LOG_NOTICE, cli, "switching tx logging on");
    }
    else if(strcmp("off", argv[0]) == 0) {
        logtx = 0;
        print_log(LOG_NOTICE, cli, "switching tx logging off");
    }
    else {
        print_log(LOG_NOTICE, cli, "invalid argument: %s\n", argv[0]);
    }
    return CLI_OK;
}
/* Currently not used?! TODO: use/remove
static int cli_showlogtx(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(logtx == 0) {
        cli_print(cli, "tx logging is disabled\n");
    }
    else {
        cli_print(cli, "tx logging is enabled\n");
    }
    return CLI_OK;
}
*/

static int cli_setlogrx(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]\n", command);
        return CLI_ERROR;
    }
    if(strcmp("on", argv[0]) == 0) { // match
        logrx = 1;
        print_log(LOG_NOTICE, cli, "switching rx logging on");
    }
    else if(strcmp("off", argv[0]) == 0) {
        logrx = 0;
        print_log(LOG_NOTICE, cli, "switching rx logging off");
    }
    else {
        print_log(LOG_NOTICE, cli, "invalid argument: %s\n", argv[0]);
    }
    return CLI_OK;
}

static int cli_showlogrx(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(logrx == 0) {
        cli_print(cli, "rx logging is disabled\n");
    }
    else {
        cli_print(cli, "rx logging is enabled\n");
    }
    return CLI_OK;
}

/** CLI command - config mode - set threshold $probability */
static int cli_setp(struct cli_def *cli, char *command, char *argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]\n", command);
        return CLI_ERROR;
    }
    d = strtod(argv[0], NULL);
    if(d <= 0 || d > 1) {
        cli_print(cli, "p must be in (0:1]\n");
        return CLI_ERROR;
    }
    p = d;
    print_log(LOG_NOTICE, cli, "setting p to %f", p);
    return CLI_OK;
}

static int cli_showp(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "p is set to %f\n", p);
    return CLI_OK;
}

static int cli_setHelloInterval(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]\n", command);
        return CLI_ERROR;
    }
    uint32_t interval_ms = strtoul(argv[0], NULL, 10);

    hello_interval.tv_sec = 0;
    hello_interval.tv_usec = interval_ms*1000;
    while(hello_interval.tv_usec >= 1000*1000) {
        hello_interval.tv_sec++;
        hello_interval.tv_usec -= 1000*1000;
    }

    update_neighborMon();

    print_log(LOG_NOTICE, cli, "setting HELLO interval to %d ms", (int) (hello_interval.tv_sec*1000 + hello_interval.tv_usec/1000));
    return CLI_OK;
}

static int cli_showHelloInterval(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "HELLO interval: %d ms, cleanup interval %d s\n", (int) (hello_interval.tv_sec*1000 + hello_interval.tv_usec/1000), (int) cleanup_interval.tv_sec);
    return CLI_OK;
}

static int cli_setcleanup(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]\n", command);
        return CLI_ERROR;
    }

    cleanup_interval.tv_sec = (uint16_t) strtoul(argv[0], NULL, 10);
    cleanup_interval.tv_usec = 0;

    update_neighborMon();

    print_log(LOG_NOTICE, cli, "setting neighborhood cleanup interval to %d s", (uint16_t) cleanup_interval.tv_sec);
    return CLI_OK;
}

static int cli_showHelloTTL(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "HELLO TTL=%d\n", helloTTL);
    return CLI_OK;
}

static int cli_setHelloTTL(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]\n", command);
        return CLI_ERROR;
    }

    helloTTL = (uint8_t) strtoul(argv[0], NULL, 10);
    print_log(LOG_NOTICE, cli, "setting HELLO TTL to %d", helloTTL);
    return CLI_OK;
}

void init_cli() {
    cli_register_command(dessert_cli, dessert_cli_set, "probability", cli_setp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set gossip probability");
    cli_register_command(dessert_cli, dessert_cli_show, "probability", cli_showp, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show p");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip", cli_setgossip, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set gossip mode [0..11]");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip", cli_showgossip, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip mode");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize gossip interface");

    cli_register_command(dessert_cli, dessert_cli_set, "probability2", cli_setp2, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set p2");
    cli_register_command(dessert_cli, dessert_cli_show, "probability2", cli_showp2, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show p2");

    cli_register_command(dessert_cli, dessert_cli_set, "k", cli_setk, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flood on first k hops");
    cli_register_command(dessert_cli, dessert_cli_show, "k", cli_showk, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show k");

    cli_register_command(dessert_cli, dessert_cli_set, "forwarder", cli_setforwarder, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "forward packet or don't");
    cli_register_command(dessert_cli, dessert_cli_show, "forwarder", cli_showforwarder, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show if node forwards packets");

    cli_register_command(dessert_cli, dessert_cli_set, "n", cli_setn, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set node degree threshold for gossip2");
    cli_register_command(dessert_cli, dessert_cli_show, "n", cli_shown, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show n");

    cli_register_command(dessert_cli, dessert_cli_set, "m", cli_setm, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set copy number threshold");
    cli_register_command(dessert_cli, dessert_cli_show, "m", cli_showm, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show m");

    cli_register_command(dessert_cli, dessert_cli_set, "timeout_ms", cli_settimeout, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set timeout [ms]");
    cli_register_command(dessert_cli, dessert_cli_show, "timeout_ms", cli_showtimeout, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show timeout");

    cli_register_command(dessert_cli, dessert_cli_set, "helloSize", cli_sethellosize, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set size of HELLO packets [bytes]");
    cli_register_command(dessert_cli, dessert_cli_show, "helloSize", cli_showhellosize, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show timeout");
    cli_register_command(dessert_cli, dessert_cli_show, "neighbors", cli_showneighbors, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show neighbors");
    // hello functions
    cli_register_command(dessert_cli, dessert_cli_set, "helloInterval_ms", cli_setHelloInterval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set the interval for hello messages");
    cli_register_command(dessert_cli, dessert_cli_set, "cleanupInterval_s", cli_setcleanup, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set the cleanup interval for hello messages");
    cli_register_command(dessert_cli, dessert_cli_show, "helloInterval_ms", cli_showHelloInterval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO and cleanup interval");
    cli_register_command(dessert_cli, dessert_cli_set, "helloTTL", cli_setHelloTTL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set helloTTL");
    cli_register_command(dessert_cli, dessert_cli_show, "helloTTL", cli_showHelloTTL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show helloTTL");

    cli_register_command(dessert_cli, dessert_cli_show, "counter_hello", cli_showcounterhello, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show hello counter");

#ifndef ANDROID
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_stats", gossip13_stats, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 data");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_mystats", gossip13_mystats, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 data");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_tx_history", gossip13_show_tx_history, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 tx history");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_rx_history", gossip13_show_rx_history, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 rx history");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_data", gossip13_show_data, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 data for a host");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_hops_history", gossip13_show_hops_history, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show gossip13 hops history");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_tau_ms", gossip13_set_tau, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set restart interval");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_tau_ms", gossip13_show_tau, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show restart interval");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_restarts", gossip13_set_restarts, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set number of restarts");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_restarts", gossip13_show_restarts, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show number of restarts");

    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_deadline_ms", gossip13_set_deadline, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set deadline");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_deadline_ms", gossip13_show_deadline, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show deadline");

    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_update_ms", gossip13_set_update, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set update interval");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_update_ms", gossip13_show_update, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show update interval");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_observation_ms", gossip13_set_observation, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set update interval");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_observation_ms", gossip13_show_observation, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show update interval");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_emission_ms", gossip13_set_emission, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set emission interval");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_emission_ms", gossip13_show_emission, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show emission interval");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_drop_seq2_duplicates", gossip13_set_seq2, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "drop or deliver seq2 duplicates");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_drop_seq2_duplicates", gossip13_show_seq2, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show seq2 setting");
    cli_register_command(dessert_cli, dessert_cli_show, "gossip13_send_updates", gossip13_show_send_updates, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show if updates are sent");
    cli_register_command(dessert_cli, dessert_cli_set, "gossip13_send_updates", gossip13_set_send_updates, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "set if updates shall be sent [on, off]");
#endif

    cli_register_command(dessert_cli, dessert_cli_set, "tmax", cli_settmax, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set the T_MAX value for gossip7");
    cli_register_command(dessert_cli, dessert_cli_show, "tmax", cli_showtmax, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show tmax value");

    // min & max p for gossip8
    cli_register_command(dessert_cli, dessert_cli_set, "pmin", cli_setpmin, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set the p_min value for gossip8");
    cli_register_command(dessert_cli, dessert_cli_show, "pmin", cli_showpmin, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show p_min value");
    cli_register_command(dessert_cli, dessert_cli_set, "pmax", cli_setpmax, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set the p_max value for gossip8");
    cli_register_command(dessert_cli, dessert_cli_show, "pmax", cli_showpmax, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show p_max value");
    
    cli_register_command(dessert_cli, dessert_cli_set, "logrx", cli_setlogrx, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "switch rx logging on and off");
    cli_register_command(dessert_cli, dessert_cli_show, "logrx", cli_showlogrx, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rx logging state");
    cli_register_command(dessert_cli, dessert_cli_set, "logtx", cli_setlogtx, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "switch tx logging on and off");
    cli_register_command(dessert_cli, dessert_cli_show, "logtx", cli_showlogrx, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show tx logging state");

//     cli_register_command(dessert_cli, NULL, "start_helper", cli_start, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "start helper services");
//     cli_register_command(dessert_cli, NULL, "stop_helper", cli_stop, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "stop helper services");
//     cli_register_command(dessert_cli, NULL, "stop_neighbormon", cli_stopneighbormon, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG, "stop neighbor monitoring");
    dessert_info("Command line interface initialized");
}
