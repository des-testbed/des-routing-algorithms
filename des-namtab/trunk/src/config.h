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

#ifndef BATMAN_CONFIG
#define BATMAN_CONFIG

#include <stdbool.h>
#include <dessert.h>

#define VERSION 				5
#define TTL_MIN 				2
#define TTL_MAX 				255
#define SEQNO_MAX 				65535
#define BROADCAST_DELAY_MAX 	100
#define LOG_INTERVAL			60
#define ORIG_INTERVAL			2
#define ORIG_INTERVAL_JITTER	200
#define WINDOW_SIZE				16
#define RL_TIMEOUT 				WINDOW_SIZE * 2 * ORIG_INTERVAL
#define WINDOW_SWITCH_DIFF		1			// difference between seq_num to switch next hop
// avoids route blinking

#define BRC_PURGE_TIME			8
#define PURGE_TIMEOUT_KOEFF		2
#define PUDGE_TIMEOUT			PURGE_TIMEOUT_KOEFF * WINDOW_SIZE * ORIG_INTERVAL
#define NEIGHBOR_TIMEOUT		PUDGE_TIMEOUT
#define DB_CLEANUP_INTERVAL		PUDGE_TIMEOUT
#define OGM_EXT_TYPE			DESSERT_EXT_USER
#define OGM_INVRT_EXT_TYPE		DESSERT_EXT_USER + 1
#define BRC_STAMP_EXT_TYPE		DESSERT_EXT_USER + 2
#define RL_EXT_TYPE				DESSERT_EXT_USER + 3
#define OGM_EXT_LEN				sizeof(struct batman_msg_ogm)
#define OGM_RESET_COUNT			3

#define USE_PRECURSOR_LIST		true		// 1 = user precursor list
#define OGM_PREC_LIST_SIZE		12 			// size of precursor list in OGM. Size MUST be between 1 and 255

extern int 						be_verbose;
extern int 						ogm_precursor_mode;
extern int 						cli_port;
extern char*					routing_log_file;


#endif
