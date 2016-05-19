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

#ifndef BATMAN_RT
#define BATMAN_RT

#include <dessert.h>
#include "../../config.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

int batman_db_rt_init();

int batman_db_rt_captureroute(uint8_t dest_addr[ETH_ALEN], const dessert_meshif_t* output_iface, uint8_t next_hop[ETH_ALEN]);

int batman_db_rt_getroute(uint8_t dest_addr[ETH_ALEN], const dessert_meshif_t** iface_out, uint8_t next_hop_out[ETH_ALEN]);

int batman_db_rt_getroute_arl(uint8_t dest_addr[ETH_ALEN],
                              const dessert_meshif_t** iface_out, uint8_t next_hop_out[ETH_ALEN],
                              uint8_t precursors_iface_list[OGM_PREC_LIST_SIZE* ETH_ALEN],
                              uint8_t* precursors_iface_count);

int batman_db_rt_cleanup();

int batman_db_rt_change_pt(time_t pudge_timeout);

int batman_db_rt_report(char** str_out);

#endif
