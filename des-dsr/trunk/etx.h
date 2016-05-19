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

#ifndef ETX_H_
#define ETX_H_

#include "dsr.h"

/******************************************************************************
 *
 * ETX - DSR ETX Extension
 *
 ******************************************************************************/

#define DSR_ETX_EXTENSION_HDRLEN (sizeof(uint8_t))

#define DSR_ETX_MAX_DATA_IN_EXTENSION ((DESSERT_MAXEXTDATALEN - DSR_ETX_EXTENSION_HDRLEN) / sizeof(dsr_etx_data_t))

#define DSR_ETX_DATA_LENGTH (DSR_ETX_MAX_DATA_IN_EXTENSION * sizeof(dsr_etx_data_t))

#define DSR_ETX_EXTENSION_LENGTH (DSR_ETX_DATA_LENGTH + sizeof(uint8_t) )

typedef struct __attribute__((__packed__)) dsr_etx_data {
    uint8_t address[ETHER_ADDR_LEN];
    uint8_t probes_received;
} dsr_etx_data_t;

typedef struct __attribute__((__packed__)) dsr_etx_ext {
    uint8_t len;
    dsr_etx_data_t data[DSR_ETX_MAX_DATA_IN_EXTENSION];
} dsr_etx_ext_t;


typedef struct __attribute__((__packed__)) dsr_etx_window_data {
    uint8_t local_address[ETHER_ADDR_LEN];  /* key part 1 */
    uint8_t remote_address[ETHER_ADDR_LEN]; /* key part 2 */
    uint16_t probes;   /* reverse delivery probes received by me       (bits set) [up-to-date] */
    uint8_t my_probes; /* forward delivery probes received by neighbor (count)    [last known] */
    struct timeval last_probe;
    UT_hash_handle hh;
} dsr_etx_window_data_t;

typedef struct __attribute__((__packed__)) dsr_etx_window_data_lookup_key {
    uint8_t local_address[ETHER_ADDR_LEN];  /* key part 1 */
    uint8_t remote_address[ETHER_ADDR_LEN]; /* key part 2 */
} dsr_etx_window_data_lookup_key_t;

#define DSR_ETX_WINDOW_DATA_LOOKUP_KEY_SIZE (sizeof(dsr_etx_window_data_lookup_key_t))


inline double dsr_etx_get_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
inline double dsr_unicast_etx_get_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
inline double dsr_etx_get_forward_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);

inline uint16_t dsr_etx_encode(double etx);
inline uint16_t dsr_etx_encode_to_network(double etx);
inline double dsr_etx_decode(uint16_t etx);
inline double dsr_etx_decode_from_network(uint16_t etx);
inline dsr_etx_ext_t* dsr_msg_add_etx_ext(dessert_msg_t* msg, uint8_t ETX_TYPE);

dessert_cb_result etx_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);
dessert_cb_result unicast_etx_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id);

int dessert_cli_cmd_showetx(struct cli_def* cli, char* command, char* argv[], int argc);
int dessert_cli_cmd_showunicastetx(struct cli_def* cli, char* command, char* argv[], int argc);

dessert_per_result_t dsr_etx_send_probes(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t dsr_unicastetx_send_probes(void* data, struct timeval* scheduled, struct timeval* interval);
dessert_per_result_t dsr_etx_cleanup(void* data, struct timeval* scheduled,	struct timeval* interval);


#endif /* ETX_H_ */
