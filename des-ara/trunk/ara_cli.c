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

struct cli_command* cli_cfg_flush;
struct cli_command* cli_show_loopprotect;

static const char* ara_forw_mode_strings[] = { "BEST", "WEIGHTED", "RANDOM", "ROUNDROBIN" };
static const char* ara_ptrail_mode_strings[] = { "CLASSIC", "CUBIC", "LINEAR" };
static const char* ara_ack_mode_strings[] = { "LINK", "PASSIVE", "NETWORK", "DISABLED" };

/*****************************************************************************/

int cli_show_ara_ack_credit_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "credit is increased by %04.02lf when an ack is received ", ara_ack_credit_inc);
    return CLI_OK;
}

int cli_set_ara_ack_credit_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    double i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..]", command);
        return CLI_ERROR;
    }

    i = strtod(argv[0], NULL);

    if(i < 0.0) {
        cli_print(cli, "parameter has to be >= 1.0");
        return CLI_ERROR;
    }

    ara_ack_credit_inc = i;

    dessert_notice("setting ara_ack_credit_inc to %04.02lf", ara_ack_credit_inc);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_ack_miss_credit(struct cli_def* cli, char* command, char* argv[], int argc) {
    double i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    ara_ack_miss_credit = i;

    dessert_notice("setting ara_ack_miss_credit to %04.02lf", ara_ack_miss_credit);
    return CLI_OK;
}

int cli_show_ara_ack_miss_credit(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "%04.02lf acknowledgements may be missed before a route is dropped in PASSIVE mode", ara_ack_miss_credit);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_prune_routes(struct cli_def* cli, char* command, char* argv[], int argc) {
    double i;

    if(argc != 1) {
        cli_print(cli, "usage %s [1..] (0 disables pruning)", command);
        return CLI_ERROR;
    }

    i = strtod(argv[0], NULL);

    if(i < 1.0) {
        cli_print(cli, "parameter has to be >= 1.0");
        return CLI_ERROR;
    }

    ara_prune_routes = i;

    dessert_notice("setting ara_prune_routes to %04.02lf", ara_prune_routes);
    return CLI_OK;
}

int cli_show_prune_routes(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(ara_prune_routes) {
        cli_print(cli, "altervative routes that are more than %04.02lf times longer than the shortest path are not accepted", ara_prune_routes);
    }
    else {
        cli_print(cli, "route pruning is disabled");
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_pant_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..255] (0 disables PANTs)", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    ara_pant_interval = i;

    dessert_notice("setting ara_pant_interval to %d", ara_pant_interval);
    return CLI_OK;
}

int cli_show_pant_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(ara_pant_interval) {
        cli_print(cli, "PANT interval is set to %d seconds", ara_pant_interval);
    }
    else {
        cli_print(cli, "PANTs are disabled");
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_print_rt_periodic(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..255] (0 disables printing)", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(ara_print_rt_interval_s == 0) {
        ara_print_rt_interval_s = i;
        dessert_periodic_add(ara_print_rt_periodic, NULL, NULL, NULL);
    }
    else {
        ara_print_rt_interval_s = i;
    }

    dessert_notice("setting ara_print_rt_interval_s to %d", ara_print_rt_interval_s);
    return CLI_OK;
}

int cli_show_ara_print_rt_periodic(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(ara_print_rt_interval_s) {
        cli_print(cli, "routing table is printed every %d seconds", ara_print_rt_interval_s);
    }
    else {
        cli_print(cli, "routing table is not printed periodically");
    }

    return CLI_OK;
}

/*****************************************************************************/
int cli_set_ara_print_cl_periodic(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..255] (0 disables printing)", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(ara_print_cl_interval_s == 0) {
        ara_print_cl_interval_s = i;
        dessert_periodic_add(ara_print_cl_periodic, NULL, NULL, NULL);
    }
    else {
        ara_print_cl_interval_s = i;
    }

    dessert_notice("setting ara_print_cl_interval_s to %d", ara_print_cl_interval_s);
    return CLI_OK;
}

int cli_show_ara_print_cl_periodic(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(ara_print_cl_interval_s) {
        cli_print(cli, "path classification table is printed every %d seconds", ara_print_cl_interval_s);
    }
    else {
        cli_print(cli, "path classification table is not printed periodically");
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_ack_wait_ms(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..2^16-1]", command);
        return CLI_ERROR;
    }

    i = (uint16_t) strtol(argv[0], NULL, 10);

    ara_ack_wait_ms = i;

    dessert_notice("setting ara_ack_wait_ms to %d", ara_ack_wait_ms);
    return CLI_OK;
}

int cli_show_ara_ack_wait_ms(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "acknowledgements are expected to arrive in %d ms", ara_ack_wait_ms);
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_ndisjoint(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "node disjoint mode is %s", (ara_ndisjoint == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

int cli_set_ndisjoint(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]", command);
        return CLI_ERROR;
    }

    if(!strcmp("on", argv[0])) {
        ara_ndisjoint = 1;
    }
    else if(!strcmp("off", argv[0])) {
        ara_ndisjoint = 0;
    }
    else {
        cli_print(cli, "invalid parameter: %s", argv[0]);
        dessert_err("invalid parameter: %s", argv[0]);
        return CLI_ERROR;
    }

    dessert_notice("node disjoint mode is %s", (ara_ndisjoint == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_ara_adap_evaporation(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "adaptive evaporation is %s", (ara_adap_evaporation == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

int cli_set_ara_adap_evaporation(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]", command);
        return CLI_ERROR;
    }

    if(!strcmp("on", argv[0])) {
        ara_adap_evaporation = 1;
    }
    else if(!strcmp("off", argv[0])) {
        ara_adap_evaporation = 0;
    }
    else {
        cli_print(cli, "invalid parameter: %s", argv[0]);
        dessert_err("invalid parameter: %s", argv[0]);
        return CLI_ERROR;
    }

    dessert_notice("adaptive evaporation is %s", (ara_adap_evaporation == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_ara_backwards_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "backwards pheromone trail increase is %s", (ara_backwards_inc == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

int cli_set_ara_backwards_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]", command);
        return CLI_ERROR;
    }

    if(!strcmp("on", argv[0])) {
        ara_backwards_inc = 1;
    }
    else if(!strcmp("off", argv[0])) {
        ara_backwards_inc = 0;
    }
    else {
        cli_print(cli, "invalid parameter: %s", argv[0]);
        dessert_err("invalid parameter: %s", argv[0]);
        return CLI_ERROR;
    }

    dessert_notice("backwards pheromone trail increase is %s", (ara_backwards_inc == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_rtprob_bants(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "route problem BANTs are %s", (ara_rtprob_bants == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

int cli_set_rtprob_bants(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]", command);
        return CLI_ERROR;
    }

    if(!strcmp("on", argv[0])) {
        ara_rtprob_bants = 1;
    }
    else if(!strcmp("off", argv[0])) {
        ara_rtprob_bants = 0;
    }
    else {
        cli_print(cli, "invalid parameter: %s", argv[0]);
        dessert_err("invalid parameter: %s", argv[0]);
        return CLI_ERROR;
    }

    ;

    dessert_notice("route problem BANTs have been %s", (ara_rtprob_bants == 0) ? "disabled" : "enabled");

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_retry_delay_ms(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..2^16-1] (timeout in ms)", command);
        return CLI_ERROR;
    }

    i = (uint16_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_retry_delay_ms must be > 0");
        return CLI_ERROR;
    }

    ara_retry_delay_ms = i;
    dessert_notice("setting ara_retry_delay_ms to %d", ara_retry_delay_ms);
    return CLI_OK;
}

int cli_show_ara_retry_delay_ms(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_retry_delay_ms = %d", ara_retry_delay_ms);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_retry_max(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_retry_max must be > 0");
        return CLI_ERROR;
    }

    ara_retry_max = i;
    dessert_notice("setting ara_retry_max to %d", ara_retry_max);
    return CLI_OK;
}

int cli_show_ara_retry_max(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_retry_max = %d", ara_retry_max);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_clsf_lossmin(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..100]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_lossmin must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_lossmin = i;
    dessert_notice("setting ara_clsf_lossmin to %d", ara_clsf_lossmin);
    return CLI_OK;
}

int cli_show_ara_clsf_lossmin(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_lossmin = %d", ara_clsf_lossmin);
    return CLI_OK;
}

/*****************************************************************************/
int cli_set_ara_clsf_lossmax(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [0..100]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_lossmax must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_lossmax = i;
    dessert_notice("setting ara_clsf_lossmax to %d", ara_clsf_lossmax);
    return CLI_OK;
}

int cli_show_ara_clsf_lossmax(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_lossmax = %d", ara_clsf_lossmax);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_clsf_tick_interval_s(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_tick_interval_s must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_tick_interval_s = i;
    dessert_notice("setting ara_clsf_tick_interval_s to %d", ara_clsf_tick_interval_s);
    return CLI_OK;
}

int cli_show_ara_clsf_tick_interval_s(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_tick_interval_s = %d", ara_clsf_tick_interval_s);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_clsf_skiptimes(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_skiptimes must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_skiptimes = i;
    dessert_notice("setting ara_clsf_skiptimes to %d", ara_clsf_skiptimes);
    return CLI_OK;
}

int cli_show_ara_clsf_skiptimes(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_skiptimes = %d", ara_clsf_skiptimes);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_clsf_sw_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_sw_size must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_sw_size = i;
    dessert_notice("setting ara_clsf_sw_size to %d", ara_clsf_sw_size);
    return CLI_OK;
}

int cli_show_ara_clsf_sw_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_sw_size = %d", ara_clsf_sw_size);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_clsf_sender_rate(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (uint16_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "ara_clsf_sender_rate must be > 0");
        return CLI_ERROR;
    }

    ara_clsf_sender_rate = i;
    dessert_notice("setting ara_clsf_sender_rate to %d", ara_clsf_sender_rate);
    return CLI_OK;
}

int cli_show_ara_clsf_sender_rate(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_clsf_sender_rate = %d", ara_clsf_sender_rate);
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_ara_classify(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "path classification is %s", (ara_classify == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

int cli_set_ara_classify(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [on, off]", command);
        return CLI_ERROR;
    }

    if(!strcmp("on", argv[0])) {
        ara_classify = 1;
    }
    else if(!strcmp("off", argv[0])) {
        ara_classify = 0;
    }
    else {
        cli_print(cli, "invalid parameter: %s", argv[0]);
        dessert_err("invalid parameter: %s", argv[0]);
        return CLI_ERROR;
    }

    dessert_notice("path classification mode is %s", (ara_classify == 0) ? "disabled" : "enabled");
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_ack_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [LINK, PASSIVE, NETWORK, DISABLED]", command);
        return CLI_ERROR;
    }

    switch(argv[0][0]) {
        case 'L':
            // TODO implement mode
            dessert_err("mode not yet implemented");
            break;
            ara_ack_mode = ARA_ACK_LINK;
            break;
        case 'P':
            ara_ack_mode = ARA_ACK_PASSIVE;
            break;
        case 'N':
            ara_ack_mode = ARA_ACK_NETWORK;
            break;
        case 'D':
            ara_ack_mode = ARA_ACK_DISABLED;
            break;
        default:
            cli_print(cli, "invalid acknowledgement mode: %s", argv[0]);
            dessert_err("invalid acknowledgement mode: %s", argv[0]);
            return CLI_ERROR;
    }

    dessert_notice("ara_ack_mode set to %s", ara_ack_mode_strings[ara_ack_mode]);
    return CLI_OK;
}

int cli_show_ara_ack_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    switch(ara_ack_mode) {
        case ARA_ACK_LINK:
        case ARA_ACK_PASSIVE:
        case ARA_ACK_NETWORK:
        case ARA_ACK_DISABLED:
            cli_print(cli, "acknowledgement mode is set to %s", ara_ack_mode_strings[ara_ack_mode]);
            break;
        default:
            cli_print(cli, "an invalid acknowledgement mode is set: %d", ara_forw_mode);
            break;
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_forw_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [BEST, WEIGHTED, RANDOM]", command);
        return CLI_ERROR;
    }

    switch(argv[0][0]) {
        case 'B':
            ara_forw_mode = ARA_FORW_B;
            break;
        case 'W':
            ara_forw_mode = ARA_FORW_W;
            break;
        case 'R':
            ara_forw_mode = ARA_FORW_R;
            break;
        default:
            cli_print(cli, "invalid forward mode: %s", argv[0]);
            dessert_err("invalid forward mode: %s", argv[0]);
            return CLI_ERROR;
    }

    dessert_notice("ara_forw_mode set to %s", ara_forw_mode_strings[ara_forw_mode]);
    return CLI_OK;
}

int cli_show_ara_forw_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    switch(ara_forw_mode) {
        case ARA_FORW_B:
            cli_print(cli, "forward mode is set to %s", ara_forw_mode_strings[ara_forw_mode]);
            break;
        case ARA_FORW_W:
            cli_print(cli, "forward mode is set to %s", ara_forw_mode_strings[ara_forw_mode]);
            break;
        case ARA_FORW_R:
            cli_print(cli, "forward mode is set to %s", ara_forw_mode_strings[ara_forw_mode]);
            break;
        default:
            cli_print(cli, "an invalid forward mode is set: %d", ara_forw_mode);
            break;
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_ptrail_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [CLASSIC, CUBIC]", command);
        return CLI_ERROR;
    }

    if(!strcmp("CLASSIC", argv[0])) {
        ara_ptrail_mode = ARA_PTRAIL_CLASSIC;
    }
    else if(!strcmp("CUBIC", argv[0])) {
        ara_ptrail_mode = ARA_PTRAIL_CUBIC;
    }
    else if(!strcmp("LINEAR", argv[0])) {
        ara_ptrail_mode = ARA_PTRAIL_LINEAR;
    }
    else {
        cli_print(cli, "invalid pheromone trail mode: %s", argv[0]);
        dessert_err("invalid pheromone trail mode: %s", argv[0]);
        return CLI_ERROR;
    }

    dessert_notice("pheromone trail mode set to %s", ara_ptrail_mode_strings[ara_ptrail_mode]);
    return CLI_OK;
}

int cli_show_ara_ptrail_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    switch(ara_ptrail_mode) {
        case ARA_PTRAIL_CLASSIC:
        case ARA_PTRAIL_CUBIC:
        case ARA_PTRAIL_LINEAR:
            cli_print(cli, "pheromone trail mode set to %s", ara_ptrail_mode_strings[ara_ptrail_mode]);
            break;
        default:
            cli_print(cli, "an invalid pheromone trail mode is set: %d", ara_ptrail_mode);
            break;
    }

    return CLI_OK;
}

/*****************************************************************************/

int cli_set_rt_min_pheromone(struct cli_def* cli, char* command, char* argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]", command);
        return CLI_ERROR;
    }

    d = strtod(argv[0], NULL);

    if(d <= 0) {
        cli_print(cli, "rt_min_pheromone must be > 0");
        return CLI_ERROR;
    }

    rt_min_pheromone = d;
    dessert_notice("setting threshold to %f", rt_min_pheromone);
    return CLI_OK;
}

int cli_show_rt_min_pheromone(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "rt_min_pheromone = %f", rt_min_pheromone);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_rt_delta_q(struct cli_def* cli, char* command, char* argv[], int argc) {
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
    dessert_notice("setting rt_delta_q to %f", rt_delta_q);
    return CLI_OK;
}

int cli_show_rt_delta_q(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "rt_delta_q = %f", rt_delta_q);
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_rt_initial(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "rt_initial = %f", rt_initial);
    return CLI_OK;
}

int cli_set_rt_initial(struct cli_def* cli, char* command, char* argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]", command);
        return CLI_ERROR;
    }

    d = strtod(argv[0], NULL);

    if(d <= 0 || d >= 1) {
        cli_print(cli, "rt_initial must be in (0:1)");
        return CLI_ERROR;
    }

    rt_initial = d;
    dessert_notice("setting rt_initial to %f", rt_initial);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_rt_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    double d;

    if(argc != 1) {
        cli_print(cli, "usage %s [float]", command);
        return CLI_ERROR;
    }

    d = strtod(argv[0], NULL);

    if(d <= 0 || d >= 1) {
        cli_print(cli, "rt_inc must be in (0:1)");
        return CLI_ERROR;
    }

    rt_inc = d;
    dessert_notice("setting rt_inc to %f", rt_inc);
    return CLI_OK;
}

int cli_show_rt_inc(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "rt_inc = %f", rt_inc);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_rt_tick_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint8_t i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (uint8_t) strtol(argv[0], NULL, 10);

    if(i <= 0) {
        cli_print(cli, "rt_tick_interval must be > 0");
        return CLI_ERROR;
    }

    rt_tick_interval = i;
    dessert_notice("setting rt_tick_interval to %d", rt_tick_interval);
    return CLI_OK;
}

int cli_show_rt_tick_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "rt_tick_interval = %d", rt_tick_interval);
    return CLI_OK;
}

/*****************************************************************************/

int cli_set_ara_trace_broadcastlen(struct cli_def* cli, char* command, char* argv[], int argc) {
    int i;

    if(argc != 1) {
        cli_print(cli, "usage %s [int]", command);
        return CLI_ERROR;
    }

    i = (int) strtol(argv[0], NULL, 10);

    if(i < 0) {
        cli_print(cli, "ara_trace_broadcastlen must be >= 0");
        return CLI_ERROR;
    }

    ara_trace_broadcastlen = i;
    dessert_notice("setting ara_trace_broadcastlen to %d", ara_trace_broadcastlen);
    return CLI_OK;
}

int cli_show_ara_trace_broadcastlen(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "ara_trace_broadcastlen = %d", (uint16_t) ara_trace_broadcastlen);
    return CLI_OK;
}

/*****************************************************************************/
// Weng Yu add ara_ant_size
int cli_set_ara_ant_size(struct cli_def *cli, char *command, char *argv[], int argc) {
    size_t i;
    if(argc != 1) {
        cli_print(cli, "usage %s [0..2^16-1]", command);
        return CLI_ERROR;
    }
    i = (size_t) strtol(argv[0], NULL, 10);

    ara_ant_size = i;

    dessert_notice("setting ara_ant_size to %d", ara_ant_size);
    return CLI_OK;
}

int cli_show_ara_ant_size(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "ANT Size is set to %d", ara_ant_size);
    return CLI_OK;
}

/*****************************************************************************/

int cli_show_ara_config(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "ara_prune_routes = %04.02lf", ara_prune_routes);
    cli_print(cli, "ara_pant_interval = %d", ara_pant_interval);
    cli_print(cli, "ara_ant_size = %d", ara_ant_size);
    cli_print(cli, "ara_rtprob_bants = %s", (ara_rtprob_bants==0)?"off":"on");
    cli_print(cli, "ara_ndisjoint = %s", (ara_ndisjoint==0)?"off":"on");
    cli_print(cli, "ara_retry_delay_ms = %d", ara_retry_delay_ms);
    cli_print(cli, "ara_retry_max = %d", ara_retry_max);
    cli_print(cli, "ara_forw_mode = %s", ara_forw_mode_strings[ara_forw_mode]);
    cli_print(cli, "ara_ptrail_mode = %s", ara_ptrail_mode_strings[ara_ptrail_mode]);
    cli_print(cli, "ara_rt_min_pheromone = %f", rt_min_pheromone);
    cli_print(cli, "ara_rt_initial = %f", rt_initial);
    cli_print(cli, "ara_rt_delta_q = %f", rt_delta_q);
    cli_print(cli, "ara_rt_inc = %f", rt_inc);
    cli_print(cli, "ara_rt_tick_interval = %d", rt_tick_interval);
    cli_print(cli, "ara_trace_broadcastlen = %d", (uint16_t) ara_trace_broadcastlen);
    cli_print(cli, "ara_ack_mode = %s", ara_ack_mode_strings[ara_ack_mode]);
    cli_print(cli, "ara_ack_wait_ms = %d", ara_ack_wait_ms);
    cli_print(cli, "ara_backwards_inc = %s", (ara_backwards_inc == 0) ? "disabled" : "enabled");
    cli_print(cli, "ara_ack_miss_credit = %04.02lf", ara_ack_miss_credit);
    cli_print(cli, "ara_ack_credit_inc = %04.02lf", ara_ack_credit_inc);
    cli_print(cli, "ara_adap_evaporation = %s", (ara_adap_evaporation == 0) ? "disabled" : "enabled");
    cli_print(cli, "ara_clsf_lossmin = %d", ara_clsf_lossmin);
    cli_print(cli, "ara_clsf_lossmax = %d", ara_clsf_lossmax);
    cli_print(cli, "ara_clsf_tick_interval_s = %d", ara_clsf_tick_interval_s);
    cli_print(cli, "ara_clsf_skiptimes = %d", ara_clsf_skiptimes);
    cli_print(cli, "ara_clsf_sw_size = %d", ara_clsf_sw_size);
    cli_print(cli, "ara_clsf_sender_rate = %d", ara_clsf_sender_rate);
    cli_print(cli, "ara_classify = %s", (ara_classify==0)?"off":"on");
    return CLI_OK;
}

int cli_flush_all(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_flushroutingtable(cli, command, argv, argc);
    cli_flushclassifictable(cli, command, argv, argc);
    cli_flush_loopprotec_table(cli, command, argv, argc);
    cli_flush_ack_monitor(cli, command, argv, argc);
    cli_flush_rmnt(cli, command, argv, argc);

    return CLI_OK;
}

void ara_init_cli() {
    dessert_debug("initalizing CLI");

    cli_register_command(dessert_cli, dessert_cli_show, "ara_config", cli_show_ara_config, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ARA configuration");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ack_mode", cli_set_ara_ack_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set acknowledgement mode");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ack_mode", cli_show_ara_ack_mode, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show acknowledgement mode");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ack_miss_credit", cli_set_ara_ack_miss_credit, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set how many ACK may be missed (credit)");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ack_miss_credit", cli_show_ara_ack_miss_credit, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show how many ACK may be missed (credit)");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ack_credit_inc", cli_set_ara_ack_credit_inc, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set value to increase credit on rx ACK");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ack_credit_inc", cli_show_ara_ack_credit_inc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_ack_credit_inc");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_prune_routes", cli_set_prune_routes, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "prune alternative routes");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_prune_routes", cli_show_prune_routes, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show route pruning length");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_pant_interval", cli_set_pant_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set PANT interval");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_pant_interval", cli_show_pant_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show PANT interval");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ack_wait_ms", cli_set_ara_ack_wait_ms, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set timeout for acknowledgements");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ack_wait_ms", cli_show_ara_ack_wait_ms, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show acknowledgement timeout");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ndisjoint", cli_set_ndisjoint, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable/disable node disjoint route discovery");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ndisjoint", cli_show_ndisjoint, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show node disjoint setting");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_adap_evaporation", cli_set_ara_adap_evaporation, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable/disable adaptive evaporation");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_adap_evaporation", cli_show_ara_adap_evaporation, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show adaptive evaporation setting");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_backwards_inc", cli_set_ara_backwards_inc, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable/disable node backwards pheromone increase");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_backwards_inc", cli_show_ara_backwards_inc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_backwards_inc setting");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rtprob_bants", cli_set_rtprob_bants, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable/disable route problem BANTs");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rtprob_bants", cli_show_rtprob_bants, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show route problem BANT mode");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_retry_delay_ms", cli_set_ara_retry_delay_ms, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set retry timeout [ms]");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_retry_delay_ms", cli_show_ara_retry_delay_ms, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show retry timout");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_retry_max", cli_set_ara_retry_max, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set maximum number of retries");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_retry_max", cli_show_ara_retry_max, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show maximum number of retries");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_forw_mode", cli_set_ara_forw_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set forward mode");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_forw_mode", cli_show_ara_forw_mode, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show forward mode");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_ptrail_mode", cli_set_ara_ptrail_mode, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set pheromone trail mode");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ptrail_mode", cli_show_ara_ptrail_mode, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show pheromone trail mode");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rt_min_pheromone", cli_set_rt_min_pheromone, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_min_pheromone");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rt_min_pheromone", cli_show_rt_min_pheromone, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rt_min_pheromone");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rt_initial", cli_set_rt_initial, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_initial");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rt_initial", cli_show_rt_initial, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rt_initial");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rt_delta_q", cli_set_rt_delta_q, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_delta_q");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rt_delta_q", cli_show_rt_delta_q, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rt_delta_q");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rt_inc", cli_set_rt_inc, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_inc");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rt_inc", cli_show_rt_inc, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rt_inc");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_rt_tick_interval", cli_set_rt_tick_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set rt_tick_interval");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_rt_tick_interval", cli_show_rt_tick_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show rt_tick_interval");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_trace_broadcastlen", cli_set_ara_trace_broadcastlen, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_trace_broadcastlen");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_trace_broadcastlen", cli_show_ara_trace_broadcastlen, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_trace_broadcastlen");

    cli_register_command(dessert_cli, dessert_cli_set, "ara_print_rt_interval_s", cli_set_ara_print_rt_periodic, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_print_rt_interval_s");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_print_rt_interval_s", cli_show_ara_print_rt_periodic, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_print_rt_interval_s setting");


    cli_register_command(dessert_cli, dessert_cli_set, "ara_print_cl_interval_s", cli_set_ara_print_cl_periodic, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_print_cl_interval_s");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_print_cl_interval_s", cli_show_ara_print_cl_periodic, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_print_cl_interval_s setting");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_lossmin", cli_set_ara_clsf_lossmin, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set lower classification bound");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_lossmin", cli_show_ara_clsf_lossmin, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show lower classification bound");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_lossmax", cli_set_ara_clsf_lossmax, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set upper classification bound");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_lossmax", cli_show_ara_clsf_lossmax, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show upper classification bound");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_tick_interval_s", cli_set_ara_clsf_tick_interval_s, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_clsf_tick_interval_s");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_tick_interval_s", cli_show_ara_clsf_tick_interval_s, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_clsf_tick_interval_s");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_skiptimes", cli_set_ara_clsf_skiptimes, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_clsf_skiptimes");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_skiptimes", cli_show_ara_clsf_skiptimes, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_clsf_skiptimes");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_sw_size", cli_set_ara_clsf_sw_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_clsf_sw_size");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_sw_size", cli_show_ara_clsf_sw_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_clsf_sw_size");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_clsf_sender_rate", cli_set_ara_clsf_sender_rate, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_clsf_sender_rate");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_clsf_sender_rate", cli_show_ara_clsf_sender_rate, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_clsf_sender_rate");
    cli_register_command(dessert_cli, dessert_cli_set, "ara_classify", cli_set_ara_classify, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "enable/disable path classification");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_classify", cli_show_ara_classify, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show path classification setting");


    cli_register_command(dessert_cli, dessert_cli_set, "ara_ant_size", cli_set_ara_ant_size, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set ara_ant_size");
    cli_register_command(dessert_cli, dessert_cli_show, "ara_ant_size", cli_show_ara_ant_size, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show ara_ant_size setting");

    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize tap interface");
    cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

    cli_register_command(dessert_cli, dessert_cli_show, "packettrap", cli_showpackettrap, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show packet trap");

    cli_register_command(dessert_cli, dessert_cli_show, "routing table", cli_showroutingtable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the routing table");

    cli_register_command(dessert_cli, dessert_cli_show, "path classification table", cli_showclassifictable, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the classification table");

    cli_register_command(dessert_cli, dessert_cli_show, "ack monitor", cli_show_ack_monitor, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show packets waiting for acknowledgement");

    cli_show_loopprotect = cli_register_command(dessert_cli, dessert_cli_show, "loopprotect", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect history");

    cli_register_command(dessert_cli, cli_show_loopprotect, "table", cli_showloopprotect_table, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect history");

    cli_register_command(dessert_cli, cli_show_loopprotect, "statistics", cli_showloopprotect_statistics, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show the loopprotect statistics");

    cli_cfg_flush = cli_register_command(dessert_cli, NULL, "flush", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush data structure");

    cli_register_command(dessert_cli, cli_cfg_flush, "routing table", cli_flushroutingtable, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush routing table");

    cli_register_command(dessert_cli, cli_cfg_flush, "path classification table", cli_flushclassifictable, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush classification table");

    cli_register_command(dessert_cli, cli_cfg_flush, "ack monitor", cli_flush_ack_monitor, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush ack monitor");

    cli_register_command(dessert_cli, cli_cfg_flush, "loop protection", cli_flush_loopprotec_table, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush loop protection table");

    cli_register_command(dessert_cli, cli_cfg_flush, "rmnt", cli_flush_rmnt, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush route management table");

    cli_register_command(dessert_cli, cli_cfg_flush, "all", cli_flush_all, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "flush all data structures");

    dessert_debug("CLI initialized");
}
