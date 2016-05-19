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

#include "batman_database.h"
#include "rl_seq_t/rl_seq.h"
#include "../config.h"
#include <pthread.h>

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void batman_db_rlock() {
    pthread_rwlock_rdlock(&rwlock);
}

void batman_db_wlock() {
    pthread_rwlock_wrlock(&rwlock);
}

void batman_db_unlock() {
    pthread_rwlock_unlock(&rwlock);
}

int batman_db_init() {
    batman_db_irt_init();
    batman_db_brt_init();
    batman_db_rt_init();
    batman_db_brct_init();
    batman_db_nt_init();
    rl_table_init();
    return true;
}

int batman_db_cleanup() {
    if(batman_db_irt_cleanup() == false) {
        return false;
    }

    if(batman_db_rt_cleanup() == false) {
        return false;
    }

    return true;
}

int batman_db_change_pt(time_t purge_timeout) {
    batman_db_irt_change_pt(purge_timeout);
    batman_db_rt_change_pt(purge_timeout);
    return true;
}
