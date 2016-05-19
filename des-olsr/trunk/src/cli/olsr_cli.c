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
#include <time.h>
#include <stdio.h>
#include "olsr_cli.h"
#include "../database/olsr_database.h"
#include "../pipeline/olsr_pipeline.h"
#include "../config.h"

// -------------------- config ------------------------------------------------------------

int cli_set_hello_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t min_size = sizeof(dessert_msg_t) + sizeof(struct ether_header) + 2;

    if(argc != 1) {
    label_out_usage:
        cli_print(cli, "usage %s [%d..1500]\n", command, min_size);
        return CLI_ERROR;
    }

    uint16_t psize = (uint16_t) strtoul(argv[0], NULL, 10);

    if(psize < min_size || psize > 1500) {
        goto label_out_usage;
    }

    hello_size = psize;
    dessert_notice("setting HELLO size to %d", hello_size);
    return CLI_OK;
}

int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s  [interval]\n", command);
        return CLI_ERROR;
    }

    hello_interval_ms = strtoul(argv[0], NULL, 10);
    dessert_periodic_del(periodic_send_hello);
    struct timeval hello_interval_tv;
    hello_interval_tv.tv_sec = hello_interval_ms / 1000;
    hello_interval_tv.tv_usec = (hello_interval_ms % 1000) * 1000;
    periodic_send_hello = dessert_periodic_add(olsr_periodic_send_hello, NULL, NULL, &hello_interval_tv);
    dessert_notice("setting HELLO interval to %d", hello_interval_ms);
    return CLI_OK;
}

int cli_set_tc_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint16_t min_size = sizeof(dessert_msg_t) + sizeof(struct ether_header) + 2;

    if(argc != 1) {
    label_out_usage:
        cli_print(cli, "usage %s [%d..1500]\n", command, min_size);
        return CLI_ERROR;
    }

    uint16_t psize = (uint16_t) strtoul(argv[0], NULL, 10);

    if(psize < min_size || psize > 1500) {
        goto label_out_usage;
    }

    tc_size = psize;
    dessert_notice("setting TC size to %d", tc_size);
    return CLI_OK;
}

int cli_set_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s  [interval]\n", command);
        return CLI_ERROR;
    }

    tc_interval_ms = strtoul(argv[0], NULL, 10);
    dessert_periodic_del(periodic_send_tc);
    struct timeval tc_interval_tv;
    tc_interval_tv.tv_sec = tc_interval_ms / 1000;
    tc_interval_tv.tv_usec = (tc_interval_ms % 1000) * 1000;
    periodic_send_tc = dessert_periodic_add(olsr_periodic_send_tc, NULL, NULL, &tc_interval_tv);
    dessert_notice("setting TC interval to %d", tc_interval_ms);
    return CLI_OK;
}

int cli_set_ett_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s  [interval]\n", command);
        return CLI_ERROR;
    }

    ett_interval = strtoul(argv[0], NULL, 10);
    dessert_periodic_del(periodic_send_ett);
    struct timeval ett_interval_tv;
    ett_interval_tv.tv_sec = ett_interval / 1000;
    ett_interval_tv.tv_usec = (ett_interval % 1000) * 1000;
    periodic_send_ett = dessert_periodic_add(olsr_periodic_send_ett, NULL, NULL, &ett_interval_tv);
    dessert_notice("setting ETT interval to %d", ett_interval);
    return CLI_OK;
}

int cli_set_rt_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s  [ms]\n", command);
        return CLI_ERROR;
    }

    rt_interval_ms = strtoul(argv[0], NULL, 10);
    dessert_periodic_del(periodic_rt);
    struct timeval rt_interval_tv;
    rt_interval_tv.tv_sec = rt_interval_ms / 1000;
    rt_interval_tv.tv_usec = (rt_interval_ms % 1000) * 1000;
    periodic_rt = dessert_periodic_add(olsr_periodic_build_routingtable, NULL, NULL, &rt_interval_tv);
    dessert_notice("setting RT interval to %d [ms]", rt_interval_ms);
    return CLI_OK;
}

int cli_show_rt_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "RT interval is set to %d [ms]", rt_interval_ms);
    return CLI_OK;
}

int cli_set_max_missed_tc(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int tcoeff;

    if(argc != 1 || sscanf(argv[0], "%u", &tcoeff) != 1) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(tcoeff >= max_missed_hello) {
        max_missed_tc = (uint16_t) tcoeff;
        cli_print(cli, "set maximum number of tolerated missed TCs to %d", max_missed_tc);
        dessert_notice("set maximum number of tolerated missed TCs to %d", max_missed_tc);
        return CLI_OK;
    }

    cli_print(cli, "ERROR: max_missed_tc < max_missed_hello");
    dessert_err("max_missed_tc < max_missed_hello");
    return CLI_ERROR_ARG;
}

int cli_show_max_missed_tc(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "maximum number of tolerated missed TCs = %d", max_missed_tc);
    return CLI_OK;
}

int cli_set_max_missed_hello(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int tcoeff;

    if(argc != 1 || sscanf(argv[0], "%u", &tcoeff) != 1) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(max_missed_tc >= tcoeff) {
        max_missed_hello = (uint16_t) tcoeff;
        cli_print(cli, "set maximum number of tolerated missed HELLOs to %d", max_missed_hello);
        dessert_notice("set maximum number of tolerated missed HELLOs to %d", max_missed_hello);
        return CLI_OK;
    }

    cli_print(cli, "ERROR: max_missed_tc < max_missed_hello");
    dessert_err("max_missed_tc < max_missed_hello");
    return CLI_ERROR_ARG;
}


int cli_show_max_missed_hello(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "maximum number of tolerated missed HELLOs = %d", max_missed_hello);
    return CLI_OK;
}

int cli_set_willingness(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int mode;

    if(argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    willingness = (uint8_t) mode;
    dessert_debug("set WILLINGNESS to %i", willingness);
    return CLI_OK;
}

int cli_set_window_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int mode;

    if(argc != 1 || sscanf(argv[0], "%u", &mode) != 1) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    window_size = (uint8_t) mode;
    dessert_debug("set WINDOW_SIZE to %i", window_size);
    return CLI_OK;
}

const char* metric2str[] = {
    "unknown",
    "PLR/PDR",
    "HC",
    "ETX",
    "ETX-ADD",
    "ETT"
};

int cli_show_rc_metric(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "metric = %s", metric2str[rc_metric]);
    return CLI_OK;
}

int cli_set_rc_metric(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        goto error;
    }

    if(strcmp(argv[0], "PLR") == 0 || strcmp(argv[0], "PDR") == 0) {
        rc_metric = RC_METRIC_PLR;
        dessert_notice("set metric to PLR (packet lost rate) where PLR = 1-PDR");
        cli_print(cli, "set metric to PLR (packet lost rate) where PLR = 1-PDR");
        goto ok;
    }
    else if(strcmp(argv[0], metric2str[RC_METRIC_HC]) == 0) {
        rc_metric = RC_METRIC_HC;
        dessert_notice("set metric to HC (hop count)");
        cli_print(cli, "set metric to HC (hop count)");
        goto ok;
    }
    else if(strcmp(argv[0], metric2str[RC_METRIC_ETX]) == 0) {
        rc_metric = RC_METRIC_ETX;
        dessert_notice("set metric to ETX (multiplicative ETX)");
        cli_print(cli, "set metric to ETX (multiplicative ETX)");
        goto ok;
    }
    else if(strcmp(argv[0], metric2str[RC_METRIC_ETX_ADD]) == 0) {
        rc_metric = RC_METRIC_ETX_ADD;
        dessert_notice("set metric to ETX-ADD (additive ETX)");
        cli_print(cli, "set metric to ETX-ADD (additive ETX)");
        goto ok;
    }
    else if(strcmp(argv[0], metric2str[RC_METRIC_ETT]) == 0) {
        rc_metric = RC_METRIC_ETT;
        dessert_debug("set metric to ETT (expected transmission time)");
        cli_print(cli, "set metric to ETT (expected transmission time)");
        goto ok;
    }

error:
    cli_print(cli, "usage: set %s [PLR, PDR,, HC, ETX, ETX-ADD, ETT]\n", command);
    return CLI_ERROR;

ok:
    return CLI_OK;
}

int cli_show_fisheye(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "fisheye = %s", fisheye ? "on" : "off");
    return CLI_OK;
}

int cli_set_fisheye(struct cli_def* cli, char* command, char* argv[], int argc) {
    if(argc != 1) {
        goto error;
    }

    if(strcmp(argv[0], "on") == 0 || strcmp(argv[0], "1") == 0 || strcmp(argv[0], "true") == 0) {
        fisheye = true;
        dessert_notice("enabling fisheye routing");
        cli_print(cli, "enabling fisheye routing");
        goto ok;
    }
    if(strcmp(argv[0], "off") == 0 || strcmp(argv[0], "0") == 0 || strcmp(argv[0], "false") == 0) {
        fisheye = false;
        dessert_notice("disabling fisheye routing");
        cli_print(cli, "disabling fisheye routing");
        goto ok;
    }

error:
    cli_print(cli, "usage: set %s [on,off]\n", command);
    return CLI_ERROR;

ok:
    return CLI_OK;
}

// -------------------- Testing ------------------------------------------------------------

/**
* Print hello size
*/
int cli_show_hello_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "Hello size = %d bytes\n", hello_size);
    return CLI_OK;
}

int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "Hello interval = %d [ms]\n", hello_interval_ms);
    return CLI_OK;
}

int cli_show_tc_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "TC size = %d bytes\n", tc_size);
    return CLI_OK;
}

int cli_show_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "TC interval = %d [ms]\n", tc_interval_ms);
    return CLI_OK;
}

int cli_show_ett_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "ETT interval = %d [ms]\n", ett_interval);
    return CLI_OK;
}

/**
* Print neighbor set table
*/
int cli_show_ns(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_wlock();
    int result = olsr_db_ns_report(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
* Print neighbor set table (simple output)
*/
int cli_show_ns_so(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_wlock();
    int result = olsr_db_ns_report_so(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
* Print link set table
*/
int cli_show_ls(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_wlock();
    int result = olsr_db_ls_report(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
* Print 2hop neighbor set table
*/
int cli_show_2hns(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_wlock();
    int result = olsr_db_2hns_report(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
* Print TC set table
*/
int cli_show_tc(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_wlock();
    int result = olsr_db_tc_report(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
 * Print ETT table
 */
int cli_show_ett(struct cli_def* cli, char* command, char* argv[], int argc){
    char* report;
    olsr_db_wlock();
    int result = olsr_db_ett_report(&report);
    olsr_db_unlock();
    if (result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }
    return CLI_OK;
}

/**
* Print routing table
*/
int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_rlock();
    int result = olsr_db_rt_report(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}

/**
* Print routing table (simple output)
*/
int cli_show_rt_so(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* report;
    olsr_db_rlock();
    int result = olsr_db_rt_report_so(&report);
    olsr_db_unlock();

    if(result == true) {
        cli_print(cli, "\n%s\n", report);
        free(report);
    }

    return CLI_OK;
}
