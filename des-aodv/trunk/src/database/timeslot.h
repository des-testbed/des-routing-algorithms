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

typedef void object_purger_t(struct timeval* purge_time, void* src_object, void* object);

typedef struct timeslot_element {
    struct timeslot_element*	prev;
    struct timeslot_element*	next;
    struct timeval* 			purge_time;
    void*						object; // key
    UT_hash_handle 				hh;
} timeslot_element_t;

typedef struct timeslot {
    struct timeslot_element*	head;
    struct timeslot_element*	tail;
    uint32_t					size;
    object_purger_t*			object_purger;
    struct timeval*				purge_timeout;
    void*						src_object;
    struct timeslot_element*	elements_hash;
} timeslot_t;

/** Create time-slot */
int timeslot_create(timeslot_t** ts_out, struct timeval* purge_timeout,
                    void* src_object, object_purger_t* object_purger);

/** Remove all time-slot elements and destroy time-slot */
int timeslot_destroy(timeslot_t* ts);

/** Add object with timestamp number time-slot.
 * Pudges all objects older than timestamp - pudge_timeout from time-slot */
int timeslot_addobject(timeslot_t* ts, struct timeval* timestamp, void* object);

/**
 * Add object with given lifetime to timeslot 
 * Should be used, if the lifetime differs from the timeslots default purge timeout
 */
int timeslot_addobject_varpurge(timeslot_t* ts, struct timeval* timestamp, void* object, struct timeval* not_def_lifetime);

/** delete an object from timeslot */
int timeslot_deleteobject(timeslot_t* ts, void* object);

/**Pudges all objects older with curr_time > purge_time from time-slot*/
int timeslot_purgeobjects(timeslot_t* sw, struct timeval* curr_time);

void timeslot_report(timeslot_t* ts, char** str_out);

#endif
