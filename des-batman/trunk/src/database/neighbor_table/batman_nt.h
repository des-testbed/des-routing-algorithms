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

#ifndef BATMAN_NT
#define BATMAN_NT

#include <linux/if_ether.h>
#include <dessert.h>

/** initialize neighbor table */
int batman_db_nt_init();

/**
 * Take a record that the given neighbor seems to have
 * 1 hop bidirectional connection with this iface
 */
int batman_db_nt_cap2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* local_iface);

/**
 * Check whether given iface pair is in 1 hop bidirectional connection
 */
int batman_db_nt_check2Dneigh(uint8_t ether_neighbor_addr[ETH_ALEN], const dessert_meshif_t* local_iface);

/** change pudge_timeout for neighbor table */
int batman_db_nt_change_pt(time_t pudge_timeout);

#endif
