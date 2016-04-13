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
#include <stdlib.h>
#include <string.h>
#include "batman_cli.h"
#include "linux/if_ether.h"
#include "../database/batman_database.h"
#include "../pipeline/batman_pipeline.h"
#include "../config.h"

// ----------------------- Setup -----------------------------------------------------

int cli_set_window_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int new_window_size;

    if(argc != 1 || sscanf(argv[0], "%u", &new_window_size) != 1) {
        cli_print(cli, "usage of %s command [0..255]\n", command);
        return CLI_ERROR_ARG;
    }

    window_size = new_window_size;
    batman_db_wlock();
    batman_db_change_pt(PURGE_TIMEOUT_KOEFF * ogm_interval * window_size);
    batman_db_change_window_size((uint8_t) new_window_size);
    batman_db_unlock();
    dessert_info("set WINDOW_SIZE to %i", new_window_size);
    return CLI_OK;
}

int cli_set_ogm_resend_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t mode;

    if(argc != 1 || sscanf(argv[0], "%u", &mode) != 1 || (mode != 0 && mode != 1)) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(mode == 1) {
        dessert_info("re-send OGMs always = true");
        resend_ogm_always = true;
    }
    else {
        dessert_info("re-send OGMs always = false");
        resend_ogm_always = false;
    }

    return CLI_OK;
}

int cli_set_ogm_precursor_mode(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t mode;

    if(argc != 1 || sscanf(argv[0], "%u", &mode) != 1 || (mode != 0 && mode != 1)) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(mode == 1) {
        dessert_info("use OGM precursor mode = true");
        ogm_precursor_mode = true;
    }
    else {
        dessert_info("use OGM precursor mode = false");
        ogm_precursor_mode = false;
    }

    return CLI_OK;
}

int cli_set_ogm_size(struct cli_def* cli, char* command, char* argv[], int argc) {
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

    ogm_size = psize;
    dessert_notice("setting OGM size to %d", ogm_size);
    return CLI_OK;
}

int cli_set_ogm_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    unsigned int new_ogm_int;

    if(argc != 1 || sscanf(argv[0], "%u", &new_ogm_int) != 1) {
        cli_print(cli, "usage of %s command [ogm_interval in millisec]\n", command);
        return CLI_ERROR_ARG;
    }

    ogm_interval = (uint16_t) new_ogm_int;
    batman_periodic_register_send_ogm(new_ogm_int);
    dessert_info("set OGM interval to %i", new_ogm_int);
    return CLI_OK;
}

// ----------------------- CLI Debug and Report --------------------------------------

int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* rt_report;
    batman_db_rlock();
    batman_db_view_routingtable(&rt_report);
    batman_db_unlock();
    cli_print(cli, "\n%s\n", rt_report);
    free(rt_report);
    return CLI_OK;
}

int cli_show_ogm_interval(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "OGM interval = %d millisec\n", ogm_interval);
    return CLI_OK;
}

int cli_show_ogm_size(struct cli_def* cli, char* command, char* argv[], int argc) {
    cli_print(cli, "OGM size = %d bytes\n", ogm_size);
    return CLI_OK;
}
