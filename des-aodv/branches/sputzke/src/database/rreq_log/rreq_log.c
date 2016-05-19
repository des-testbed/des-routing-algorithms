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

#include "../../config.h"
#include "../timeslot.h"

timeslot_t* rreq_log_ts = NULL;
uint32_t rreq_count = 0;
void* rreq_pseudo_pointer = 0;

void rreq_decrement_counter(struct timeval* timestamp, void* src_object, void* object) {
    rreq_count--;
}

int aodv_db_rl_init() {
    // 1 sek timeout since we are interested for number of sent RREQ in last 1 sec
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    return timeslot_create(&rreq_log_ts, &timeout, NULL, rreq_decrement_counter);
}

int aodv_db_rl_cleanup(struct timeval* timestamp) {
    return timeslot_purgeobjects(rreq_log_ts, timestamp);
}

void aodv_db_rl_putrreq(struct timeval* timestamp) {
    if(timeslot_addobject(rreq_log_ts, timestamp, rreq_pseudo_pointer++) == true) {
        rreq_count++;
    }
}

void aodv_db_rl_getrreqcount(struct timeval* timestamp, uint32_t* count_out) {
    aodv_db_rl_cleanup(timestamp);
    *count_out = rreq_count;
}
