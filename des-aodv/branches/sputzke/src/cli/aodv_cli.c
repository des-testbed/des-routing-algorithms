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
#include <string.h>
#include <time.h>

#include "../config.h"
#include "aodv_cli.h"
#include "../database/aodv_database.h"
#include "../pipeline/aodv_pipeline.h"

// -------------------- Testing ------------------------------------------------------------

int cli_set_dest_only(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t mode;

    if(argc != 1 || sscanf(argv[0], "%" PRIu32 "", &mode) != 1 || (mode != 0 && mode != 1)) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(mode == 1) {
        dessert_notice("use dest_only = true");
        dest_only = true;
    }
    else {
        dessert_notice("use dest_only = false");
        dest_only = false;
    }
    return CLI_OK;
}

int cli_set_ring_search(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t mode;

    if(argc != 1 || sscanf(argv[0], "%" PRIu32 "", &mode) != 1 || (mode != 0 && mode != 1)) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(mode == 1) {
        dessert_notice("use ring_search = true");
        ring_search = true;
    }
    else {
        dessert_notice("use ring_search = false");
        ring_search = false;
    }
    return CLI_OK;
}


int cli_set_hello_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t min_size = sizeof(dessert_msg_t) + sizeof(struct ether_header) + 2;

    if(argc != 1) {
    label_out_usage:
        cli_print(cli, "usage %s [%" PRIu16 "..1500]\n", command, min_size);
        return CLI_ERROR;
    }

    uint16_t psize = (uint16_t) strtoul(argv[0], NULL, 10);

    if(psize < min_size || psize > 1500) {
        goto label_out_usage;
    }

    hello_size = psize;
    dessert_notice("setting HELLO size to %" PRIu16 "", hello_size);
    return CLI_OK;
}

int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [interval]\n", command);
        return CLI_ERROR;
    }

    hello_interval = (uint16_t) strtoul(argv[0], NULL, 10);

    uint32_t count = 0;
    aodv_db_neighbor_reset(&count);

    dessert_periodic_del(send_hello_periodic);
    send_hello_periodic = NULL;

    struct timeval hello_interval_t;
    hello_interval_t.tv_sec = hello_interval / 1000;
    hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
    send_hello_periodic = dessert_periodic_add(aodv_periodic_send_hello, NULL, NULL, &hello_interval_t);

    aodv_db_pdr_upd_expected(hello_interval);

    dessert_notice("setting HELLO interval to %" PRIu16 " ms - %" PRIu32 " neighbors invalidated...", hello_interval, count);
    return CLI_OK;
}

int cli_set_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t min_size = sizeof(dessert_msg_t) + sizeof(struct ether_header) + 2;

    if(argc != 1) {
    label_out_usage:
        cli_print(cli, "usage %s [%" PRIu16 "..1500]\n", command, min_size);
        return CLI_ERROR;
    }

    uint16_t psize = (uint16_t) strtoul(argv[0], NULL, 10);

    if(psize < min_size || psize > 1500) {
        goto label_out_usage;
    }

    rreq_size = psize;
    dessert_notice("setting RREQ size to %" PRIu16 "", rreq_size);
    return CLI_OK;
}

int cli_set_gossip_p(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 1) {
    label_out_usage:
        cli_print(cli, "usage %s [0.0..1.0]\n", command);
        return CLI_ERROR;
    }

    double psize = strtod(argv[0], NULL);

    if(psize < 0 || psize > 1) {
        goto label_out_usage;
    }

    gossip_p = psize;
    cli_print(cli, "setting p for gossip to %lf", gossip_p);
    dessert_notice("setting p for gossip to %lf", gossip_p);
    return CLI_OK;
}

int cli_set_tracking_factor(struct cli_def* cli, char* command, char* argv[], int argc) { 

    if(argc != 1) { 
    label_out_usage: 
        cli_print(cli, "usage %s [0..1000]\n", command); 
        return CLI_ERROR; 
    } 

    uint16_t ptracking_factor = (uint16_t) strtoul(argv[0], NULL, 10); 

    if(ptracking_factor > 1000) { 
        goto label_out_usage; 
    } 

    tracking_factor = ptracking_factor; 
    aodv_db_pdr_upd_expected(hello_interval);

    dessert_notice("setting PDR_TRACKING_FACTOR to %" PRIu16 "", ptracking_factor); 
    return CLI_OK; 
} 

int cli_send_rreq(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 2) {
        cli_print(cli, "usage of %s command [hardware address as XX:XX:XX:XX:XX:XX] [initial_metric]\n", command);
        return CLI_ERROR_ARG;
    }

    mac_addr host;
    int ok = dessert_parse_mac(argv[0], &host);

    if(ok != 0) {
        cli_print(cli, "usage of %s command [hardware address as XX:XX:XX:XX:XX:XX] [initial_metric]\n", command);
        return CLI_ERROR_ARG;
    }

    metric_t initial_metric = metric_startvalue;
    initial_metric = atoi(argv[1]);

    cli_print(cli, MAC " -> using %" AODV_PRI_METRIC " as initial_metric\n", EXPLODE_ARRAY6(host), initial_metric);

    struct timeval ts;

    gettimeofday(&ts, NULL);

    aodv_send_rreq(host, &ts);

    return CLI_OK;
}


int cli_show_gossip_p(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "GOSSIP_P = %lf \n", gossip_p);
    return CLI_OK;
}

int cli_set_gossip(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 1) {
        cli_print(cli, "usage of %s command [gossip]\n", command);
        return CLI_ERROR_ARG;
    }

    char* gossip_string = argv[0];

    int goff = strcmp(gossip_string, "GOSSIP_NONE");
    if(goff == 0) {
        gossip_type = GOSSIP_NONE;
    }

    int g0 = strcmp(gossip_string, "GOSSIP_0");
    if(g0 == 0) {
        gossip_type = GOSSIP_0;
    }

    int g1 = strcmp(gossip_string, "GOSSIP_1");
    if(g1 == 0) {
        gossip_type = GOSSIP_1;
    }

    int g3 = strcmp(gossip_string, "GOSSIP_3");
    if(g3 == 0) {
        gossip_type = GOSSIP_3;
    }

    int p0 = strcmp(gossip_string, "PISSOG_0");
    if(p0 == 0) {
        gossip_type = PISSOG_0;
    }

    int p3 = strcmp(gossip_string, "PISSOG_3");
    if(p3 == 0) {
        gossip_type = PISSOG_3;
    }

    uint32_t count_out = 0;
    aodv_db_routing_reset(&count_out);

    cli_print(cli, "gossip set to %s....resetting routing table: %" PRIu32 " entries invalidated!", gossip_string, count_out);
    dessert_notice("gossip set to %s....resetting routing table: %" PRIu32 " entries invalidated!", gossip_string, count_out);
    return CLI_OK;
}

int cli_set_metric(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 1) {
        cli_print(cli, "usage of %s command [metric]\n", command);
        return CLI_ERROR_ARG;
    }

    char* metric_string = argv[0];

    int hop = strcmp(metric_string, "AODV_METRIC_HOP_COUNT");

    if(hop == 0) {
        metric_type = AODV_METRIC_HOP_COUNT;
        metric_startvalue = 0;
    }
#ifndef ANDROID
    int rssi = strcmp(metric_string, "AODV_METRIC_RSSI");

    if(rssi == 0) {
        metric_type = AODV_METRIC_RSSI;
        metric_startvalue = 0;
    }
#endif

    int etxadd = strcmp(metric_string, "AODV_METRIC_ETX_ADD");

    if(etxadd == 0) {
        metric_type = AODV_METRIC_ETX_ADD;
        metric_startvalue = 0;
    }

    int etxmul = strcmp(metric_string, "AODV_METRIC_ETX_MUL");

    if(etxmul == 0) {
        metric_type = AODV_METRIC_ETX_MUL;
        metric_startvalue = AODV_MAX_METRIC;
    }

    int rfc = strcmp(metric_string, "AODV_METRIC_RFC");

    if(rfc == 0) {
        metric_type = AODV_METRIC_RFC;
        metric_startvalue = 0;
    }

    int pdr = strcmp(metric_string, "AODV_METRIC_PDR");

    if(pdr == 0) {
        metric_type = AODV_METRIC_PDR;
        metric_startvalue = AODV_MAX_METRIC;
    }

    uint32_t count_out = 0;
    aodv_db_routing_reset(&count_out);

    cli_print(cli, "metric set to %s....resetting routing table: %" PRIu32 " entries invalidated!", metric_string, count_out);
    dessert_notice("metric set to %s....resetting routing table: %" PRIu32 " entries invalidated!", metric_string, count_out);
    return CLI_OK;
}

int cli_set_periodic_rreq_interval(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 1) {
        cli_print(cli, "usage %s [interval in ms]\n", command);
        return CLI_ERROR;
    }

    rreq_interval = strtol(argv[0], NULL, 10);

    dessert_periodic_del(send_rreq_periodic);
    send_rreq_periodic = NULL;

    if(rreq_interval == 0) {
        cli_print(cli, "periodic RREQ is off");
        dessert_notice("periodic RREQ is off");
        return CLI_OK;
    }

    struct timeval schedule_rreq_interval;

    schedule_rreq_interval.tv_sec = rreq_interval / 1000;

    schedule_rreq_interval.tv_usec = (rreq_interval % 1000) * 1000;

    send_rreq_periodic = dessert_periodic_add(aodv_periodic_send_rreq, NULL, NULL, &schedule_rreq_interval);

    cli_print(cli, "periodic RREQ Interval set to %" PRIu16 " ms", rreq_interval);

    dessert_notice("periodic RREQ Interval set to %" PRIu16 " ms", rreq_interval);

    return CLI_OK;
}

int cli_show_periodic_rreq_interval(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(rreq_interval == 0) {
        cli_print(cli, "periodic RREQ is off");
    }
    else {
        cli_print(cli, "periodic RREQ Interval = %" PRIu16 " ms", rreq_interval);
    }

    return CLI_OK;
}

int cli_set_preemptive_rreq_signal_strength_threshold(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(argc != 1) {
        cli_print(cli, "usage %s [threshold in dbm]\n", command);
        return CLI_ERROR;
    }

    signal_strength_threshold = strtol(argv[0], NULL, 10);

    if(signal_strength_threshold == 0) {
        cli_print(cli, "preemptive RREQ is off");
        dessert_notice("preemptive RREQ is off");
        return CLI_OK;
    }

    uint32_t count_out = 0;
    aodv_db_neighbor_reset(&count_out);

    cli_print(cli, "preemptive RREQ treshold is %" PRId8 " dbm - %" PRIu32 " neighbors invalidated...", signal_strength_threshold, count_out);
    dessert_notice("preemptive RREQ treshold is %" PRId8 " dbm - %" PRIu32 " neighbors invalidated...", signal_strength_threshold, count_out);

    return CLI_OK;
}

int cli_show_preemptive_rreq_signal_strength_threshold(struct cli_def* cli, char* command, char* argv[], int argc) {

    if(signal_strength_threshold == 0) {
        cli_print(cli, "preemptive RREQ is off");
    }
    else {
        cli_print(cli, "preemptive RREQ treshold is %" PRId8 " dbm", signal_strength_threshold);
    }

    return CLI_OK;
}

int cli_show_metric(struct cli_def* cli, char* command, char* argv[], int argc) {

    char* metric_string = NULL;

    switch(metric_type) {
        case AODV_METRIC_HOP_COUNT: {
            metric_string = "AODV_METRIC_HOP_COUNT";
            break;
        }
#ifndef ANDROID
        case AODV_METRIC_RSSI: {
            metric_string = "AODV_METRIC_RSSI";
            break;
        }
#endif
        case AODV_METRIC_ETX_ADD: {
            metric_string = "AODV_METRIC_ETX_ADD";
            break;
        }
        case AODV_METRIC_RFC: {
            metric_string = "AODV_METRIC_RFC";
            break;
        }
        case AODV_METRIC_ETX_MUL: {
            metric_string = "AODV_METRIC_ETX_MUL";
            break;
        }
        case AODV_METRIC_PDR: {
            metric_string = "AODV_METRIC_PDR";
            break;
        }
        default: {
            metric_string = "UNKNOWN METRIC -> you have some serious problems -> using AODV_METRIC_HOP_COUNT as fallback";
        }
    }

    cli_print(cli, "metric is set to %s", metric_string);
    return CLI_OK;
}

int cli_show_gossip(struct cli_def* cli, char* command, char* argv[], int argc) {

    char* gossip_string = NULL;

    switch(gossip_type) {
        case GOSSIP_NONE: {
            gossip_string = "GOSSIP_NONE";
            break;
        }
        case GOSSIP_0: {
            gossip_string = "GOSSIP_0";
            break;
        }
        case GOSSIP_1: {
            gossip_string = "GOSSIP_1";
            break;
        }
        case GOSSIP_3: {
            gossip_string = "GOSSIP_3";
            break;
        }
        case PISSOG_0: {
            gossip_string = "PISSOG_0";
            break;
        }
        case PISSOG_3: {
            gossip_string = "PISSOG_3";
            break;
        }
        default: {
            gossip_string = "UNKNOWN GOSSIP";
        }
    }

    cli_print(cli, "gossip is set to %s", gossip_string);
    return CLI_OK;
}


int cli_show_hello_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "HELLO size = %" PRIu16 " bytes\n", hello_size);
    return CLI_OK;
}

int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "HELLO interval = %" PRIu16 " millisec\n", hello_interval);
    return CLI_OK;
}

int cli_show_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "RREQ size = %" PRIu16 " bytes\n", rreq_size);
    return CLI_OK;
}

int cli_show_tracking_factor(struct cli_def* cli, char* command, char* argv[], int argc) { 
    cli_print(cli, "PDR_TRACKING_FACTOR = %" PRIu16 "\n", tracking_factor); 
    return CLI_OK; 
} 

int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* rt_report;
    aodv_db_view_routing_table(&rt_report);
    cli_print(cli, "\n%s\n", rt_report);
    free(rt_report);
    return CLI_OK;
}

int cli_show_pdr_nt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* pdr_nt_report;
    aodv_db_view_pdr_nt(&pdr_nt_report);
    cli_print(cli, "\n%s\n", pdr_nt_report);
    free(pdr_nt_report);
    return CLI_OK;
}

int cli_show_neighbor_timeslot(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    aodv_db_neighbor_timeslot_report(&report);
    cli_print(cli, "\n%s\n", report);
    free(report);
    return CLI_OK;
}

int cli_show_packet_buffer_timeslot(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    aodv_db_packet_buffer_timeslot_report(&report);
    cli_print(cli, "\n%s\n", report);
    free(report);
    return CLI_OK;
}

int cli_show_data_seq_timeslot(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    aodv_db_data_seq_timeslot_report(&report);
    cli_print(cli, "\n%s\n", report);
    free(report);
    return CLI_OK;
}
