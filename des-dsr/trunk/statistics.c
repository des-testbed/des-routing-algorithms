/******************************************************************************
 Copyright 2009,  2010, David Gutzmann, Freie Universitaet Berlin (FUB).
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

#include "dsr.h"

pthread_mutex_t _dsr_statistics_mutex = PTHREAD_MUTEX_INITIALIZER;
#define _STATISTICS_LOCK pthread_mutex_lock(&_dsr_statistics_mutex)
#define _STATISTICS_UNLOCK pthread_mutex_unlock(&_dsr_statistics_mutex)
#define _SAFE_RETURN(x) _STATISTICS_UNLOCK; return(x);

static inline dsr_statistics_link_t* _get_link_data(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
static inline dsr_statistics_link_t* _new_link_data(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
static void _print_link_statistics_to_cli(struct cli_def* cli, uint8_t local[ETHER_ADDR_LEN], uint8_t remote[ETHER_ADDR_LEN], dsr_statistics_data_t data);

static dsr_statistics_t _stat;

int statistics_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dsr_statistics_link_t* link = NULL;
    dessert_ext_t* ext = NULL;
    int promiscous = 0;

    if(!(proc->lflags & DESSERT_RX_FLAG_L2_DST) || proc->lflags
       & DESSERT_RX_FLAG_L2_OVERHEARD) {
        /* We are not the intended recipient of this message or - if we are - we
         * did not receive the message via the correct interface. */
        promiscous = 1;
    }

    _STATISTICS_LOCK;

    link = _get_link_data(iface->hwaddr, msg->l2h.ether_shost);

    _stat.total.rx_data_bytes += ntohs(msg->plen);
    _stat.total.rx_routing_bytes += ntohs(msg->hlen);
    link->data.rx_data_bytes += ntohs(msg->plen);
    link->data.rx_routing_bytes += ntohs(msg->hlen);

    if(dessert_msg_getext(msg, &ext, DESSERT_EXT_ETH, 0) > 0) {
        _stat.total.rx_data++;
        link->data.rx_data++;
    }
    else {
        _stat.total.rx_routing++;
        link->data.rx_routing++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RREQ, 0) > 0) {
        _stat.total.rx_rreq++;
        link->data.rx_rreq++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_SOURCE, 0) > 0) {
        _stat.total.rx_source++;
        link->data.rx_source++;

        if(promiscous) {
            _stat.total.prx_source++;
            link->data.prx_source++;
        }
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_REPL, 0) > 0) {
        _stat.total.rx_repl++;
        link->data.rx_repl++;

        if(promiscous) {
            _stat.total.prx_repl++;
            link->data.prx_repl++;
        }
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RERR, 0) > 0) {
        _stat.total.rx_rerr++;
        link->data.rx_rerr++;

        if(promiscous) {
            _stat.total.prx_rerr++;
            link->data.prx_rerr++;
        }
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACK, 0) > 0) {
        _stat.total.rx_ack++;
        link->data.rx_ack++;

        if(promiscous) {
            _stat.total.prx_ack++;
            link->data.prx_ack++;
        }
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACKREQ, 0) > 0) {
        _stat.total.rx_ackreq++;
        link->data.rx_ackreq++;

        if(promiscous) {
            _stat.total.prx_ackreq++;
            link->data.prx_ackreq++;
        }
    }


#if (METRIC == ETX)
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_ETX, 0) > 0) {
        _stat.total.rx_etx++;
        link->data.rx_etx++;

        if(promiscous) {
            _stat.total.prx_etx++;
            link->data.prx_etx++;
        }
    }
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_UNICAST_ETX, 0) > 0) {
        _stat.total.rx_uetx++;
        link->data.rx_uetx++;

        if(promiscous) {
            _stat.total.prx_uetx++;
            link->data.prx_uetx++;
        }
    }

#endif

    _STATISTICS_UNLOCK;

    return DESSERT_MSG_KEEP;
}

inline void dsr_statistics_emit_msg(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], dessert_msg_t* msg) {
    dessert_ext_t* ext = NULL;

    _STATISTICS_LOCK;

    _stat.emit_data_bytes += ntohs(msg->plen);
    _stat.emit_routing_bytes += ntohs(msg->hlen);

    if(dessert_msg_getext(msg, &ext, DESSERT_EXT_ETH, 0) > 0) {
        _stat.emit_data++;
    }
    else {
        _stat.emit_routing++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RREQ, 0) > 0) {
        _stat.emit_rreq++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_SOURCE, 0) > 0) {
        _stat.emit_source++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_REPL, 0) > 0) {
        _stat.emit_repl++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RERR, 0) > 0) {
        _stat.emit_rerr++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACK, 0) > 0) {
        _stat.emit_ack++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACKREQ, 0) > 0) {
        _stat.emit_ackreq++;
    }


#if (METRIC == ETX)
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_ETX, 0) > 0) {
        _stat.emit_etx++;
    }
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_UNICAST_ETX, 0) > 0) {
        _stat.emit_uetx++;
    }

#endif

    _STATISTICS_UNLOCK;

    dsr_statistics_tx_msg(local, remote, msg);
}

inline void dsr_statistics_tx_msg(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], dessert_msg_t* msg) {
    dsr_statistics_link_t* link = NULL;
    dessert_ext_t* ext = NULL;

    _STATISTICS_LOCK;

    link = _get_link_data(local, remote);

    _stat.total.tx_data_bytes += ntohs(msg->plen);
    _stat.total.tx_routing_bytes += ntohs(msg->hlen);
    link->data.tx_data_bytes += ntohs(msg->plen);
    link->data.tx_routing_bytes += ntohs(msg->hlen);

    if(dessert_msg_getext(msg, &ext, DESSERT_EXT_ETH, 0) > 0) {
        _stat.total.tx_data++;
        link->data.tx_data++;
    }
    else {
        _stat.total.tx_routing++;
        link->data.tx_routing++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RREQ, 0) > 0) {
        _stat.total.tx_rreq++;
        link->data.tx_rreq++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_SOURCE, 0) > 0) {
        _stat.total.tx_source++;
        link->data.tx_source++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_REPL, 0) > 0) {
        _stat.total.tx_repl++;
        link->data.tx_repl++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_RERR, 0) > 0) {
        _stat.total.tx_rerr++;
        link->data.tx_rerr++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACK, 0) > 0) {
        _stat.total.tx_ack++;
        link->data.tx_ack++;
    }

    if(dessert_msg_getext(msg, &ext, DSR_EXT_ACKREQ, 0) > 0) {
        _stat.total.tx_ackreq++;
        link->data.tx_ackreq++;
    }

#if (METRIC == ETX)
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_ETX, 0) > 0) {
        _stat.total.tx_etx++;
        link->data.tx_etx++;
    }
    else if(dessert_msg_getext(msg, &ext, DSR_EXT_UNICAST_ETX, 0) > 0) {
        _stat.total.tx_uetx++;
        link->data.tx_uetx++;
    }

#endif

    _STATISTICS_UNLOCK;
}

int dessert_cli_cmd_showstatistics(struct cli_def* cli, char* command, char* argv[], int argc) {
    dsr_statistics_link_t* link = NULL;

    _STATISTICS_LOCK;
    cli_print(cli, "emit [" MAC "] [" MAC "] "
              "rreq[%i] repl[%i] source[%i] ack[%i] ackreq[%i] etx[%i] uetx[%i] "
              "rerr[%i] data[%i] data_bytes[%i] routing[%i] routing_bytes[%i]",
              EXPLODE_ARRAY6(dessert_l25_defsrc), EXPLODE_ARRAY6(ether_null), _stat.emit_rreq, _stat.emit_repl,
              _stat.emit_source, _stat.emit_ack, _stat.emit_ackreq,
              _stat.emit_etx, _stat.emit_uetx, _stat.emit_rerr, _stat.emit_data,
              _stat.emit_data_bytes, _stat.emit_routing, _stat.emit_routing_bytes);

    /* print the total*/
    _print_link_statistics_to_cli(cli, dessert_l25_defsrc, ether_null, _stat.total);

    HASH_FOREACH(hh, _stat.nodes, link) {
        _print_link_statistics_to_cli(cli, link->local, link->remote, link->data);
    }

    _STATISTICS_UNLOCK;

    return CLI_OK;
}

static void _print_link_statistics_to_cli(struct cli_def* cli, uint8_t local[ETHER_ADDR_LEN], uint8_t remote[ETHER_ADDR_LEN], dsr_statistics_data_t data) {
    int total = 0;

    if(ADDR_CMP(remote, ether_null) == 0) {
        total = 1;
    }

    cli_print(cli, "stat-%i [" MAC "] [" MAC "] "
        "rx_rreq[%i] tx_rreq[%i] rx_repl[%i] prx_repl[%i] tx_repl[%i] rx_s[%i] "
        "prx_s[%i] tx_s[%i] rx_a[%i] prx_a[%i] tx_a[%i] rx_ar[%i] "
        "prx_ar[%i] tx_ar[%i] rx_e[%i] prx_e[%i] tx_e[%i] rx_ue[%i] "
        "prx_ue[%i] tx_ue[%i] rx_err[%i] prx_err[%i] tx_err[%i] rx_d[%i] "
        "rx_db[%i] prx_d[%i] prx_db[%i] tx_d[%i] tx_db[%i] rx_r[%i] "
        "rx_rb[%i] prx_r[%i] prx_rb[%i] tx_r[%i] tx_rb[%i]",
        total, local[0], local[1], local[2], local[3], local[4], local[5],
        remote[0], remote[1], remote[2], remote[3], remote[4], remote[5],
        data.rx_rreq, data.tx_rreq, data.rx_repl, data.prx_repl, data.tx_repl, data.rx_source,
        data.prx_source, data.tx_source, data.rx_ack, data.prx_ack, data.tx_ack, data.rx_ackreq,
        data.prx_ackreq, data.tx_ackreq, data.rx_etx, data.prx_etx, data.tx_etx, data.rx_uetx,
        data.prx_uetx, data.tx_uetx, data.rx_rerr, data.prx_rerr, data.tx_rerr, data.rx_data,
        data.rx_data_bytes, data.prx_data, data.prx_data_bytes, data.tx_data, data.tx_data_bytes, data.rx_routing,
        data.rx_routing_bytes, data.prx_routing, data.prx_routing_bytes, data.tx_routing, data.tx_routing_bytes);
}


static inline dsr_statistics_link_t* _get_link_data(
    const uint8_t local[ETHER_ADDR_LEN],
    const uint8_t remote[ETHER_ADDR_LEN]) {

    dsr_statistics_link_t* data = NULL;
    dsr_statistics_link_lookup_key_t lookupkey;

    ADDR_CPY(lookupkey.local, local);
    ADDR_CPY(lookupkey.remote, remote);

    HASH_FIND(hh, _stat.nodes, &lookupkey, sizeof(dsr_statistics_link_lookup_key_t), data);

    if(data == NULL) {
        data = _new_link_data(local, remote);
    }
    else {
    }

    return data;
}

static inline dsr_statistics_link_t* _new_link_data(
    const uint8_t local[ETHER_ADDR_LEN],
    const uint8_t remote[ETHER_ADDR_LEN]) {

    dsr_statistics_link_t* data = NULL;

    data = malloc(sizeof(dsr_statistics_link_t));
    assert(data != NULL);

    ADDR_CPY(data->local, local);
    ADDR_CPY(data->remote, remote);
    memset(&(data->data), 0, sizeof(dsr_statistics_data_t));

    HASH_ADD(hh, _stat.nodes, local, sizeof(dsr_statistics_link_lookup_key_t), data);

    return data;
}
