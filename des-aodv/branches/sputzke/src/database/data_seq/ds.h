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

#include <dessert.h>
#include <uthash.h>
#include "../timeslot.h"
#include "../../config.h"
#include "../../helper.h"

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

/** initialize neighbor table */
int db_ds_init();

int aodv_db_ds_capt_data_seq(mac_addr src_addr, uint16_t data_seq_num, uint8_t hop_count, struct timeval* timestamp);

int db_ds_cleanup(struct timeval* timestamp);

void ds_report(char** str_out);

void db_ds_on_neigbor_timeout(struct timeval* timestamp, void* src_object, void* object);

