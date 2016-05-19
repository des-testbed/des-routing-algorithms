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

#ifndef OLSR_DATABASE
#define OLSR_DATABASE

#include <stdlib.h>
#include "link_set/link_set.h"
#include "neighbor_set/neighbor_set.h"
#include "2hop_neighbor_set/2hop_neighbor_set.h"
#include "duplicate_table/duplicate_table.h"
#include "local_iface_set/liface_set.h"
#include "routing_calculation/route_calculation.h"
#include "topology_set/topology_set.h"
#include "routing_table/routing_table.h"
#include "broadcast_log/broadcast_log.h"

/** Make read lock over database to avoid corrupt read/write */
inline void olsr_db_rlock();

/** Make write lock over database to avoid currupt read/write */
inline void olsr_db_wlock();

/** Unlock previos locks for this thread */
inline void olsr_db_unlock();

/** initialize all tables of routing database */
int olsr_db_init();

/** cleanup (purge) old entrys from all database tables */
int olsr_db_cleanup(struct timeval* timestamp);

// ----------------------------------- reporting -------------------------------------------------------------------------

int olsr_db_view_routing_table(char** str_out);
#endif
