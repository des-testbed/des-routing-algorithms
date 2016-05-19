/******************************************************************************
 Copyright 2010, David Gutzmann, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by David Gutzmann
 at Freie Universitaet Berlin (http://www.fu-berlin.de/),
 Computer Systems and Telematics / Distributed, Embedded Systems (DES) group
 (http://cst.mi.fu-berlin.de/, http://www.des-testbed.net/)
 ------------------------------------------------------------------------------
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program. If not, see http://www.gnu.org/licenses/ .
 ------------------------------------------------------------------------------
 For further information and questions please use the web site
 http://www.des-testbed.net/
 ------------------------------------------------------------------------------

 ******************************************************************************/

#include "dsr.h"

pthread_rwlock_t _dsr_sendbuffer_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _SB_READLOCK pthread_rwlock_rdlock(&_dsr_sendbuffer_rwlock)
#define _SB_WRITELOCK pthread_rwlock_wrlock(&_dsr_sendbuffer_rwlock)
#define _SB_UNLOCK pthread_rwlock_unlock(&_dsr_sendbuffer_rwlock)
#define _SAFE_RETURN(x) _SB_UNLOCK; return(x);

dsr_sendbuffer_t* sendbuffer = NULL;

/* local forward declarations */
static inline void _destroy_entry(dsr_sendbuffer_t* buffer);

inline void dsr_sendbuffer_add(const uint8_t dest[ETHER_ADDR_LEN], dessert_msg_t* msg) {
    dsr_sendbuffer_t* new = NULL;

    new = malloc(sizeof(dsr_sendbuffer_t));
    assert(new != NULL);

    _SB_WRITELOCK;

    ADDR_CPY(new->dest, dest);
    new->msg = msg;
    gettimeofday(&new->timeout, NULL);
    TIMEVAL_ADD_SAFE(&new->timeout, 0, dsr_conf_get_sendbuffer_timeout());
    dessert_debug("SENDBUFFER: adding msg to [" MAC "] timeout in %usecs", EXPLODE_ARRAY6(dest), dsr_conf_get_sendbuffer_timeout() / 1000000);

    DL_APPEND(sendbuffer, new);

    _SB_UNLOCK;
}

inline void dsr_sendbuffer_send_msgs_to(const uint8_t dest[ETHER_ADDR_LEN]) {
    dsr_path_t path;

#if (LOAD_BALANCING == 0)
    int res = dsr_routecache_get_first(dest, &path);
#elif (LOAD_BALANCING == 1)
    int res = dsr_routecache_get_next_round_robin(dest, &path);
#endif
    assert(res == DSR_ROUTECACHE_SUCCESS);

    if(res == DSR_ROUTECACHE_SUCCESS) {
        dsr_sendbuffer_t* iter_buffer = NULL;
        dsr_sendbuffer_t* tbd_buffer = NULL;

        __suseconds_t delay = 1000; /* 10ms */
        int delay_factor = 1;

        _SB_WRITELOCK;
        iter_buffer = sendbuffer;

        while(iter_buffer) {
            if(ADDR_CMP(dest, iter_buffer->dest) == 0) {
                dessert_debug("SENDBUFFER: msg for dest[" MAC "] found.", EXPLODE_ARRAY6(iter_buffer->dest));
                tbd_buffer = iter_buffer;
                DL_DELETE(sendbuffer, iter_buffer);
                iter_buffer = iter_buffer->next;
                /* delay every msg by an increasing factor */
                dsr_msg_send_via_path_delay(tbd_buffer->msg, &path, delay_factor * delay);
                delay_factor++;
                _destroy_entry(tbd_buffer);
            }
            else {
                iter_buffer = iter_buffer->next;
            }
        }

        _SB_UNLOCK;
    }
}

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t cleanup_sendbuffer(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_sendbuffer_t* iter_buffer = NULL;
    dsr_sendbuffer_t* tbd_buffer = NULL;

    struct timeval now;

    _SB_WRITELOCK;

    gettimeofday(&now, NULL);
    iter_buffer = sendbuffer;
    int i = 0;

    while(iter_buffer) {
        if(TIMEVAL_COMPARE(&now, &iter_buffer->timeout) >= 0) {
            //			dessert_debug("\n"
            //					"now.tvsec     [%li]\n"
            //					"timeout.tvsec [%lu]", now.tv_sec, iter_buffer->timeout.tv_sec);
            tbd_buffer = iter_buffer;
            DL_DELETE(sendbuffer, iter_buffer);
            iter_buffer = iter_buffer->next;
            dessert_debug("SENDBUFER: Removing msg to [" MAC "], no route in %usecs", EXPLODE_ARRAY6(tbd_buffer->dest), dsr_conf_get_sendbuffer_timeout() / 1000000);
            _destroy_entry(tbd_buffer);
        }
        else {
            iter_buffer = iter_buffer->next;
            i++;
        }
    }

    _SB_UNLOCK;

    return DESSERT_PER_KEEP;
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/

static inline void _destroy_entry(dsr_sendbuffer_t* buffer) {
    dessert_msg_destroy(buffer->msg);
    free(buffer);
}















