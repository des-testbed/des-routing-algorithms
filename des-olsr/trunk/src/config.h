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

#ifndef OLSR_CONFIG
#define OLSR_CONFIG

#include <stdlib.h>
#include <stdint.h>
#include <dessert.h>

enum extension_types {
    HELLO_EXT_TYPE = DESSERT_EXT_USER,
    HELLO_NEIGH_DESRC_TYPE,
    TC_EXT_TYPE,
    BROADCAST_ID_EXT_TYPE,
    RL_EXT_TYPE,
    ETT_EXT_TYPE
};

#define SEQNO_MAX                   (1 << 16) - 1

// emission intervals
#define HELLO_INTERVAL_MS           2000
#define TC_INTERVAL_MS              5000
#define ETT_INTERVAL_MS             60000

#define FISHEYE                     0

// holding times
// determines max. number of missed HELLO packets before neighbor is discarded
#define LINK_HOLD_TIME_COEFF        7
// determines max. number of missed TCs packets the corresponding information is discarded
#if FISHEYE
#define TC_HOLD_TIME_COEFF          (8 * 20) ///< distant nodes receive 8x fewer TCs when using fisheye
#else
#define TC_HOLD_TIME_COEFF          20
#endif 
#define BRCLOG_HOLD_TIME            3

// link types
enum link_type {
    UNSPEC_LINK = 0,
    ASYM_LINK   = 1,
    SYM_LINK    = 2,
    LOST_LINK   = 3
};

#define LINK_MASK                   3

enum neighbor_types {
    NOT_NEIGH = 0,
    SYM_NEIGH,
    MPR_NEIGH
};

enum olsr_willingness {
    WILL_NEVER      = 0,
    WILL_LOW        = 1,
    WILL_DEFAULT    = 3,
    WILL_HIGH       = 6,
    WILL_ALLWAYS    = 7
};

// build intervals
#define RT_INTERVAL_MS              1000

// link quality
#define WINDOW_SIZE                 50
#define MPR_QUALITY_THRESHOLD       75

//ETT
#define ETT_START                   0
#define ETT_STOP                    1
#define ETT_MSG                     2
#define ETT_START_SIZE              128
#define ETT_STOP_SIZE               1024

// Size of the sliding window for the ett calculation
#define ETT_SW_SIZE 10

#define C_INV_COEFF                 64

#define HELLO_SIZE                  128
#define TC_SIZE                     128

typedef enum olsr_metric {
    RC_METRIC_PLR = 1,
    RC_METRIC_HC,
    RC_METRIC_ETX,
    RC_METRIC_ETX_ADD,
    RC_METRIC_ETT
} olsr_metric_t;

extern uint16_t                     hello_interval_ms;
extern uint16_t                     tc_interval_ms;
extern uint16_t                     ett_interval;
extern uint16_t                     rt_interval_ms;
extern uint16_t                     max_missed_tc;
extern uint16_t                     max_missed_hello;
extern uint8_t                      willingness;
extern olsr_metric_t                rc_metric;
extern uint16_t                     hello_size;
extern uint16_t                     tc_size;
extern dessert_periodic_t*          periodic_send_hello;
extern dessert_periodic_t*          periodic_send_tc;
extern dessert_periodic_t*          periodic_send_ett;
extern dessert_periodic_t*          periodic_rt;
extern uint16_t                     window_size; ///< window size for calculation of PDR or ETX
extern bool                         fisheye; //limit ttl of TCs (Fisheye State Routing)

#endif
