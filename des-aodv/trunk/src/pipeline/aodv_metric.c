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

#include "../config.h"
#include "../helper.h"
#include "../database/aodv_database.h"
#include "aodv_pipeline.h"

int aodv_metric_do(metric_t* metric, mac_addr last_hop, dessert_meshif_t* iface, struct timeval* timestamp) {

    switch(metric_type) {
        case AODV_METRIC_HOP_COUNT: {
            (*metric)++;
            break;
        }
#ifndef ANDROID
        case AODV_METRIC_RSSI: {
            struct avg_node_result sample = dessert_rssi_avg(last_hop, iface);
            metric_t interval = hf_rssi2interval(sample.avg_rssi);
            dessert_trace("incoming rssi_metric=%" AODV_PRI_METRIC ", add %" PRIu8 " (rssi=%" PRId8 ") for the last hop " MAC, (*metric), interval, sample.avg_rssi, EXPLODE_ARRAY6(last_hop));
            *metric += interval;
            break;
        }
#endif
        case AODV_METRIC_ETX_ADD: {
            metric_t link_etx_add = AODV_MAX_METRIC;
            if(aodv_db_pdr_get_etx_add(last_hop, &link_etx_add, timestamp)) {
                dessert_debug("Old metricval %" AODV_PRI_METRIC " ETX_ADD rcvd =%" PRIu16 " for this hop " MAC, (*metric), link_etx_add, EXPLODE_ARRAY6(last_hop));
            }
            else {
                dessert_debug("Old metricval %" AODV_PRI_METRIC " ETX_ADD for hop " MAC " failed", *metric, EXPLODE_ARRAY6(last_hop));
            }
            /**prevent overflow*/
            if(AODV_MAX_METRIC - link_etx_add > *metric) {
                *metric += link_etx_add;
            }
            else {
                *metric = AODV_MAX_METRIC;
            }
            dessert_debug("New metric value =%" AODV_PRI_METRIC " for hop " MAC, *metric, EXPLODE_ARRAY6(last_hop));
            break;
        }
        case AODV_METRIC_ETX_MUL: {
            metric_t link_etx_mul = 0;
            if(aodv_db_pdr_get_etx_mul(last_hop, &link_etx_mul, timestamp) == true) {
                dessert_debug("Old metricval %" AODV_PRI_METRIC " ETX_MUL rcvd =%" PRIu16 " for this hop " MAC, (*metric), link_etx_mul, EXPLODE_ARRAY6(last_hop));
                uintmax_t result = (*metric) * (uintmax_t)link_etx_mul;
                result /= (AODV_MAX_METRIC/32);
                (*metric) = (metric_t) result;
            }
            else {
                dessert_debug("Old metricval %" AODV_PRI_METRIC " ETX_MUL for hop " MAC " failed", (*metric), EXPLODE_ARRAY6(last_hop));
                (*metric) = 0;
            }
            dessert_debug("New metric value =%" AODV_PRI_METRIC " for hop " MAC, (*metric), EXPLODE_ARRAY6(last_hop));
            break;
        }
        case AODV_METRIC_PDR: {
            uint16_t link_pdr = 0;
            if(aodv_db_pdr_get_pdr(last_hop, &link_pdr, timestamp) == true){
                dessert_debug("Old metricval %" AODV_PRI_METRIC " PDR rcvd =%" PRIu16 " for this hop " MAC, (*metric), link_pdr, EXPLODE_ARRAY6(last_hop));
                uint32_t result = (*metric) * (uint32_t)link_pdr;
                result = result / AODV_MAX_METRIC;
                (*metric) = (metric_t) result;
            }
            else {
                dessert_debug("Old metricval %" AODV_PRI_METRIC " PDR for hop " MAC " failed", (*metric), EXPLODE_ARRAY6(last_hop));
                (*metric) = 0;
            }
            dessert_debug("New metric value =%" AODV_PRI_METRIC " for hop " MAC, (*metric), EXPLODE_ARRAY6(last_hop));
            break;
        }
        case AODV_METRIC_RFC: {
            break;
        }
        default: {
            dessert_crit("unknown metric set -> using AODV_METRIC_RFC as fallback");
            return false;
        }
    }

    return true;
}


