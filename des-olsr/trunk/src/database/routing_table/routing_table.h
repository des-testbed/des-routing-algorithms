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

#ifndef ROUTING_TABLE
#define ROUTING_TABLE

#include <linux/if_ether.h>
#include <stdlib.h>

void olsr_db_rt_destroy();

int olsr_db_rt_addroute(uint8_t dest_addr[ETH_ALEN], uint8_t next_hop[ETH_ALEN],
                        uint8_t precursor_addr[ETH_ALEN], uint8_t hop_count, float link_quality);

int olsr_db_rt_getnexthop(uint8_t dest_addr[ETH_ALEN], uint8_t next_hop_out[ETH_ALEN]);

int olsr_db_rt_report(char** str_out);

int olsr_db_rt_report_so(char** str_out);

#endif
