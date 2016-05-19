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
#ifndef CONF_H_
#define CONF_H_

#include "dsr.h"

typedef struct dsr_conf {

    int routemaintenance_passive_ack;
    int routemaintenance_network_ack;
    int retransmission_count;
    __suseconds_t retransmission_timeout;
    dessert_periodic_t* retransmission_timeout_periodic;

    __suseconds_t sendbuffer_timeout;

    __suseconds_t routediscovery_timeout;
    dessert_periodic_t* routediscovery_timeout_periodic;
    int routediscovery_maximum_retries;
    int routediscovery_expanding_ring_search;


} dsr_conf_t;

inline void dsr_conf_initialize(void);
void dsr_conf_register_cli_callbacks(struct cli_command* cli_cfg_set, struct cli_command* cli_exec_info);


inline int dsr_conf_get_routemaintenance_passive_ack(void);
inline int dsr_conf_get_routemaintenance_network_ack(void);
inline int dsr_conf_get_retransmission_count(void);
inline __suseconds_t dsr_conf_get_retransmission_timeout(void);

inline __suseconds_t dsr_conf_get_sendbuffer_timeout(void);

inline __suseconds_t dsr_conf_get_routediscovery_timeout(void);
inline int dsr_conf_get_routediscovery_maximum_retries(void);

inline int dsr_conf_get_routediscovery_expanding_ring_search(void);

#endif /* CONF_H_ */
