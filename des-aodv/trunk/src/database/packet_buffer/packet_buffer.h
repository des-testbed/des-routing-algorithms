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

#ifndef PACKET_BUFFER
#define PACKET_BUFFER

#include <dessert.h>

#ifdef ANDROID
#include <linux/if_ether.h>
#endif


int pb_init();

void pb_push_packet(mac_addr dhost_ether, dessert_msg_t* msg, struct timeval* timestamp);

dessert_msg_t* pb_pop_packet(mac_addr dhost_ether);

void pb_drop_packets(mac_addr dhost_ether);

int pb_cleanup(struct timeval* timestamp);

void pb_report(char** str_out);

#endif
