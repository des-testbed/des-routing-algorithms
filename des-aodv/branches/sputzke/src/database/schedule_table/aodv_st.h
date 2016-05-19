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

#ifndef AODV_ST
#define AODV_ST

#include <time.h>
#include <dessert.h>

int aodv_db_sc_addschedule(struct timeval* execute_ts, mac_addr ether_addr, uint8_t type, void* param);

int aodv_db_sc_popschedule(struct timeval* timestamp, mac_addr ether_addr_out, uint8_t* type, void** param);

int aodv_db_sc_schedule_exists(mac_addr ether_addr, uint8_t type);

int aodv_db_sc_dropschedule(mac_addr ether_addr, uint8_t type);

#endif
