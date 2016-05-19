/******************************************************************************
 Copyright 2010, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
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
 ------------------------------------------------------------------------------

 ******************************************************************************/

#include "dsr.h"

pthread_rwlock_t _dsr_conf_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _CONF_READLOCK pthread_rwlock_rdlock(&_dsr_conf_rwlock)
#define _CONF_WRITELOCK pthread_rwlock_wrlock(&_dsr_conf_rwlock)
#define _CONF_UNLOCK pthread_rwlock_unlock(&_dsr_conf_rwlock)
#define _SAFE_RETURN(x) _CONF_UNLOCK; return(x);

dsr_conf_t dsr_conf;

/* local forward declarations */
int dsr_cli_cmd_set_routemaintenance_passive_ack_status(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_routemaintenance_network_ack_status(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_retransmission_count(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_retransmission_timeout(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_sendbuffer_timeout(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_routediscovery_timeout(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_routediscovery_maximum_retries(struct cli_def* cli, char* command, char* argv[], int argc);
int dsr_cli_cmd_set_routediscovery_expanding_ring_search_status(struct cli_def* cli, char* command, char* argv[], int argc);

int dsr_cli_cmd_info_conf(struct cli_def* cli, char* command, char* argv[], int argc);

static inline int _set_routemaintenance_passive_ack_status(int status);
static inline int _set_routemaintenance_network_ack_status(int status);
static inline int _set_retransmission_count(int count);
static inline int _set_retransmission_timeout(__suseconds_t timeout);
static inline int _set_sendbuffer_timeout(__suseconds_t timeout);
static inline int _set_routediscovery_timeout(__suseconds_t timeout);
static inline int _set_routediscovery_maximum_retries(int count);
static inline int _set_routediscovery_expanding_ring_search_status(int status);

inline void dsr_conf_initialize(void) {
    _set_routemaintenance_passive_ack_status(DSR_CONFVAR_ROUTEMAINTENANCE_PASSIVE_ACK);
    _set_routemaintenance_network_ack_status(DSR_CONFVAR_ROUTEMAINTENANCE_NETWORK_ACK);
    _set_retransmission_count(DSR_CONFVAR_RETRANSMISSION_COUNT);
    _set_retransmission_timeout(DSR_CONFVAR_RETRANSMISSION_TIMEOUT);
    _set_sendbuffer_timeout(DSR_CONFVAR_SENDBUFFER_TIMEOUT);
    _set_routediscovery_timeout(DSR_CONFVAR_ROUTEDISCOVERY_TIMEOUT);
    _set_routediscovery_maximum_retries(DSR_CONFVAR_ROUTEDISCOVERY_MAXIMUM_RETRIES);
    _set_routediscovery_expanding_ring_search_status(DSR_CONFVAR_ROUTEDISCOVERY_EXPANDING_RING_SEARCH);
}

void dsr_conf_register_cli_callbacks(struct cli_command* cli_cfg_set, struct cli_command* cli_exec_info) {
    cli_register_command(dessert_cli, cli_cfg_set , "routemaintenance_passive_ack",
        dsr_cli_cmd_set_routemaintenance_passive_ack_status, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set route maintenance passive ack feature");

    cli_register_command(dessert_cli, cli_cfg_set , "routemaintenance_network_ack",
        dsr_cli_cmd_set_routemaintenance_network_ack_status, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set route maintenance network ack feature");

    cli_register_command(dessert_cli, cli_cfg_set , "retransmission_count",
        dsr_cli_cmd_set_retransmission_count, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set maximum retransmission attempts (no ACK received in time)");

    cli_register_command(dessert_cli, cli_cfg_set , "retransmission_timeout",
        dsr_cli_cmd_set_retransmission_timeout, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set retransmission timeout (time to wait for ACK per retry)");

    cli_register_command(dessert_cli, cli_cfg_set , "sendbuffer_timeout",
        dsr_cli_cmd_set_sendbuffer_timeout, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set sendbuffer timeout (time to wait for a route)");

    cli_register_command(dessert_cli, cli_cfg_set , "routediscovery_timeout",
        dsr_cli_cmd_set_routediscovery_timeout, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set route discovery timeout (initial time to wait for a REPL)");

    cli_register_command(dessert_cli, cli_cfg_set , "routediscovery_maximum_retries",
        dsr_cli_cmd_set_routediscovery_maximum_retries, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set route discovery maximum retries (NO REPL received in time)");

    cli_register_command(dessert_cli, cli_cfg_set , "routediscovery_expanding_ring_search",
        dsr_cli_cmd_set_routediscovery_expanding_ring_search_status, PRIVILEGE_UNPRIVILEGED, MODE_CONFIG,
        "set route discovery expanding ring search feature");

    cli_register_command(dessert_cli, cli_exec_info , "conf",
        dsr_cli_cmd_info_conf, PRIVILEGE_UNPRIVILEGED, MODE_EXEC,
        "show the current configuration (set *)");
}

inline int dsr_conf_get_routemaintenance_passive_ack(void) {
    int status;
    _CONF_READLOCK;
    status = dsr_conf.routemaintenance_passive_ack;
    _CONF_UNLOCK;
    return status;
}

inline int dsr_conf_get_routemaintenance_network_ack(void) {
    int status;
    _CONF_READLOCK;
    status = dsr_conf.routemaintenance_network_ack;
    _CONF_UNLOCK;
    return status;
}

inline int dsr_conf_get_retransmission_count(void) {
    int count_copy;
    _CONF_READLOCK;
    count_copy = dsr_conf.retransmission_count;
    _CONF_UNLOCK;
    return count_copy;
}

inline __suseconds_t dsr_conf_get_retransmission_timeout(void) {
    __suseconds_t timeout_copy;
    _CONF_READLOCK;
    timeout_copy = dsr_conf.retransmission_timeout;
    _CONF_UNLOCK;
    return timeout_copy;
}

inline __suseconds_t dsr_conf_get_sendbuffer_timeout(void) {
    __suseconds_t timeout_copy;
    _CONF_READLOCK;
    timeout_copy = dsr_conf.sendbuffer_timeout;
    _CONF_UNLOCK;
    return timeout_copy;
}

inline __suseconds_t dsr_conf_get_routediscovery_timeout(void) {
    __suseconds_t timeout_copy;
    _CONF_READLOCK;
    timeout_copy = dsr_conf.routediscovery_timeout;
    _CONF_UNLOCK;
    return timeout_copy;
}

inline int dsr_conf_get_routediscovery_maximum_retries(void) {
    int count_copy;
    _CONF_READLOCK;
    count_copy = dsr_conf.routediscovery_maximum_retries;
    _CONF_UNLOCK;
    return count_copy;
}

inline int dsr_conf_get_routediscovery_expanding_ring_search(void) {
    int status;
    _CONF_READLOCK;
    status = dsr_conf.routediscovery_expanding_ring_search;
    _CONF_UNLOCK;
    return status;
}

/******************************************************************************
 *
 * C L I --
 *
 ******************************************************************************/

/** CLI command - exec mode - set routemaintenance_passiv_ack $n */
int dsr_cli_cmd_set_routemaintenance_passive_ack_status(struct cli_def* cli, char* command, char* argv[], int argc) {
    int status;
    int i;

    if(argc != 1 || sscanf(argv[0], "%i", &status) != 1) {
        cli_print(cli, "usage %s [status]\n", command);
        return CLI_ERROR;
    }

    i = _set_routemaintenance_passive_ack_status(status);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set routemaintenance_network_ack $n */
int dsr_cli_cmd_set_routemaintenance_network_ack_status(struct cli_def* cli, char* command, char* argv[], int argc) {
    int status;
    int i;

    if(argc != 1 || sscanf(argv[0], "%i", &status) != 1) {
        cli_print(cli, "usage %s [status]\n", command);
        return CLI_ERROR;
    }

    i = _set_routemaintenance_network_ack_status(status);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set retransmission_count $n */
int dsr_cli_cmd_set_retransmission_count(struct cli_def* cli, char* command, char* argv[], int argc) {
    int count;
    int i;

    if(argc != 1 || sscanf(argv[0], "%i", &count) != 1) {
        cli_print(cli, "usage %s [retransmissions]\n", command);
        return CLI_ERROR;
    }

    i = _set_retransmission_count(count);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set retransmission_timeout $n */
int dsr_cli_cmd_set_retransmission_timeout(struct cli_def* cli, char* command, char* argv[], int argc) {
    __suseconds_t timeout;
    int i;

    if(argc != 1 || sscanf(argv[0], "%li", &timeout) != 1) {
        cli_print(cli, "usage %s [retransmission timeout (microseconds)]\n", command);
        return CLI_ERROR;
    }

    i = _set_retransmission_timeout(timeout);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set sendbuffer_timeout $n */
int dsr_cli_cmd_set_sendbuffer_timeout(struct cli_def* cli, char* command, char* argv[], int argc) {
    __suseconds_t timeout;
    int i;

    if(argc != 1 || sscanf(argv[0], "%li", &timeout) != 1) {
        cli_print(cli, "usage %s [sendbuffer timeout (microseconds)]\n", command);
        return CLI_ERROR;
    }

    i = _set_sendbuffer_timeout(timeout);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set routediscovery_timeout $n */
int dsr_cli_cmd_set_routediscovery_timeout(struct cli_def* cli, char* command, char* argv[], int argc) {
    __suseconds_t timeout;
    int i;

    if(argc != 1 || sscanf(argv[0], "%li", &timeout) != 1) {
        cli_print(cli, "usage %s [route discovery timeout (microseconds)]\n", command);
        return CLI_ERROR;
    }

    i = _set_routediscovery_timeout(timeout);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set routediscovery_maximum_retries $n */
int dsr_cli_cmd_set_routediscovery_maximum_retries(struct cli_def* cli, char* command, char* argv[], int argc) {
    int count;
    int i;

    if(argc != 1 || sscanf(argv[0], "%i", &count) != 1) {
        cli_print(cli, "usage %s [retries]\n", command);
        return CLI_ERROR;
    }

    i = _set_routediscovery_maximum_retries(count);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - set routediscovery_expanding_ring_search $n */
int dsr_cli_cmd_set_routediscovery_expanding_ring_search_status(struct cli_def* cli, char* command, char* argv[], int argc) {
    int status;
    int i;

    if(argc != 1 || sscanf(argv[0], "%i", &status) != 1) {
        cli_print(cli, "usage %s [status]\n", command);
        return CLI_ERROR;
    }

    i = _set_routediscovery_expanding_ring_search_status(status);

    return (i == 0 ? CLI_OK : CLI_ERROR);
}

/** CLI command - exec mode - info conf  */
int dsr_cli_cmd_info_conf(struct cli_def* cli, char* command, char* argv[], int argc) {
    _CONF_READLOCK;
    cli_print(cli, "retransmission count %i", dsr_conf.retransmission_count);
    cli_print(cli, "retransmission timeout %li", dsr_conf.retransmission_timeout);
    cli_print(cli, "sendbuffer timeout %li", dsr_conf.sendbuffer_timeout);
    cli_print(cli, "route discovery timeout %li", dsr_conf.routediscovery_timeout);
    cli_print(cli, "route discovery maximum retries %i", dsr_conf.routediscovery_maximum_retries);
    cli_print(cli, "route discovery expanding ring search %i", dsr_conf.routediscovery_expanding_ring_search);
    cli_print(cli, "routemaintenance network ack %i", dsr_conf.routemaintenance_network_ack);
    cli_print(cli, "routemaintenance passive ack %i", dsr_conf.routemaintenance_passive_ack);
    _CONF_UNLOCK;
    return CLI_OK;
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

static inline int _set_routemaintenance_passive_ack_status(int status) {
    dessert_info("setting route maintenance passive ack to %i", status);

    _CONF_WRITELOCK;
    dsr_conf.routemaintenance_passive_ack = status;
    _SAFE_RETURN(CLI_OK);
}

static inline int _set_routemaintenance_network_ack_status(int status) {
    dessert_info("setting route maintenance network ack to %i", status);

    _CONF_WRITELOCK;
    dsr_conf.routemaintenance_network_ack = status;
    _SAFE_RETURN(CLI_OK);
}

static inline int _set_retransmission_count(int count) {
    dessert_info("setting retransmission count to %i", count);

    _CONF_WRITELOCK;
    dsr_conf.retransmission_count = count;
    _SAFE_RETURN(CLI_OK);
}

static inline int _set_retransmission_timeout(__suseconds_t timeout) {
    struct timeval maintenance_buffer_cleanup_interval;
    maintenance_buffer_cleanup_interval.tv_sec = 0;
    maintenance_buffer_cleanup_interval.tv_usec = 0;

    TIMEVAL_ADD_SAFE(&maintenance_buffer_cleanup_interval, 0, 5000); //XXX HOTFIX

    dessert_info("setting retransmission timeout to %li", timeout);

    _CONF_WRITELOCK;
    dsr_conf.retransmission_timeout = timeout;

    if(dsr_conf.retransmission_timeout_periodic != NULL) {
        dessert_periodic_del(dsr_conf.retransmission_timeout_periodic);
    }

    dsr_conf.retransmission_timeout_periodic = dessert_periodic_add(cleanup_maintenance_buffer, NULL, NULL, &maintenance_buffer_cleanup_interval);
    _CONF_UNLOCK;

    return (dsr_conf.retransmission_timeout_periodic != NULL ? CLI_OK : CLI_ERROR);
}

static inline int _set_sendbuffer_timeout(__suseconds_t timeout) {
    dessert_info("setting sendbuffer timeout to %li", timeout);

    _CONF_WRITELOCK;
    dsr_conf.sendbuffer_timeout = timeout;
    _SAFE_RETURN(CLI_OK);
}

static inline int _set_routediscovery_timeout(__suseconds_t timeout) {
    struct timeval routediscovery_run_interval;
    routediscovery_run_interval.tv_sec = 0;
    routediscovery_run_interval.tv_usec = 0;

    TIMEVAL_ADD_SAFE(&routediscovery_run_interval, 0, timeout);

    dessert_info("setting route discovery timeout to %li", timeout);

    _CONF_WRITELOCK;
    dsr_conf.routediscovery_timeout = timeout;

    if(dsr_conf.routediscovery_timeout_periodic != NULL) {
        dessert_periodic_del(dsr_conf.routediscovery_timeout_periodic);
    }

    dsr_conf.routediscovery_timeout_periodic = dessert_periodic_add(
                run_rreqtable, NULL, NULL, &routediscovery_run_interval);
    _CONF_UNLOCK;

    return (dsr_conf.routediscovery_timeout_periodic != NULL ? CLI_OK : CLI_ERROR);
}

static inline int _set_routediscovery_maximum_retries(int count) {
    dessert_info("setting maximum route discovery retries to %i", count);

    _CONF_WRITELOCK;
    dsr_conf.routediscovery_maximum_retries = count;
    _SAFE_RETURN(CLI_OK);
}

static inline int _set_routediscovery_expanding_ring_search_status(int status) {
    dessert_info("setting route discovery expanding ring search to %i", status);

    _CONF_WRITELOCK;
    dsr_conf.routediscovery_expanding_ring_search = status;
    _SAFE_RETURN(CLI_OK);
}
