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

#ifndef BATMAN_DATABASE
#define BATMAN_DATABASE

#include "inv_routing_table/batman_invrt.h"
#include "routing_table/batman_rt.h"
#include "broadcast_log/broadcast_log.h"
#include "neighbor_table/batman_nt.h"
#include "backup_routing_table/batman_brt.h"


/** Make read lock over database to avoid corrupt read/write */
void batman_db_rlock();

/** Make write lock over database to avoid currutp read/write */
void batman_db_wlock();

/** Unlock previos locks for this thread */
void batman_db_unlock();

/** initialize all tables of routing database */
int batman_db_init();

/** cleanup (pudge) old etnrys from all database tables */
int batman_db_cleanup();

/** change pudge timeout for all tables */
int batman_db_change_pt(time_t purge_timeout);


// ------------------- reporting -----------------------------------------------

/** get NextHop-List table for given destination */
int batman_db_view_nexthoptable(uint8_t ether_dest_addr[ETH_ALEN], const dessert_meshif_t* local_iface,
                                char** str_out);

/** get routing table as string */
int batman_db_view_routingtable(char** str_out);

#endif
