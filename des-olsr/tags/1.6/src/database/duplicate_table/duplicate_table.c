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

#include "duplicate_table.h"
#include "../../config.h"
#include "../timeslot.h"


typedef struct dtuple {
    uint8_t 		addr[ETH_ALEN];		// key
    uint16_t		seq_num;
    uint8_t		retransmitted;
    UT_hash_handle	hh;
} dtuple_t;

dtuple_t*		dupl_table = NULL;
timeslot_t*		dt_ts;

void purge_dtuple(struct timeval* timestamp, void* src_object, void* object) {
    dtuple_t* tuple = object;
    HASH_DEL(dupl_table, tuple);
    free(tuple);
}

int	olsr_db_dt_init() {
    return timeslot_create(&dt_ts, NULL, purge_dtuple);
}

int dtuple_create(dtuple_t** tuple_out, uint8_t ether_addr[ETH_ALEN], uint16_t seq_num, uint8_t retransmitted) {
    dtuple_t* tuple = malloc(sizeof(dtuple_t));

    if(tuple == NULL) {
        return false;
    }

    memcpy(tuple->addr, ether_addr, ETH_ALEN);
    tuple->seq_num = seq_num;
    tuple->retransmitted = retransmitted;
    *tuple_out = tuple;
    return true;
}

int olsr_db_dt_settuple(uint8_t ether_addr[ETH_ALEN], uint16_t seq_num, uint8_t retransmitted, struct timeval* purge_time) {
    dtuple_t* tuple;
    HASH_FIND(hh, dupl_table, ether_addr, ETH_ALEN, tuple);

    if(tuple == NULL) {
        if(dtuple_create(&tuple, ether_addr, seq_num, retransmitted) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, dupl_table, tuple->addr, ETH_ALEN, tuple);
    }

    tuple->seq_num = seq_num;
    tuple->retransmitted = retransmitted;

    timeslot_addobject(dt_ts, purge_time, tuple);
    return true;
}

int olsr_db_dt_gettuple(uint8_t ether_addr[ETH_ALEN], uint8_t* retransmitted_out) {
    timeslot_purgeobjects(dt_ts);

    dtuple_t* tuple;
    HASH_FIND(hh, dupl_table, ether_addr, ETH_ALEN, tuple);

    if(tuple == NULL) {
        return false;
    }

    *retransmitted_out = tuple->retransmitted;
    return true;
}
