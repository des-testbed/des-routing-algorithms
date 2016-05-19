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
#include "batman_cli.h"
#include "stdlib.h"
#include "linux/if_ether.h"
#include "../database/batman_database.h"
#include "../pipeline/batman_pipeline.h"
#include "../config.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

// ----------------------- Setup -----------------------------------------------------

int batman_cli_change_ogmint(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t new_ogm_int;

    if(argc != 1 || sscanf(argv[0], "%u", &new_ogm_int) != 1) {
        cli_print(cli, "usage of %s command [ogm_interval]\n", command);
        return CLI_ERROR_ARG;
    }

    batman_periodic_register_send_ogm(new_ogm_int);
    dessert_info("set OGM interval to %i", new_ogm_int);
    return CLI_OK;
}

int batman_cli_beverbose(struct cli_def* cli, char* command, char* argv[], int argc) {
    uint32_t mode;

    if(argc != 1 || sscanf(argv[0], "%u", &mode) != 1 || (mode != 0 && mode != 1)) {
        cli_print(cli, "usage of %s command [0, 1]\n", command);
        return CLI_ERROR_ARG;
    }

    if(mode == 1) {
        dessert_info("be verbose = true");
        be_verbose = true;
    }
    else {
        dessert_info("be verbose = false");
        be_verbose = false;
    }

    return CLI_OK;
}

int batman_cli_ogmprecursormode(struct cli_def* cli, char* command, char* argv[], int argc) {
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



// ----------------------- CLI Debug and Report --------------------------------------

int batman_cli_print_irt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* rt_report;
    batman_db_rlock();
    batman_db_irt_report(&rt_report);
    batman_db_unlock();
    cli_print(cli, "\n%s\n", rt_report);
    free(rt_report);
    return CLI_OK;
}

int batman_cli_print_brt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* rt_report;
    batman_db_rlock();
    batman_db_brt_report(&rt_report);
    batman_db_unlock();
    cli_print(cli, "\n%s\n", rt_report);
    free(rt_report);
    return CLI_OK;
}

int batman_cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc) {
    char* rt_report;
    batman_db_rlock();
    batman_db_rt_report(&rt_report);
    batman_db_unlock();
    cli_print(cli, "\n%s\n", rt_report);
    free(rt_report);
    return CLI_OK;
}

// -------------------- common cli functions ----------------------------------------------

int cli_setrouting_log(struct cli_def* cli, char* command, char* argv[], int argc) {
    routing_log_file = malloc(strlen(argv[0]));
    strcpy(routing_log_file, argv[0]);
    FILE* f = fopen(routing_log_file, "a+");
    time_t lt;
    lt = time(NULL);
    fprintf(f, "\n--- %s\n", ctime(&lt));
    fclose(f);
    dessert_info("logging routing data at file %s", routing_log_file);
    return CLI_OK;
}
