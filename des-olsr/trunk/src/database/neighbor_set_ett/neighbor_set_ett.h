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

#ifndef OLSR_NEIGHBOR_SET_ETT
#define OLSR_NEIGHBOR_SET_ETT

#include <uthash.h>
#include <dessert.h>
#include "../../config.h"
#include "../../android.h"

typedef struct olsr_db_neighbors_ett_entry {
    uint8_t		neighbor_main_addr[ETH_ALEN];		//key
    //timeval when the first packet (ETT_START) was received
    struct timeval	timeval_recv;
    //Sliding windows of times from this neighbor
    uint32_t 		time_int_sw[ETT_SW_SIZE];
    //pointer to the times sliding windows
    uint8_t		time_sw_ptr;
    UT_hash_handle	hh;
} olsr_db_neighbors_ett_entry_t;

/*
 * Gets the minimum value of time_int_sw from the data set.
 */
uint32_t get_min_time_from_neighbor(uint8_t neighbor_main_addr[ETH_ALEN]);

/*
 * Inserts a timeval into timeval_recv when the ETT_START packet is received.
 */
int process_ett_start_time(uint8_t neighbor_main_addr[ETH_ALEN], struct timeval* ett_start_time);

/*
 * When the ETT_STOP packet is received this method calculates the difference to the
 * timeval when the ETT_START packet was received and returns it.
 */
uint32_t process_ett_stop_time(uint8_t neighbor_main_addr[ETH_ALEN], struct timeval* ett_stop_time);

/*
 * When an ETT_MSG packet is received the contained time is saved into the time_int_sw.
 */
int process_ett_msg(uint8_t neighbor_main_addr[ETH_ALEN], uint32_t recved_time);

/*
 * Reporting
 */
int olsr_db_ett_report(char** str_out);

#endif
