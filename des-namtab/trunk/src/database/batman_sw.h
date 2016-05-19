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

#ifndef BATMAN_SW
#define BATMAN_SW

#include <stdlib.h>
#include <stdint.h>

typedef struct batman_sw_element {
    struct batman_sw_element*	prev;
    struct batman_sw_element*	next;
    uint16_t					seq_num;
} batman_sw_element_t;

typedef struct batman_sw {
    struct batman_sw_element*	head;
    struct batman_sw_element*	tail;
    uint16_t					size;
    uint8_t					window_size;
} batman_sw_t;

/** Create sliding window */
int batman_sw_create(batman_sw_t** sw, uint8_t ws);

/** Remove all Seq_Num elements and destroy sliding window */
int batman_sw_destroy(batman_sw_t* sw);

/** Add sequence number to sliding window.
 * Drops all values out of  {max_value - WINDOW_SIZE + 1, max_value} range */
int batman_sw_addsn(batman_sw_t* sw, uint16_t value);

/**Drops all sequence numbers out of  {value - WINDOW_SIZE + 1, value} range*/
int batman_sw_dropsn(batman_sw_t* sw, uint16_t value);

#endif
