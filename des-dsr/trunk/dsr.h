/******************************************************************************
 Copyright 2009, 2010 David Gutzmann, Freie Universitaet Berlin (FUB).
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

#ifndef DSR_H_
#define DSR_H_

#define TOO_OLD_LIBC 1

#define HC  1
#define ETX 2
#define MDSR_PROTOKOLL_1           1
#define SMR                        2
#define BACKUPPATH_VARIANT_1       3
#define BACKUPPATH_VARIANT_2       4
#define DSR                        5
#define ETXDSR                     6

#if (DAEMON == 0)
#    define DAEMON_NAME "des-dsr-hc"
#    define DESSERT_PROTO_STRING "DSRH"
#    define PROTOCOL DSR
#    define METRIC HC
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 1
#elif (DAEMON == 1)
#    define DAEMON_NAME "des-dsr"
#    define DESSERT_PROTO_STRING "DSR1"
#    define PROTOCOL DSR
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 1
#elif (DAEMON == 2)
#    define DAEMON_NAME "des-dsr-etx"
#    define DESSERT_PROTO_STRING "DSR2"
#    define PROTOCOL ETXDSR
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 1
#elif (DAEMON == 3)
#    define DAEMON_NAME "des-dsr-linkcache"
#    define DESSERT_PROTO_STRING "DSR3"
#    define PROTOCOL DSR
#    define METRIC ETX
#    define LINKCACHE 1
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 1
#elif (DAEMON == 4)
#    define DAEMON_NAME "des-dsr-linkcache-etx"
#    define DESSERT_PROTO_STRING "DSR4"
#    define PROTOCOL ETXDSR
#    define METRIC ETX
#    define LINKCACHE 1
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 1
#elif (DAEMON == 5)
#    define DAEMON_NAME "des-dsr-mdsr"
#    define DESSERT_PROTO_STRING "DSR5"
#    define PROTOCOL MDSR_PROTOKOLL_1
#    define METRIC ETX
#    define LINKCACHE 0
#    define PROTOCOL  MDSR_PROTOKOLL_1
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 2
#elif (DAEMON == 6)
#    define DAEMON_NAME "des-dsr-smr"
#    define DESSERT_PROTO_STRING "DSR6"
#    define PROTOCOL SMR
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 1
#    define PROTOCOL  SMR
#elif (DAEMON == 7)
#    define DAEMON_NAME "des-dsr-backuppath1"
#    define DESSERT_PROTO_STRING "DSR7"
#    define PROTOCOL BACKUPPATH_VARIANT_1
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define PROTOCOL  BACKUPPATH_VARIANT_1
#elif (DAEMON == 8)
#    define DAEMON_NAME "des-dsr-backuppath2"
#    define DESSERT_PROTO_STRING "DSR8"
#    define PROTOCOL BACKUPPATH_VARIANT_2
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define PROTOCOL  BACKUPPATH_VARIANT_2
#elif (DAEMON == 9)
#    define DAEMON_NAME "des-dsr-etx-backup"
#    define DESSERT_PROTO_STRING "DSR9"
#    define PROTOCOL ETXDSR
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 2
#elif (DAEMON == 10)
#    define DAEMON_NAME "des-dsr-etx-lb"
#    define DESSERT_PROTO_STRING "DSR0"
#    define PROTOCOL ETXDSR
#    define METRIC ETX
#    define LINKCACHE 0
#    define LOAD_BALANCING 1
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 2
#else
#    define DAEMON_NAME "des-dsr-mdsr"
#    define DESSERT_PROTO_STRING "DSR5"
#    define PROTOCOL MDSR_PROTOKOLL_1
#    define METRIC ETX
#    define LINKCACHE 0
#    define PROTOCOL  MDSR_PROTOKOLL_1
#    define LOAD_BALANCING 0
#    define DSR_CONFVAR_ROUTECACHE_KEEP_PATHS 2
#endif


#ifndef LINKCACHE
#   define LINKCACHE 0
#endif

#ifndef METRIC
#   define METRIC HC
#endif


#define DSR_CONFVAR_RREQTABLE_REQUESTTABLEIDS                 32 /* rfc4728 p95 states 16*/

#define DSR_CONFVAR_BLACKLIST_CLEANUP_INTERVAL_SECS           10
#define DSR_CONFVAR_BLACKLIST_REVERT_TO_QUESTIONABLE_SECS     60
#define DSR_CONFVAR_BLACKLIST_EXPIRATION_SECS                120

#define DSR_CONFVAR_ROUTEMAINTENANCE_PASSIVE_ACK               0
#define DSR_CONFVAR_ROUTEMAINTENANCE_NETWORK_ACK               0
#define DSR_CONFVAR_RETRANSMISSION_COUNT                       0 /* cli: set retransmission_count     */
#define DSR_CONFVAR_RETRANSMISSION_TIMEOUT                 50000 /* cli: set retransmission_timeout   */

#define DSR_CONFVAR_SENDBUFFER_CLEANUP_INTERVAL_SECS           1
#define DSR_CONFVAR_SENDBUFFER_TIMEOUT                  20000000 /* cli: set sendbuffer_timeout */

#define DSR_CONFVAR_RREQTABLE_CLEANUP_INTERVAL_SECS           60
#define DSR_CONFVAR_ROUTEDISCOVERY_TIMEOUT               1000000 /* cli: set routediscovery_timeout */
#define DSR_CONFVAR_ROUTEDISCOVERY_MAXIMUM_RETRIES             3
#define DSR_CONFVAR_ROUTEDISCOVERY_EXPANDING_RING_SEARCH       0

#if (PROTOCOL == SMR || PROTOCOL == BACKUPPATH_VARIANT_1 || PROTOCOL == BACKUPPATH_VARIANT_2)
#define DSR_CONFVAR_SMR_RREQCACHE_NEIGHBOR_LIST_MAX_LEN       30
#define DSR_CONFVAR_SMR_RREQCACHE_REPLY_TIMEOUT_MSECS    1000000
#endif

#if (METRIC == ETX)
#define DSR_CONFVAR_ETX_PROBE_RATE_SECS                        1
#define DSR_CONFVAR_ETX_WINDOW_SIZE_SECS                      16
#define DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS                (2 * DSR_CONFVAR_ETX_WINDOW_SIZE_SECS)
#endif

#if (LINKCACHE == 1)
#define DSR_CONFVAR_DIJKSTRA_SECS                              4
#endif

#include <stdlib.h>
#include <unistd.h>

#include <dessert.h>

#include <utlist.h>
#include <uthash.h>

#include <netinet/in.h>
#include <pthread.h>
#include <inttypes.h>
#ifndef ANDROID
#include <printf.h>
#endif
#include <math.h>

#include "build.h"
#include "macros.h"
#include "conf.h"
#include "statistics.h"
#include "extensions.h"

#if (METRIC == ETX)
#include "etx.h"
#endif

#include "helper.h"
#include "blacklist.h"

#if (LINKCACHE == 1)
#include "linkcache.h"
#endif

#include "sendbuffer.h"
#include "rreqtable.h"
#include "routecache.h"
#include "maintenance_buffer.h"

#include "alloc_cache.h"


#endif /* DSR_H_ */
