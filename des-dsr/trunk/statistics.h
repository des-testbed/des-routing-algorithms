/******************************************************************************
 Copyright  2010 David Gutzmann, Freie Universitaet Berlin (FUB).
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

#ifndef STATISTICS_H_
#define STATISTICS_H_

typedef struct dsr_statistics_data {

    int rx_rreq;
    int tx_rreq;

    int rx_repl;
    int prx_repl;
    int tx_repl;

    int rx_source;
    int prx_source;
    int tx_source;

    int rx_ack;
    int prx_ack;
    int tx_ack;

    int rx_ackreq;
    int prx_ackreq;
    int tx_ackreq;

    int rx_etx;
    int prx_etx;
    int tx_etx;

    int rx_uetx;
    int prx_uetx;
    int tx_uetx;

    int rx_rerr;
    int prx_rerr;
    int tx_rerr;

    int rx_data;
    int rx_data_bytes;
    int prx_data;
    int prx_data_bytes;
    int tx_data;
    int tx_data_bytes;

    int rx_routing;
    int rx_routing_bytes;
    int prx_routing;
    int prx_routing_bytes;
    int tx_routing;
    int tx_routing_bytes;

} dsr_statistics_data_t ;

typedef struct __attribute__((__packed__)) dsr_statistics_link_lookup_key {
    uint8_t local[ETHER_ADDR_LEN];   /* key: part 1 */
    uint8_t remote[ETHER_ADDR_LEN]; /* key: part 2 */
} dsr_statistics_link_lookup_key_t;

typedef struct __attribute__((__packed__)) dsr_statistics_link {
    uint8_t local[ETHER_ADDR_LEN];
    uint8_t remote[ETHER_ADDR_LEN];
    dsr_statistics_data_t data;
    UT_hash_handle hh;
} dsr_statistics_link_t;

typedef struct dsr_statistics {
    int emit_rreq;
    int emit_repl;
    int emit_source;
    int emit_ack;
    int emit_ackreq;
    int emit_etx;
    int emit_uetx;
    int emit_rerr;
    int emit_data;
    int emit_data_bytes;
    int emit_routing;
    int emit_routing_bytes;
    dsr_statistics_data_t total;
    dsr_statistics_link_t* nodes;
} dsr_statistics_t;

int statistics_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

inline void dsr_statistics_emit_msg(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], dessert_msg_t* msg);
inline void dsr_statistics_tx_msg(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], dessert_msg_t* msg);
int dessert_cli_cmd_showstatistics(struct cli_def* cli, char* command, char* argv[], int argc);

#endif /* STATISTICS_H_ */
