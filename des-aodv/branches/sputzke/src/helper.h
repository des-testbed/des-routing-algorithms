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

#ifndef HELPER
#define HELPER

#ifdef ANDROID
#include <sys/time.h>
#endif

#include <linux/if_ether.h>
#include <stdlib.h>
#include <stdint.h>
#include "config.h"

/**
 * Compares two unsigned integers taking overflow into account
 * returns 0 if i == j
 * returns a positive integer if i is "ahead" of j
 * returns a negative integer if i is "behind" j
 */
int hf_comp_u32(uint32_t i, uint32_t j);

/**
 * Compares two metric values according to the current metric_type
 * returns 0 if i == j
 * returns a positive integer if i is better than j
 * returns a negative integer if i is worse than j
 */
int hf_comp_metric(metric_t i, metric_t j);

static inline struct timeval hf_tv_add_ms(const struct timeval tv, uintmax_t ms) __attribute__ ((__unused__));
static inline struct timeval hf_tv_add_ms(const struct timeval tv, uintmax_t ms) {
    struct timeval result = tv;
    dessert_timevaladd(&result, ms / 1000, (ms % 1000) * 1000);
    return result;
}

static inline uint64_t hf_mac_addr_to_uint64(const mac_addr addr) __attribute__ ((__unused__));
static inline uint64_t hf_mac_addr_to_uint64(const mac_addr addr) {
    uint64_t result = addr[5];
    int i;
    for(i = 4; i >= 0; --i) {
        result <<= 8;
        result += addr[i];
    }
    return result;
}

/******************************************************************************/

/** Return value between 1 and 5 for rssi values */
uint8_t hf_rssi2interval(int8_t rssi);

#endif
