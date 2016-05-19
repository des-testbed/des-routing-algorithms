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

#include "olsr_database.h"
#include "../config.h"
#include <pthread.h>
#include "rl_seq_t/rl_seq.h"

pthread_rwlock_t db_rwlock = PTHREAD_RWLOCK_INITIALIZER;

inline void olsr_db_rlock() {
    pthread_rwlock_rdlock(&db_rwlock);
}

inline void olsr_db_wlock() {
    pthread_rwlock_wrlock(&db_rwlock);
}

inline void olsr_db_unlock() {
    pthread_rwlock_unlock(&db_rwlock);
}

int olsr_db_init() {
    if(olsr_db_dt_init() != true) {
        return false;
    }

    if(olsr_db_ns_init() != true) {
        return false;
    }

    if(olsr_db_tc_init() != true) {
        return false;
    }

    if(olsr_db_brct_init() != true) {
        return false;
    }

    if(rl_table_init() != true) {
        return false;
    }

    return true;
}

int olsr_db_cleanup(struct timeval* timestamp) {
    return true;
}

// --------------------------------------- reporting ---------------------------------------------------------------

int olsr_db_view_routing_table(char** str_out) {
    //int result =  aodv_db_rt_report(str_out);
    return false;
}
