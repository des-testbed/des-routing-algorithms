/******************************************************************************
 * Copyright 2009, Freie Universitaet Berlin (FUB). All rights reserved.

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

#ifndef SLIDING_WINDOW
#define SLIDING_WINDOW

#include <stdlib.h>
#include <stdint.h>

typedef struct olsr_sw_element {
    struct olsr_sw_element*	prev;
    struct olsr_sw_element*	next;
    uint16_t				seq_num;
} olsr_sw_element_t;

typedef struct olsr_sw {
    olsr_sw_element_t*	head;
    olsr_sw_element_t*	tail;
    uint8_t			size;
    uint8_t			max_size;
} olsr_sw_t;

int olsr_sw_create(olsr_sw_t** sw_out, uint8_t max_window_size);

int olsr_sw_destroy(olsr_sw_t* sw);

int olsr_sw_addsn(olsr_sw_t* sw, uint16_t seq_num);

uint8_t olsr_sw_getquality(olsr_sw_t* sw);

#endif
