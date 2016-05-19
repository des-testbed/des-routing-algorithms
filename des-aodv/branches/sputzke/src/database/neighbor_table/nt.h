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

#ifndef AODV_NT
#define AODV_NT

#include <dessert.h>

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

/** initialize neighbor table */
int db_nt_init();

int aodv_db_nt_neighbor_reset(uint32_t* count_out);

int db_nt_reset_rssi(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp);

int8_t db_nt_update_rssi(mac_addr ether_neighbor, dessert_meshif_t* iface, struct timeval* timestamp);
/**
 * Take a record that the given neighbor seems to be
 * 1 hop bidirectional neighbor
 */
int db_nt_cap2Dneigh(mac_addr ether_neighbor_addr, uint16_t hello_seq, dessert_meshif_t* iface, struct timeval* timestamp);

/**
 * Check whether given neighbor is 1 hop bidirectional neighbor
 */
int db_nt_check2Dneigh(mac_addr ether_neighbor_addr, dessert_meshif_t* iface, struct timeval* timestamp);

int db_nt_cleanup(struct timeval* timestamp);

void nt_report(char** str_out);

void db_nt_on_neigbor_timeout(struct timeval* timestamp, void* src_object, void* object);

#endif
