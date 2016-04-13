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

#ifndef TIMESLOT
#define TIMESLOT

#include <stdlib.h>
#include <uthash.h>

typedef void pudge_object_t(time_t timestamp, void* object);

typedef struct timeslot_element {
    struct timeslot_element*	prev;
    struct timeslot_element*	next;
    time_t			 			timestamp;
    void*						object; // key
    UT_hash_handle 				hh;
} timeslot_element_t;

typedef struct timeslot {
    struct timeslot_element*	head;
    struct timeslot_element*	tail;
    uint32_t					size;
    time_t						purge_timeout;
    pudge_object_t*				object_pudger;
    struct timeslot_element*	elements_hash;
} timeslot_t;

/** Create time-slot */
int timeslot_create(timeslot_t** ts, time_t pudge_timeout, pudge_object_t* object_pudger);

/** Remove all time-slot elements and destroy time-slot */
int timeslot_destroy(timeslot_t* ts);

/** Change pudge timeout */
int timeslot_change_pt(timeslot_t* ts, time_t pudge_timeout);

/** Add object with timestamp number time-slot.
 * Pudges all objects older than timestamp - pudge_timeout from time-slot */
int timeslot_addobject(timeslot_t* ts, time_t timestamp, void* object);

/** delete an object from timeslot */
int timeslot_deleteobject(timeslot_t* ts, void* object);

/**Pudges all objects older than timestamp - pudge_timeout from time-slot*/
int timeslot_purgeobjects(timeslot_t* sw, time_t timestamp);

#endif
