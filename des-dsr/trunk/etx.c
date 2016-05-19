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

#if (METRIC == ETX)

pthread_rwlock_t _dsr_etx_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define _ETX_READLOCK pthread_rwlock_rdlock(&_dsr_etx_rwlock)
#define _ETX_WRITELOCK pthread_rwlock_wrlock(&_dsr_etx_rwlock)
#define _ETX_UNLOCK pthread_rwlock_unlock(&_dsr_etx_rwlock)

#define ETX_DATA_TYPE_BROADCAST 0
#define ETX_DATA_TYPE_UNICAST   1

int etx_time = 0;
int unicast_etx_time = 0;
dsr_etx_window_data_t* etx_data = NULL;
dsr_etx_window_data_t* unicast_etx_data = NULL;


/* local forward declarations */
static inline void _count_probe(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], uint8_t my_probes);
static inline uint8_t _extract_my_probes(const uint8_t local[ETHER_ADDR_LEN], const uint8_t len, const dsr_etx_data_t data[DSR_ETX_MAX_DATA_IN_EXTENSION]);
static inline uint8_t _get_probes_received_in_window(const dsr_etx_window_data_t* data);
static inline double _get_value_by_window_data(dsr_etx_window_data_t* data);
static inline dsr_etx_window_data_t* _get_window_data_by_key(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
static inline dsr_etx_window_data_t* _new_window_data(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]);
static inline double _etx_w_r(void);
static inline double _etx_d_f(uint8_t my_probes);
static inline double _etx_d_r(uint8_t probes_received);


inline double dsr_etx_get_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]) {
    _ETX_READLOCK;

    dsr_etx_window_data_t* data = _get_window_data_by_key(ETX_DATA_TYPE_BROADCAST, local, remote);

    if(data == NULL) {
        dessert_err("window data is NULL for link (" MAC ")-(" MAC ")", EXPLODE_ARRAY6(local), EXPLODE_ARRAY6(remote));
        _ETX_UNLOCK;
        return ((UINT16_MAX) / 100);
    }

    double value = _get_value_by_window_data(data);
    _ETX_UNLOCK;

    return value;
}

inline double dsr_unicast_etx_get_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]) {
    _ETX_READLOCK;

    dsr_etx_window_data_t* data = _get_window_data_by_key(ETX_DATA_TYPE_UNICAST, local, remote);

    if(data == NULL) {
        dessert_err("unicast window data is NULL for link (" MAC ")-(" MAC ")", EXPLODE_ARRAY6(local), EXPLODE_ARRAY6(remote));
        _ETX_UNLOCK;
        return ((UINT16_MAX) / 100);
    }

    double value = _get_value_by_window_data(data);
    _ETX_UNLOCK;
    return value;
}

inline double dsr_etx_get_forward_value(const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]) {
    _ETX_READLOCK;

    dsr_etx_window_data_t* data = _get_window_data_by_key(ETX_DATA_TYPE_BROADCAST, local, remote);

    if(data == NULL) {
        dessert_err("window data is NULL for link (" MAC ")-(" MAC ")", EXPLODE_ARRAY6(local), EXPLODE_ARRAY6(remote));
        _ETX_UNLOCK;
        return 0;
    }

    uint8_t my_probes = data->my_probes;

    _ETX_UNLOCK;

    return (my_probes ? (1 / _etx_d_f(my_probes)) : 0);
}

inline uint16_t dsr_etx_encode(double etx) {
    return etx * 100;
}

dessert_cb_result etx_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* etx_ext;
    dsr_etx_ext_t* etx;

    if(!dessert_msg_getext(msg, &etx_ext, DSR_EXT_ETX, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        etx = (dsr_etx_ext_t*) etx_ext->data;
    }

    uint8_t my_probes = _extract_my_probes(iface->hwaddr, etx->len, etx->data);

    _ETX_WRITELOCK;
    _count_probe(ETX_DATA_TYPE_BROADCAST, iface->hwaddr, msg->l2h.ether_shost, my_probes);
    _ETX_UNLOCK;

    return DESSERT_MSG_DROP;
}

int unicast_etx_meshrx_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t* proc, dessert_meshif_t* iface, dessert_frameid_t id) {
    dessert_ext_t* etx_ext;
    dsr_etx_ext_t* etx;

    if(!dessert_msg_getext(msg, &etx_ext, DSR_EXT_UNICAST_ETX, 0)) {
        return DESSERT_MSG_KEEP;
    }
    else {
        etx = (dsr_etx_ext_t*) etx_ext->data;
    }

    if(proc->lflags & DESSERT_RX_FLAG_L2_OVERHEARD || !(proc->lflags & DESSERT_RX_FLAG_L2_DST)) {
        return DESSERT_MSG_DROP;
    }

    uint8_t my_probes = _extract_my_probes(iface->hwaddr, etx->len, etx->data);

    _ETX_WRITELOCK;
    _count_probe(ETX_DATA_TYPE_UNICAST, iface->hwaddr, msg->l2h.ether_shost, my_probes);
    _ETX_UNLOCK;

    return DESSERT_MSG_DROP;
}

inline dsr_etx_ext_t* dsr_msg_add_etx_ext(dessert_msg_t* msg, uint8_t ETX_TYPE) {
    dessert_ext_t* etx_ext;
    dsr_etx_ext_t* etx;
    int res = 0;

    res = dessert_msg_addext(msg, &etx_ext, ETX_TYPE, DSR_ETX_EXTENSION_LENGTH);

    assert(res == DESSERT_OK);

    etx = (dsr_etx_ext_t*) etx_ext->data;
    etx->len = 0;
    memset(etx->data, 170, DSR_ETX_DATA_LENGTH); /* 170 is 10101010 */

    return etx;
}

/******************************************************************************
 *
 * C L I --
 *
 ******************************************************************************/

int dessert_cli_cmd_showetx(struct cli_def* cli, char* command, char* argv[], int argc) {
    dsr_etx_window_data_t* data;

    _ETX_READLOCK;

    double etx;
    int encoded;
    int i = 0;

    HASH_FOREACH(hh, etx_data, data) {
        i++;
        etx = _get_value_by_window_data(data);
        encoded = dsr_etx_encode(etx);
        cli_print(cli,
                  "[%03d] local interface [" MAC "] remote host [" MAC "] ETX [%03.2f] encoded [%06d] my_probes [%02u] probes_received [%02u]",
                  i, EXPLODE_ARRAY6(data->local_address), EXPLODE_ARRAY6(data->remote_address), etx, encoded,
                  data->my_probes, _get_probes_received_in_window(data));
    }

    _ETX_UNLOCK;

    return CLI_OK;
}

int dessert_cli_cmd_showunicastetx(struct cli_def* cli, char* command, char* argv[], int argc) {
    dsr_etx_window_data_t* data;

    _ETX_READLOCK;
    double etx;
    int encoded;
    int i = 0;

    HASH_FOREACH(hh, unicast_etx_data, data) {
        i++;
        etx = _get_value_by_window_data(data);
        encoded = dsr_etx_encode(etx);
        cli_print(cli,
                  "[%03d] local interface [" MAC "] remote host [" MAC "] ETX [%03.2f] encoded [%06d] my_probes [%02u] probes_received [%02u]",
                  i, EXPLODE_ARRAY6(data->local_address), EXPLODE_ARRAY6(data->remote_address), etx, encoded,
                  data->my_probes, _get_probes_received_in_window(data));
    }
    _ETX_UNLOCK;

    return CLI_OK;
}

/******************************************************************************
 *
 * Periodic tasks --
 *
 ******************************************************************************/

dessert_per_result_t dsr_etx_send_probes(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_etx_window_data_t* el;

    dessert_meshif_t* meshif;
    dessert_msg_t* etx_msg;
    dsr_etx_ext_t* etx;
    dsr_etx_data_t* piggy_data;
    int len = 0;
    MESHIFLIST_ITERATOR_START(meshif) {
        dessert_msg_new(&etx_msg);
        etx = dsr_msg_add_etx_ext(etx_msg, DSR_EXT_ETX);
        //		dsr_msg_add_etx_ext(etx_msg, DSR_EXT_ETX);
        //		dsr_msg_add_etx_ext(etx_msg, DSR_EXT_ETX);
        //		dsr_msg_add_etx_ext(etx_msg, DSR_EXT_ETX);
        /* could be heavily optimized --> store neighbors per interface */
        len = 0;
        _ETX_READLOCK;
        HASH_FOREACH(hh, etx_data, el) {
            if(ADDR_CMP(el->local_address, meshif->hwaddr) == 0) {
                piggy_data = &etx->data[len++];
                ADDR_CPY(piggy_data->address, el->remote_address);
                piggy_data->probes_received = _get_probes_received_in_window(el);
            }
        }
        etx->len = len;
        _ETX_UNLOCK;

        ADDR_CPY(etx_msg->l2h.ether_dhost, ether_broadcast);

        dsr_statistics_emit_msg(meshif->hwaddr, etx_msg->l2h.ether_dhost, etx_msg);
        dessert_meshsend_fast(etx_msg, meshif);
        dessert_msg_destroy(etx_msg);
    }
    MESHIFLIST_ITERATOR_STOP;

    _ETX_WRITELOCK;
    /* increment etx time MODULO etx window size*/
    etx_time = etx_time + 1 ;
    etx_time = etx_time % DSR_CONFVAR_ETX_WINDOW_SIZE_SECS;

    /* clear the corresponding bit in all etx windows*/
    HASH_FOREACH(hh, etx_data, el) {
        el->probes &= ~(1 << etx_time);
    }
    _ETX_UNLOCK;

    return DESSERT_PER_KEEP;
}

dessert_per_result_t dsr_unicastetx_send_probes(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_etx_window_data_t* el;
    dsr_etx_window_data_t* unicast_el;

    dessert_meshif_t* meshif;
    dessert_msg_t* etx_msg;
    dsr_etx_ext_t* etx;
    dsr_etx_data_t* piggy_data;

    _ETX_WRITELOCK;
    HASH_FOREACH(hh, etx_data, el) {
        if(_get_value_by_window_data(el) <= 7.0) {
            unicast_el = _get_window_data_by_key(ETX_DATA_TYPE_UNICAST, el->local_address, el->remote_address);
            if(unicast_el == NULL) {
                unicast_el = _new_window_data(ETX_DATA_TYPE_UNICAST, el->local_address, el->remote_address);
            }
        }
    }
    el = NULL;
    _ETX_UNLOCK;

    MESHIFLIST_ITERATOR_START(meshif) {
        dessert_msg_new(&etx_msg);
        etx = dsr_msg_add_etx_ext(etx_msg, DSR_EXT_UNICAST_ETX);
        dsr_msg_add_etx_ext(etx_msg, DSR_EXT_UNICAST_ETX);
        dsr_msg_add_etx_ext(etx_msg, DSR_EXT_UNICAST_ETX);
        dsr_msg_add_etx_ext(etx_msg, DSR_EXT_UNICAST_ETX);

        /* could be heavily optimized --> store neighbors per interface */
        _ETX_READLOCK;
        HASH_FOREACH(hh, unicast_etx_data, el) {
            if(ADDR_CMP(el->local_address, meshif->hwaddr) == 0) {
                piggy_data = &etx->data[0];
                ADDR_CPY(piggy_data->address, el->remote_address);
                piggy_data->probes_received = _get_probes_received_in_window(el);

                etx->len = 1;
                ADDR_CPY(etx_msg->l2h.ether_dhost, el->remote_address);
                dsr_statistics_emit_msg(meshif->hwaddr, etx_msg->l2h.ether_dhost, etx_msg);
                dessert_meshsend_fast(etx_msg, meshif);
            }
        }
        _ETX_UNLOCK;
        dessert_msg_destroy(etx_msg);
    }
    MESHIFLIST_ITERATOR_STOP;

    _ETX_WRITELOCK;
    /* increment unietx time MODULO etx window size*/
    unicast_etx_time = unicast_etx_time + 1 ;
    unicast_etx_time = unicast_etx_time % DSR_CONFVAR_ETX_WINDOW_SIZE_SECS;

    /* clear the corresponding bit in all unietx windows*/
    HASH_FOREACH(hh, unicast_etx_data, el) {
        el->probes &= ~(1 << unicast_etx_time);
    }
    _ETX_UNLOCK;

    return DESSERT_PER_KEEP;
}

dessert_per_result_t dsr_etx_cleanup(void* data, struct timeval* scheduled, struct timeval* interval) {
    dsr_etx_window_data_t* window_data = NULL;
    dsr_etx_window_data_t* next_window_data = NULL;

    _ETX_WRITELOCK;

    window_data = etx_data;

    while(window_data) {
        next_window_data = window_data->hh.next;

        if(window_data->last_probe.tv_sec
           + DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS < scheduled->tv_sec) {

            HASH_DELETE(hh, etx_data, window_data);
            dessert_info("ETX: Removing link " MAC " <--> " MAC " . No probes were received for %u seconds.", EXPLODE_ARRAY6(window_data->local_address), EXPLODE_ARRAY6(window_data->remote_address), DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS);
            free(window_data);
            window_data = NULL;
        }

        window_data = next_window_data;
    }

    window_data = unicast_etx_data;

    while(window_data) {
        next_window_data = window_data->hh.next;

        if(window_data->last_probe.tv_sec + DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS < scheduled->tv_sec) {
            HASH_DELETE(hh, unicast_etx_data, window_data);
            dessert_info("UNICASTETX: Removing link " MAC " <--> " MAC " . No probes were received for %u seconds.", EXPLODE_ARRAY6(window_data->local_address), EXPLODE_ARRAY6(window_data->remote_address), DSR_CONFVAR_ETX_LAST_SEEN_TIME_SECS);
            free(window_data);
            window_data = NULL;
        }

        window_data = next_window_data;
    }

    _ETX_UNLOCK;

    return DESSERT_PER_KEEP;
}

/******************************************************************************
 *
 * LOCAL
 *
 ******************************************************************************/


static inline uint8_t _get_probes_received_in_window(const dsr_etx_window_data_t* data) {
    if(data == NULL) {
        return 0;
    }

    return __builtin_popcount(data->probes);
}

static inline double _get_value_by_window_data(dsr_etx_window_data_t* data) {
    assert(data != NULL);

    uint8_t my_probes = data->my_probes;
    uint8_t probes_received = _get_probes_received_in_window(data);

    if(my_probes == 0 || probes_received == 0) {
        return ((UINT16_MAX) / 100);
    }

    return 1 / (_etx_d_f(my_probes) * _etx_d_r(probes_received));
}

static inline uint8_t _extract_my_probes(const uint8_t local[ETHER_ADDR_LEN], const uint8_t len, const dsr_etx_data_t data[DSR_ETX_MAX_DATA_IN_EXTENSION]) {
    int i = 0;

    for(; i < len; i++) {
        if(ADDR_CMP(data[i].address, local) == 0) {
            return data[i].probes_received;
        }
    }

    return 0;
}

static inline void _count_probe(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN], uint8_t my_probes) {
    dsr_etx_window_data_t* data = NULL;

    data = _get_window_data_by_key(etx_data_type, local, remote);

    if(data == NULL) {
        data = _new_window_data(etx_data_type, local, remote);
    }

    assert(data != NULL);

    gettimeofday(&data->last_probe, NULL);

    if(etx_data_type == ETX_DATA_TYPE_BROADCAST) {
        data->probes |= 1 << etx_time;
    }
    else {
        data->probes |= 1 << unicast_etx_time;
    }

    data->my_probes = my_probes;

}

static inline dsr_etx_window_data_t* _get_window_data_by_key(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]) {
    dsr_etx_window_data_t* data = NULL;
    dsr_etx_window_data_lookup_key_t lookup_key;

    ADDR_CPY(lookup_key.local_address, local);
    ADDR_CPY(lookup_key.remote_address, remote);

    if(etx_data_type == ETX_DATA_TYPE_BROADCAST) {
        HASH_FIND(hh, etx_data, &lookup_key, DSR_ETX_WINDOW_DATA_LOOKUP_KEY_SIZE, data);
    }
    else {
        HASH_FIND(hh, unicast_etx_data, &lookup_key, DSR_ETX_WINDOW_DATA_LOOKUP_KEY_SIZE, data);
    }

    return data;
}

static inline dsr_etx_window_data_t* _new_window_data(int etx_data_type, const uint8_t local[ETHER_ADDR_LEN], const uint8_t remote[ETHER_ADDR_LEN]) {
    dsr_etx_window_data_t* data = NULL;

    struct timeval now;
    gettimeofday(&now, NULL);

    data = malloc(sizeof(dsr_etx_window_data_t));
    assert(data != NULL);

    ADDR_CPY(data->local_address, local);
    ADDR_CPY(data->remote_address, remote);
    data->probes = 0;
    data->my_probes = 0;
    data->last_probe.tv_sec = now.tv_sec;
    data->last_probe.tv_usec = now.tv_usec;

    if(etx_data_type == ETX_DATA_TYPE_BROADCAST) {
        HASH_ADD(hh, etx_data, local_address, DSR_ETX_WINDOW_DATA_LOOKUP_KEY_SIZE, data);
    }
    else {
        HASH_ADD(hh, unicast_etx_data, local_address, DSR_ETX_WINDOW_DATA_LOOKUP_KEY_SIZE, data);
    }

    return data;
}

static inline double _etx_w_r(void) {
    return DSR_CONFVAR_ETX_WINDOW_SIZE_SECS / DSR_CONFVAR_ETX_PROBE_RATE_SECS;
}

static inline double _etx_d_f(uint8_t my_probes) {
    return my_probes / _etx_w_r();
}

static inline double _etx_d_r(uint8_t probes_received) {
    return probes_received / _etx_w_r();
}


#endif /* (METRIC == ETX) */


