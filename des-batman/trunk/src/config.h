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
#include <linux/if_ether.h>
#include <stdbool.h>
#include <dessert.h>

#ifndef BATMAN_CONFIG
#define BATMAN_CONFIG

#define RESEND_OGM_ALWAYS		false
#define VERSION 				5
#define TTL_MAX 				255
#define SEQNO_MAX 				65535
#define OGM_INTERVAL_MS         2000
#define WINDOW_SIZE				20
#define RL_TIMEOUT				WINDOW_SIZE * 2 * OGM_INTERVAL_MS
#define WINDOW_SWITCH_DIFF		1
#define PURGE_TIMEOUT_KOEFF		2
#define PURGE_TIMEOUT			PURGE_TIMEOUT_KOEFF * WINDOW_SIZE * OGM_INTERVAL_MS
#define NEIGHBOR_TIMEOUT		PURGE_TIMEOUT
#define DB_CLEANUP_INTERVAL		PURGE_TIMEOUT

enum ext_types {
    OGM_EXT_TYPE = DESSERT_EXT_USER,
    RL_EXT_TYPE,
    BROADCAST_EXT_TYPE
};

#define OGM_RESET_COUNT         3

// default values
#define USE_PRECURSOR_LIST      false
#define OGM_PREC_LIST_SIZE      12      // size of precursor list in OGM. Size MUST be between 1 and 255
#define BROADCAST_LOG_TIMEOUT   10
#define OGM_SIZE                128

extern bool     ogm_precursor_mode;
extern bool     resend_ogm_always;
extern uint8_t  window_size;
extern uint16_t ogm_interval;
extern uint16_t ogm_size;

#endif
