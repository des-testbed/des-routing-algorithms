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

#ifndef SENDBUFFER_H_
#define SENDBUFFER_H_

#include "dsr.h"

typedef struct dsr_sendbuffer dsr_sendbuffer_t;
struct dsr_sendbuffer {
    uint8_t dest[ETHER_ADDR_LEN];
    struct timeval timeout;
    dessert_msg_t* msg;
    dsr_sendbuffer_t* next;
    dsr_sendbuffer_t* prev;
};


#endif /* SENDBUFFER_H_ */

inline void dsr_sendbuffer_add(const uint8_t dest[ETHER_ADDR_LEN], dessert_msg_t* msg);

inline void dsr_sendbuffer_send_msgs_to(const uint8_t dest[ETHER_ADDR_LEN]);

dessert_per_result_t cleanup_sendbuffer(void* data, struct timeval* scheduled, struct timeval* interval);
