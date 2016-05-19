/******************************************************************************
 Copyright 2009, David Gutzmann, Freie Universitaet Berlin (FUB).
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

#ifndef ROUTECACHE_H_
#define ROUTECACHE_H_

#include "dsr.h"

#define DSR_ROUTECACHE_SUCCESS                             0
#define DSR_ROUTECACHE_ERROR_MEMORY_ALLOCATION            -1
#define DSR_ROUTECACHE_ERROR_NO_PATH_TO_DESTINATION       -2
#define DSR_ROUTECACHE_ERROR_LINKCACHE_NOT_INITIALIZED    -3

typedef struct __attribute__((__packed__)) dsr_routecache {

    uint8_t address[ETHER_ADDR_LEN];

    size_t route_count;

    dsr_path_t* paths;

    UT_hash_handle hh;

} dsr_routecache_t;

extern dsr_routecache_t* dsr_routecache;

int dsr_routecache_get_first(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path);
int dsr_routecache_get_next_round_robin(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path);
void dsr_routecache_process_link_error(const uint8_t u[ETHER_ADDR_LEN], const uint8_t v[ETHER_ADDR_LEN]);
void dsr_routecache_add_path(const uint8_t dest[ETHER_ADDR_LEN], dsr_path_t* path);
void dsr_routecache_print_routecache_to_debug();

#endif /* ROUTECACHE_H_ */
