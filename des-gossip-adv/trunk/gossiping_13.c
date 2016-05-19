/******************************************************************************
 Copyright 2011, Bastian Blywis Freie Universitaet Berlin
 (FUB).
 All rights reserved.

 These sources were originally developed by Bastian Blywis
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
*******************************************************************************/
#ifndef ANDROID

#include "gossiping.h"
#include <utlist.h>

#define DEFAULT_RESTARTS 5
#define MAX_UPDATE_INTERVAL_MS 20000
#define MAX_UPDATES_TO_KEEP 20
#define MAX_MISSED_UPDATES 5


#define SORT_TS(s, e) \
    if(s > e) { uint32_t t = s; s = e; e = t; }

/*** will be moved to libdessert ***/

void dessert_parse_mac2(char* mac_str, mac_addr* mac_parsed) {
    uint8_t i;
    for(i=0; i<18; i+=3) {
        unsigned int num = 0;
        sscanf(mac_str+i, "%2x", &num);
        (*mac_parsed)[i/3] = (uint8_t) num;
    }
}

/*** global vars ***/

extern dessert_sysif_t* _dessert_sysif;

bool gossip13_drop_seq2_duplicates = true;
bool gossip13_piggyback = false;
bool gossip13_unify_schedule = false;
bool gossip13_send_updates = false;

typedef struct {
    mac_addr mac;
    struct timeval last_rx_update;
    uint32_t update_ms;
} gossip13_schedule_t;

gossip13_schedule_t gossip13_schedule = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  {0, 0}, 0 };

uint16_t next_seq2 = 0;
uint8_t restarts = DEFAULT_RESTARTS ;
dessert_periodic_t* gossip13_task_emission = NULL;
dessert_periodic_t* gossip13_task_update = NULL;
dessert_periodic_t* gossip13_task_observations = NULL;
dessert_periodic_t* gossip13_task_cleanup = NULL;
struct timeval gossip13_restart_interval = { 0, 100*1000 };
struct timeval gossip13_emission_interval = { 0, 100*1000 };
struct timeval gossip13_update_interval = { 10, 0 };
struct timeval gossip13_observations_interval = { 1, 0 };
struct timeval gossip13_cleanup_interval = { 30, 0 };
uint32_t last_update_ms = 0;
uint32_t gossip13_deadline_ms = 10 * 1000;
pthread_mutex_t seq2_mutex = PTHREAD_MUTEX_INITIALIZER;

/*** private structs and enums ***/

typedef struct {
    dessert_msg_t*  msg;
    uint8_t         remaining_restarts;
} gossip13_stored_t;

typedef enum {
    network_ok,
    network_overloaded
} network_state_t;

/*** history database ***/

history_t gossip13_tx_history = { NULL, "tx", PTHREAD_RWLOCK_INITIALIZER };
history_t gossip13_txtotal_history = { NULL, "txtotal", PTHREAD_RWLOCK_INITIALIZER };
history_t gossip13_rx_history = { NULL, "rx", PTHREAD_RWLOCK_INITIALIZER };

history_mean_t gossip13_hops_history = { NULL, "hops", PTHREAD_RWLOCK_INITIALIZER};

void gossip13_write_sysaddr() {
    if(_dessert_sysif) {
        memcpy(gossip13_schedule.mac, _dessert_sysif->hwaddr, ETHER_ADDR_LEN);
    }
}

void gossip13_register_hop(history_mean_t* hist, uint8_t hops) {
    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    hist->observations->mean = hist->observations->mean * ((double) hist->observations->count)/(hist->observations->count+1) + ((double) hops)/(hist->observations->count+1);
    hist->observations->count++;
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_new_observation(observation_t** observation) {
    *observation = malloc(sizeof(observation_t));
    (*observation)->packets = 0;
    (*observation)->next = NULL;
    gettimeofday(&((*observation)->start), NULL);
    (*observation)->end.tv_sec = 0;
    (*observation)->end.tv_usec = 0;
}

void gossip13_new_observation_mean(observation_mean_t** observation) {
    *observation = malloc(sizeof(observation_mean_t));
    (*observation)->mean = 0.0;
    (*observation)->count = 0;
    (*observation)->next = NULL;
    gettimeofday(&((*observation)->start), NULL);
    (*observation)->end.tv_sec = 0;
    (*observation)->end.tv_usec = 0;
}

void gossip13_new_history(history_t** hist, bool local_observation) {
    *hist = malloc(sizeof(history_t));
    pthread_rwlock_init(&((*hist)->lock), NULL);
    (*hist)->observations = NULL;
    (*hist)->name = NULL;
}

void gossip13_new_history_mean(history_mean_t** hist, bool local_observation) {
    *hist = malloc(sizeof(history_mean_t));
    pthread_rwlock_init(&((*hist)->lock), NULL);
    (*hist)->observations = NULL;
    (*hist)->name = NULL;
}

inline void gossip13_store_observation(history_t* hist);
void gossip13_store_observation(history_t* hist) {
    if(hist->observations == NULL) {
        dessert_warn("history not initialized!");
        return;
    }
    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    gettimeofday(&(hist->observations->end), NULL);
    observation_t* ts = NULL;
    gossip13_new_observation(&ts);
    LL_PREPEND(hist->observations, ts);
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

inline void gossip13_store_observation_mean(history_mean_t* hist);
void gossip13_store_observation_mean(history_mean_t* hist) {
    if(hist->observations == NULL) {
        dessert_warn("history not initialized!");
        return;
    }
    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    gettimeofday(&(hist->observations->end), NULL);
    observation_mean_t* ts = NULL;
    gossip13_new_observation_mean(&ts);
    LL_PREPEND(hist->observations, ts);
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_register_observation(history_t* hist, uint32_t obs, uint32_t time_ms, uint32_t observation_time_ms) {
    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    observation_t* ts = malloc(sizeof(observation_t));
    dessert_ms2timeval(time_ms, &(ts->start));
    dessert_ms2timeval(time_ms+observation_time_ms, &(ts->end));
    ts->packets = obs;
    ts->next = NULL;
    LL_PREPEND(hist->observations, ts);
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_register_observation_mean(history_mean_t* hist, double mean, uint32_t count, uint32_t time_ms, uint32_t observation_time_ms) {
    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    observation_mean_t* ts = malloc(sizeof(observation_mean_t));
    dessert_ms2timeval(time_ms, &(ts->start));
    dessert_ms2timeval(time_ms+observation_time_ms, &(ts->end));
    ts->mean = mean;
    ts->count = count;
    ts->next = NULL;
    LL_PREPEND(hist->observations, ts);
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_cleanup_history(history_t* hist, uint32_t max_age_ms) {
    uint32_t cur_ms = dessert_cur_ms();
    observation_t *ts, *tmp = NULL;

    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    LL_FOREACH_SAFE(hist->observations, ts, tmp) {
        uint32_t start_ms = dessert_timeval2ms(&(ts->start));
        if(abs(cur_ms - start_ms) < max_age_ms) { // observation is fresh enough
            continue;
        }
        LL_DELETE(hist->observations, ts);
        free(ts);
    }
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_cleanup_history_mean(history_mean_t* hist, uint32_t max_age_ms) {
    uint32_t cur_ms = dessert_cur_ms();
    observation_mean_t *ts, *tmp = NULL;

    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    LL_FOREACH_SAFE(hist->observations, ts, tmp) {
        uint32_t start_ms = dessert_timeval2ms(&(ts->start));
        if(abs(cur_ms - start_ms) < max_age_ms) { // observation is fresh enough
            continue;
        }
        LL_DELETE(hist->observations, ts);
        free(ts);
    }
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
}

void gossip13_delete_history(history_t* hist) {
    observation_t *ts, *tmp = NULL;

    /* ### */
    pthread_rwlock_wrlock(&(hist->lock));
    LL_FOREACH_SAFE(hist->observations, ts, tmp) {
        LL_DELETE(hist->observations, ts);
        free(ts);
    }
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
    free(hist);
}

typedef enum _overlap {
    overlap_outside_front,
    overlap_outside_back,
    overlap_inside,
    overlap_partial_front,
    overlap_partial_back,
    overlap_invalid,
    overlap_unconsidered
} overlap_t;

/**
 * Checks if/how the interval [a, b] overlaps with the interval [c, d]
 *
 * invariant: a < b and c < d and abs(a-b) <= abs(c-d)
 *
 * outside_back:    a <  b < c <  d,   [a, b]
 *                                    -------------------->t
 *                                                [c, d]
 *
 * outside_front:   c <  d < a <  b               [a, b]
 *                                    -------------------->t
 *                                     [c, d]
 *
 * inside:          c <= a < b <= d          [a, b]
 *                                    -------------------->t
 *                                       [c,          d]
 *
 * partial_back:    a <  c and b <= d  [a,    b]
 *                                    -------------------->t
 *                                        [c,      d]
 *
 * partial_front:   c <= a and d <  b          [a,     b]
 *                                    -------------------->t
 *                                        [c,     d]
 *
 * @param start of interval [a, b], a < b
 * @param end of interval [a,b], a < b
 * @param start of interval [c, d], c < d
 * @param end of interval [c,d], c < d
 */
inline overlap_t _gossip13_overlap(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    SORT_TS(a, b);
    SORT_TS(c, d);

    if(abs(a-b) > abs(c-d)) {
        dessert_warn("invalid epochs: \n\t[a, b] = [%u, %u]\n\t[c, d] = [%u, %u]", a, b, c, d);
        return overlap_invalid;
    }

    if(a==b || c==d) {
        dessert_warn("null epoch: \n\t[a, b] = [%u, %u] = %u \n\t[c, d] = [%u, %u] = %u", a, b, b-a, c, d, d-c);
        return overlap_invalid;
    }

    if(a < b && b < c && c < d) {
        return overlap_outside_back;
    }
    else if(c < d && d < a && a < b) {
        return overlap_outside_front;
    }
    else if(c <= a && a < b && b <= d) {
        return overlap_inside;
    }
    else if(a <  c && b <= d) {
        return overlap_partial_back;
    }
    else if(c <= a && d < b) {
        return overlap_partial_front;
    }
    else {
        dessert_warn("unconsidered case: \n\t[a, b] = [%u, %u]\n\t[c, d] = [%u, %u]", a, b, c, d);
        return overlap_unconsidered;
    }
}

/** Get number of observations
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return number of observations
 */
double gossip13_get_observations(history_t* hist, uint32_t ts_start_ms, uint32_t ts_end_ms, double* ret_quality) {
    SORT_TS(ts_start_ms, ts_end_ms);
    uint32_t cur_ms = dessert_cur_ms();
    uint32_t prev_start_ms = cur_ms;
    uint32_t start_ms = 0;

    observation_t* ts = NULL;
    double obs = 0;
    uint32_t covered_ms = 0;

    /* ### */
    pthread_rwlock_rdlock(&(hist->lock));
    LL_FOREACH(hist->observations, ts) {
        start_ms = dessert_timeval2ms(&(ts->start));
        uint32_t end_ms = dessert_timeval2ms(&(ts->end));
        if(end_ms == 0) { // only for not yet finished (local) observations
            end_ms = cur_ms;
        }
        uint32_t observation_ms = abs(end_ms - start_ms);

        overlap_t overlap = _gossip13_overlap(start_ms, end_ms, ts_start_ms, ts_end_ms);
        if(overlap == overlap_unconsidered || overlap == overlap_invalid) {
            dessert_warn("\tcheck parameters in %s", hist->name);
            break;
        }

        if(prev_start_ms < start_ms) {
            dessert_warn("\toverlapping observations");
        }

        if(overlap == overlap_outside_front) {
            goto skip_to_next_obs;
        }

        if(overlap == overlap_partial_front) {
            uint32_t overlap_ms = abs(ts_end_ms - start_ms);
            double frac_partial;
            if(observation_ms == 0) {
                frac_partial = 0;
            }
            else {
                frac_partial = ((double) overlap_ms)/observation_ms;
            }
            obs +=  (ts->packets * frac_partial);
            covered_ms += overlap_ms;
            goto skip_to_next_obs;
        }

        if(overlap == overlap_inside) {
            obs += ts->packets;
            covered_ms += observation_ms;
        }

        if(overlap == overlap_partial_back) {
            uint32_t overlap_ms = abs(ts_start_ms - end_ms);
            double frac_partial;
            if(observation_ms == 0) {
                frac_partial = 0;
            }
            else {
                frac_partial = ((double) overlap_ms)/observation_ms;
            }
            obs +=  (ts->packets * frac_partial);
            covered_ms += overlap_ms;
            break;
        }

        if(overlap == overlap_outside_back) {
            break;
        }

skip_to_next_obs:
        prev_start_ms = start_ms;
    }
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */

    /*** EXTRA- and INTERPOLATE (assume uniform distribution of data) ***/
    double quality = ((double) covered_ms)/(ts_end_ms - ts_start_ms);
    uint32_t unknown_ms = (ts_end_ms - ts_start_ms) - covered_ms;
    if(quality < 0.5) {
//         dessert_warn("limited data available");
    }
    double extra_obs = ((double) obs) / ((ts_end_ms - ts_start_ms) - unknown_ms) * unknown_ms;

    if(ret_quality != NULL) {
        *ret_quality = quality;
    }

    return obs+extra_obs;
}

/** Get mean of observations
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return mean of observations
 */
double gossip13_get_observations_mean(history_mean_t* hist, uint32_t ts_start_ms, uint32_t ts_end_ms, double* ret_quality) {
    SORT_TS(ts_start_ms, ts_end_ms);
    uint32_t cur_ms = dessert_cur_ms();
    uint32_t prev_start_ms = cur_ms;
    uint32_t start_ms = 0;

    observation_mean_t* ts = NULL;
    double mean = 0.0;
    double obs = 0;
    uint32_t covered_ms = 0;

    /* ### */
    pthread_rwlock_rdlock(&(hist->lock));
    LL_FOREACH(hist->observations, ts) {
        start_ms = dessert_timeval2ms(&(ts->start));
        uint32_t end_ms = dessert_timeval2ms(&(ts->end));
        if(end_ms == 0) { // only for not yet finished (local) observations
            end_ms = cur_ms;
        }
        uint32_t observation_ms = abs(end_ms - start_ms);

        overlap_t overlap = _gossip13_overlap(start_ms, end_ms, ts_start_ms, ts_end_ms);
        if(overlap == overlap_unconsidered || overlap == overlap_invalid) {
            dessert_warn("\tcheck parameters in %s", hist->name);
            break;
        }

        if(prev_start_ms < start_ms) {
            dessert_warn("\toverlapping observations");
        }

        if(overlap == overlap_outside_front) {
            goto skip_to_next_obs;
        }

        if(overlap == overlap_partial_front) {
            if(ts->count > 0) {
                uint32_t overlap_ms = abs(ts_end_ms - start_ms);
                double frac_partial;
                if(observation_ms == 0) {
                    frac_partial = 0;
                }
                else {
                    frac_partial = ((double) overlap_ms)/observation_ms;
                }
                double new_obs = ts->count * frac_partial;
                mean = mean * obs/(new_obs + obs) + ts->mean * new_obs/(new_obs + obs);
                obs +=  new_obs;
                covered_ms += overlap_ms;
                goto skip_to_next_obs;
            }
        }

        if(overlap == overlap_inside) {
            if(ts->count > 0) {
                mean = mean * ((double) obs)/(ts->count + obs) + ts->mean * ((double) ts->count)/(ts->count + obs);
                obs += ts->count;
                covered_ms += observation_ms;
            }
        }

        if(overlap == overlap_partial_back) {
            if(ts->count > 0) {
                uint32_t overlap_ms = abs(ts_start_ms - end_ms);
                double frac_partial;
                if(observation_ms == 0) {
                    frac_partial = 0;
                }
                else {
                    frac_partial = ((double) overlap_ms)/observation_ms;
                }
                double new_obs = ts->count * frac_partial;
                mean = mean * obs/(new_obs + obs) + ts->mean * new_obs/(new_obs + obs);
                obs += new_obs;
                covered_ms += overlap_ms;
            }
            break;
        }

        if(overlap == overlap_outside_back) {
            break;
        }

skip_to_next_obs:
        prev_start_ms = start_ms;
    }
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */

    /*** EXTRA- and INTERPOLATE (assume uniform distribution of data) ***/
    double quality = ((double) covered_ms)/(ts_end_ms - ts_start_ms);
    uint32_t unknown_ms = (ts_end_ms - ts_start_ms) - covered_ms;
    if(quality < 0.5) {
//         dessert_warn("limited data available");
    }
    double extra_obs = obs / ((ts_end_ms - ts_start_ms) - unknown_ms) * unknown_ms;
    if(extra_obs > 0) {
        mean = mean * ((double) obs)/(extra_obs + obs) + mean * extra_obs/(extra_obs + obs);
    }

    if(ret_quality != NULL) {
        *ret_quality = quality;
    }

    return mean;
}

uint32_t gossip13_get_age(history_t* hist) {
    uint32_t age = UINT32_MAX;
    if(hist == NULL || hist->observations == NULL) {
        dessert_warn("history not initialized!");
        return age;
    }
    uint32_t cur_ms = dessert_cur_ms();

    /* ### */
    pthread_rwlock_rdlock(&(hist->lock));
    uint32_t end_ms = dessert_timeval2ms(&(hist->observations->end));
    pthread_rwlock_unlock(&(hist->lock));
    /* ### */
    if(end_ms == 0) {
        end_ms = cur_ms;
    }
    age = cur_ms - end_ms;
    return age;
}

/** Get number of transmitted packets
 *
 * The evaluation considers all observations that overlap with the periode.
 * Therefore, the maximum can be too optimistic (high).
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return transmitted packets
 */
double gossip13_get_total_packets(uint32_t ts_start_ms, uint32_t ts_end_ms) {
    double total = 0;
    SORT_TS(ts_start_ms, ts_end_ms);
    seqlog_t* s = NULL;

    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        uint32_t add = gossip13_get_observations(s->total_tx_history, ts_start_ms, ts_end_ms, NULL);
        if(isnan(add)) {
            dessert_warn("NAN in total_tx_history of " MAC, EXPLODE_ARRAY6(s->addr));
            continue;
        }
        total += add;
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/

    return total;
}

/** Number of known nodes
 *
 * Return the number of nodes from that packets were received in the specified
 * periode.
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return number of nodes
 */
uint8_t gossip13_get_num_nodes(uint32_t ts_start_ms, uint32_t ts_end_ms) {
    uint8_t total = 0;
    seqlog_t* s = NULL;

    SORT_TS(ts_start_ms, ts_end_ms);

    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        history_t* hist = s->observed_history;
        /*###*/
        pthread_rwlock_rdlock(&(hist->lock));

        observation_t* obs = NULL;
        LL_FOREACH(hist->observations, obs) {
            uint32_t start_ms = dessert_timeval2ms(&(obs->start));
            uint32_t end_ms = dessert_timeval2ms(&(obs->end));
            if(end_ms == 0) {
                end_ms = dessert_cur_ms();
            }
            overlap_t o = _gossip13_overlap(start_ms, end_ms, ts_start_ms, ts_end_ms);
            switch(o) {
                case overlap_outside_front:
                    continue;
                case overlap_outside_back:
                    goto out;
                case overlap_inside:
                    if(obs->packets > 0) {
                        total++;
                        goto out;
                    }
                    break;
                case overlap_partial_front:
                    if(obs->packets > 0) {
                        total++;
                        goto out;
                    }
                    break;
                case overlap_partial_back:
                    if(obs->packets > 0) {
                        total++;
                        goto out;
                    }
                    break;
                case overlap_invalid:
                case overlap_unconsidered:
                    goto out;
            }
        }
        out:
        pthread_rwlock_unlock(&(hist->lock));
        /*###*/
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return total;
}

/** Get maximum value
 *
 * Return the max. value in the history within the specified periode.
 * The evaluation considers all observations that overlap with the periode.
 * Therefore, the maximum can be too optimistic (high).
 *
 * @param hist history to evaluate
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return maximum number
 */
uint8_t gossip13_get_max(history_t* hist, uint32_t ts_start_ms, uint32_t ts_end_ms) {
    uint8_t maxval = 0;
    SORT_TS(ts_start_ms, ts_end_ms);

    /*###*/
    pthread_rwlock_rdlock(&(hist->lock));

    observation_t* obs = hist->observations;
    if(obs == NULL) {
        goto out;
    }
    uint32_t start_ms = dessert_timeval2ms(&(obs->start));
    uint32_t end_ms = dessert_timeval2ms(&(obs->end));

    // walk over all obs before ts_start_ms
    while(obs && start_ms > ts_start_ms) {
        obs = obs->next;
        if(obs) {
            start_ms = dessert_timeval2ms(&(obs->start));
            end_ms = dessert_timeval2ms(&(obs->end));
        }
    }

    if(obs) { // definitely evaluate this observation!
        maxval = max(obs->packets, maxval);
        obs = obs->next;
        if(obs) {
            start_ms = dessert_timeval2ms(&(obs->start));
            end_ms = dessert_timeval2ms(&(obs->end));
        }
    }

    while(obs && end_ms > ts_end_ms) {
        maxval = max(obs->packets, maxval);
        obs = obs->next;
        if(obs) {
            start_ms = dessert_timeval2ms(&(obs->start));
            end_ms = dessert_timeval2ms(&(obs->end));
        }
    }

    out:
    pthread_rwlock_unlock(&(hist->lock));
    /*###*/
    return maxval;
}

/** Get number of transmitted packets
 *
 * The evaluation considers all observations that overlap with the periode.
 * Therefore, the maximum can be too optimistic (high).
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return transmitted packets
 */
double gossip13_get_tx_packets(uint32_t ts_start_ms, uint32_t ts_end_ms) {
    double total = 0;
    SORT_TS(ts_start_ms, ts_end_ms);
    seqlog_t* s = NULL;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        double add = gossip13_get_observations(s->tx_history, ts_start_ms, ts_end_ms, NULL);
        total += add;
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return total;
}

/** Eccentricity
 *
 * Determine the maximum distance to all other nodes.
 *
 * The evaluation considers all observations that overlap with the periode.
 * Therefore, the maximum can be too optimistic (high).
 *
 * @param ts_start_ms timestamp in ms where to start
 * @param ts_end_ms timestamp in ms where to end
 * @return eccentricity, 0 if no packets have been received yet in the periode
 */
double gossip13_get_eccentricity(uint32_t ts_start_ms, uint32_t ts_end_ms) {
    double eccentricity = 0.0;
    SORT_TS(ts_start_ms, ts_end_ms);
    seqlog_t* s = NULL;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        double d = gossip13_get_observations_mean(s->my_hops_history, ts_start_ms, ts_end_ms, NULL);
        eccentricity = max(d, eccentricity);
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return eccentricity;
}


/** Adds a new observation slot to the histories
 *
 */
dessert_per_result_t gossip13_store_observations(void *data, struct timeval *scheduled, struct timeval *interval) {
    gossip13_store_observation(&gossip13_tx_history);
    gossip13_store_observation(&gossip13_txtotal_history);
    gossip13_store_observation(&gossip13_rx_history);
    gossip13_store_observation_mean(&gossip13_hops_history);

    seqlog_t* s;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        gossip13_store_observation(s->observed_history);
        gossip13_store_observation_mean(s->my_hops_history);
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return DESSERT_PER_KEEP;
}

/** Removes old observations from histories
 *
 * Removes all observation from the histories that are older
 * than MAX_UPDATE_INTERVAL_MS.
 */
dessert_per_result_t gossip13_cleanup_observations(void *data, struct timeval *scheduled, struct timeval *interval) {
    gossip13_cleanup_history(&gossip13_tx_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
    gossip13_cleanup_history(&gossip13_txtotal_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
    gossip13_cleanup_history(&gossip13_rx_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
    gossip13_cleanup_history_mean(&gossip13_hops_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
    seqlog_t* s;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    for(s=seqlog; s != NULL; s=s->hh.next) {
        gossip13_cleanup_history(s->tx_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
        gossip13_cleanup_history(s->rx_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
        gossip13_cleanup_history(s->observed_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
        gossip13_cleanup_history(s->total_tx_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
        gossip13_cleanup_history(s->nodes_history, MAX_UPDATE_INTERVAL_MS*MAX_UPDATES_TO_KEEP);
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return DESSERT_PER_KEEP;
}

/** Increase observation counter in a history
 *
 */
inline void gossip13_inc_observations(history_t* hist, uint32_t add);
void gossip13_inc_observations(history_t* hist, uint32_t add) {
    if(hist->observations == NULL) {
        dessert_warn("history not initialized!");
        return;
    }
    pthread_rwlock_wrlock(&(hist->lock));
    hist->observations->packets += add;
    pthread_rwlock_unlock(&(hist->lock));
}

/*** restart ***/

/** Retransmit a packet and schedule next restart
 *
 * Retransmits a packet and schedules the next restart if the
 * maximum number of retransmissions was not reached.
 *
 * @param data pointer to the stored packet
 * @param DESSERT_PER_UNREGISTER if max. number of retransmissions reached, else DESSERT_PER_KEEP
 */
dessert_per_result_t gossip13_restart(void *data, struct timeval *scheduled, struct timeval *interval) {
    gossip13_stored_t* store = (gossip13_stored_t*) data;
    addSeq(store->msg);
    dessert_meshsend(store->msg, NULL);
    store->remaining_restarts--;
    if(store->remaining_restarts == 0) {
        dessert_msg_destroy(store->msg);
        free(store);
        return DESSERT_PER_UNREGISTER;
    }
    return DESSERT_PER_KEEP;
}

/** Store a packet for retransmission
 *
 * @param msg the packet to store
 * @param keep true if message does not have to be cloned, else false
 */
void gossip13_schedule_retransmission(dessert_msg_t *msg, bool keep) {
    if(gossip != gossip_13) {
        dessert_warn("gossip13 not enabled");
        return;
    }
    if(restarts <= 0) {
        return;
    }

    dessert_msg_t* clone = NULL;
    if(!keep) {
        if(dessert_msg_clone(&clone, msg, false) != DESSERT_OK) {
            dessert_crit("could not clone message");
            return;
        }
    }
    else {
        clone = msg;
    }

    gossip13_stored_t* store = malloc(sizeof(gossip13_stored_t));
    if(store == NULL) {
        dessert_crit("could allocate memory");
        dessert_msg_destroy(clone);
        return;
    }

    store->msg = clone;
    store->remaining_restarts = restarts;

    struct timeval scheduled;
    gettimeofday(&scheduled, NULL);
    TIMEVAL_ADD(&scheduled, gossip13_restart_interval.tv_sec, gossip13_restart_interval.tv_usec);
    dessert_periodic_add(gossip13_restart, store, &scheduled, &gossip13_restart_interval);
}

/** Try to synchronize the update schedules
 *
 * The update schedule of the daemon with the lowest sysif MAC wins.
 *
 * @param shost sysif MAC from the received update packet
 * @param update interval signaled in the update packet
 */
void gossip13_adapt_schedule(u_char* shost, uint32_t update) {
    if(!gossip13_unify_schedule) {
        return;
    }

    // initialization
    if((gossip13_schedule.mac[0] | gossip13_schedule.mac[1] | gossip13_schedule.mac[2] | gossip13_schedule.mac[3] | gossip13_schedule.mac[4] | gossip13_schedule.mac[5]) == 0) {
        gossip13_write_sysaddr();
    }

    if(memcmp(shost, gossip13_schedule.mac, ETHER_ADDR_LEN) == 0) { // rx update from schedule leader
        gettimeofday(&(gossip13_schedule.last_rx_update), NULL);
        gossip13_schedule.update_ms = update;
    }
    else if(memcmp(shost, gossip13_schedule.mac, ETHER_ADDR_LEN) >= 0) { // the larger MAC wins
        memcpy(gossip13_schedule.mac, shost, ETHER_ADDR_LEN);
        gettimeofday(&(gossip13_schedule.last_rx_update), NULL);
        gossip13_schedule.update_ms = update;
        dessert_notice("adapted to schedule of " MAC, EXPLODE_ARRAY6(gossip13_schedule.mac));
    }
    else {
        return;
    }
    dessert_periodic_del(gossip13_task_update);
    gossip13_task_update = NULL;
    dessert_ms2timeval(update, &gossip13_update_interval);
    gossip13_send_update(); // trigger the next update now...
    gossip13_task_update = dessert_periodic_add(gossip13_update_data, NULL, NULL, &gossip13_update_interval); // ...and subsequent later
}

/** Evaluate received update packet
 *
 */
dessert_cb_result gossip13_eval_update(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
    if(gossip == gossip_13) {
        u_char* shost = dessert_msg_getl25ether(msg)->ether_shost;
        dessert_ext_t* ext = NULL;
        if(dessert_msg_getext(msg, &ext, GOSSIP13_UPDATE, 0)) {
            seqlog_t* s;
            uint32_t cur_ms = dessert_cur_ms();
            gossip_ext_update_t* update_data = (gossip_ext_update_t*) ext->data;
            uint32_t time_ms = cur_ms - update_data->observation_time_ms;
            /*###*/
            pthread_rwlock_rdlock(&seqlog_lock);
            HASH_FIND(hh, seqlog, shost, ETHER_ADDR_LEN, s);
            if(s) {
                gossip13_register_observation(s->tx_history, update_data->tx_packets, time_ms, update_data->observation_time_ms);
                gossip13_register_observation(s->rx_history, update_data->rx_packets, time_ms, update_data->observation_time_ms);
                gossip13_register_observation(s->total_tx_history, update_data->tx_packets*(update_data->restarts+1), time_ms, update_data->observation_time_ms);
                gossip13_register_observation(s->nodes_history, update_data->nodes, time_ms, update_data->observation_time_ms);
                s->tau = update_data->tau;
                gossip13_register_observation_mean(s->mean_dist_history, update_data->mean_dist, update_data->nodes, time_ms, update_data->observation_time_ms);
                gossip13_register_observation_mean(s->eccentricity_history, update_data->eccentricity, update_data->nodes, time_ms, update_data->observation_time_ms);
                s->update_time_ms = update_data->update_time_ms;
            }
            else {
                dessert_warn("found no entry in sequence log");
            }
            pthread_rwlock_unlock(&seqlog_lock);
            /*###*/
            gossip13_adapt_schedule(shost, update_data->update_time_ms);
//             dessert_notice("MAC="MAC ", tx=%u, rx=%u, time=%u", EXPLODE_ARRAY6(shost), update_data->tx_packets, update_data->rx_packets, time_ms);
        }
    }

    return DESSERT_MSG_KEEP;
}

/** Check for seq duplicates and increase rx counter
 *
 * @return true if duplicate, else false
 */
bool gossip13_rx_packet(seqlog_t* s, dessert_msg_t* msg, bool init) {
    if(gossip != gossip_13) {
        return false;
    }

    dessert_ext_t* ext = NULL;
    if(dessert_msg_getext(msg, &ext, EXT_HOPS, 0)) {
        uint32_t seq2 = ((gossip_ext_hops_t*) ext->data)->seq2;
        uint16_t i;
        if(!init) {
            for(i=0; i < LOG_SIZE; i++) {
                if(s->seqs2[i] == seq2) {
                    return true;
                }
            }
            s->seqs2[s->next_seq2] = seq2;
            s->next_seq2++;
            if(s->next_seq2 >= LOG_SIZE) {
                s->next_seq2 = 0;
            }
        }
        else { // initialize seq2 list
            for(i=0; i < LOG_SIZE; i++) {
                s->seqs2[i] = seq2;
            }
            s->next_seq2 = 1;
        }
        gossip13_inc_observations(s->observed_history, 1);
        gossip13_inc_observations(&gossip13_rx_history, 1);
        gossip13_register_hop(&gossip13_hops_history, ((gossip_ext_hops_t*) ext->data)->hops);
        gossip13_register_hop(s->my_hops_history, ((gossip_ext_hops_t*) ext->data)->hops);
    }
    else {
        dessert_warn("gossip mode = gossip_13 but not matching extension");
    }
    return false;
}

/** Estimate the network state
 *
 */
network_state_t gossip13_network_state() {
    /*
     * network is overloaded if:
     *      - distances between nodes increased
     *      - delivery ratio of all nodes is low
     *      - update packets are not received
     *      - tx_total is high
     *
     * network is underloaded if:
     *      - distances between nodes is "normal"
     *      - delivery ratio of all nodes is low
     *      - update packet are not received
     *      - tx_total is low
     *
     * network is ok if:
     *      - distances between nodes is "normal"
     *      - delivery ratio of all nodes is high
     *      - update packets are received
     *      - tx_total is low or "normal"
     */
    #define NUM_DATAPOINTS  4
    #define NUM_ESTIMATION  3
    uint16_t i;
    uint32_t cur_ms = dessert_cur_ms();

    double rx_frac[NUM_DATAPOINTS];
    double load[NUM_DATAPOINTS];
    double nodes[NUM_DATAPOINTS];
    double distance[NUM_DATAPOINTS];
    double eccentricity[NUM_DATAPOINTS];
    uint32_t times[NUM_DATAPOINTS*2];

    for(i=0; i < NUM_DATAPOINTS; i++) {
        uint32_t start_ms = cur_ms - (i*NUM_ESTIMATION*MAX_UPDATE_INTERVAL_MS) - (NUM_ESTIMATION*MAX_UPDATE_INTERVAL_MS);
        uint32_t end_ms = cur_ms - (i*NUM_ESTIMATION*MAX_UPDATE_INTERVAL_MS);
        times[i*2] = start_ms / 1000;
        times[i*2 +1] = end_ms / 1000;

        double rx = gossip13_get_observations(&gossip13_rx_history, start_ms, end_ms, NULL);
        double tx = gossip13_get_tx_packets(start_ms, end_ms);
        double tx_total_from_the_others = gossip13_get_total_packets(start_ms, end_ms);
        double tx_total_from_me = gossip13_get_observations(&gossip13_txtotal_history, start_ms, end_ms, NULL);
        double n = gossip13_get_num_nodes(start_ms, end_ms);
        double dist = gossip13_get_observations_mean(&gossip13_hops_history, start_ms, end_ms, NULL);
        double ecc = gossip13_get_eccentricity(start_ms, end_ms);

        rx_frac[i] = rx/tx;
        load[i] = tx_total_from_the_others + tx_total_from_me;
        nodes[i] = n;
        distance[i] = dist;
        eccentricity[i] = ecc;
    }
    dessert_notice("time:         [(%3u, %3u), (%3u, %8u), (%3u, %3u), (%3u, %3u)]", times[1]-times[0], times[1]-times[1], times[1]-times[2], times[1]-times[3], times[1]-times[4], times[1]-times[5], times[1]-times[6], times[1]-times[7]);
    dessert_notice("rx_frac:      [%8.6f, %8.6f, %8.6f, %8.6f]", rx_frac[0], rx_frac[1], rx_frac[2], rx_frac[3]);
    dessert_notice("load:         [%8.1f, %8.1f, %8.1f, %8.1f]", load[0], load[1], load[2], load[3]);
    dessert_notice("nodes:        [%8.6f, %8.6f, %8.6f, %8.6f]", nodes[0], nodes[1], nodes[2], nodes[3]);
    dessert_notice("distance:     [%8.6f, %8.6f, %8.6f, %8.6f]", distance[0], distance[1], distance[2], distance[3]);
    dessert_notice("eccentricity: [%8.6f, %8.6f, %8.6f, %8.6f]", eccentricity[0], eccentricity[1], eccentricity[2], eccentricity[3]);

    return network_ok;
}

/** Calculate new restart interval
 *
 * Calculate new restart interval based on the current deadline and the mean delivery ratio.
 * We assume that the network is not overloaded and we can emit any number of packets per time unit.
 *
 * \todo extend and consider delivery ratios of all other nodes
 */
void gossip13_update_restarts() {
    uint32_t cur_ms = dessert_cur_ms();
    uint32_t rx = gossip13_get_observations(&gossip13_rx_history, cur_ms - MAX_UPDATE_INTERVAL_MS, cur_ms, NULL);
    uint32_t tx = gossip13_get_tx_packets(cur_ms - MAX_UPDATE_INTERVAL_MS, cur_ms);
    if(rx == 0 || tx == 0) {
        dessert_warn("cannot update parameters: no packets received, rx=%u, tx=%u", rx, tx);
        return;
    }
    double pdr = ((double) rx)/tx; // too simple
    uint8_t req_restarts = ceil(1.0/pdr); // be pessimistic
    uint16_t req_tau = floor(((double) gossip13_deadline_ms) / req_restarts); // be pessimistic
    /** \todo consider state of the network: sane, overloaded, etc **/
    gossip13_network_state();
    dessert_notice("proposing restarts=%d, tau=%d", req_restarts, req_tau);
}

dessert_per_result_t gossip13_update_data(void *data, struct timeval *scheduled, struct timeval *interval) {
    gossip13_send_update();
    /** \todo update parameters based on observations **/
    gossip13_update_restarts();
    return DESSERT_PER_KEEP;
}

/** Write next sequence number seq2 and increase tx counter
 */
void gossip13_seq2(gossip_ext_hops_t* ext) {
    pthread_mutex_lock(&seq2_mutex);
    ext->seq2 = next_seq2++;
    pthread_mutex_unlock(&seq2_mutex);
    gossip13_inc_observations(&gossip13_tx_history, 1);
    gossip13_inc_observations(&gossip13_txtotal_history, restarts+1);
}

/** Add extension with information about the last update interval
 *
 */
void gossip13_add_ext(dessert_msg_t* msg) {
    dessert_ext_t  *ext;
    dessert_msg_addext(msg, &ext, GOSSIP13_UPDATE, sizeof(gossip_ext_update_t));
    gossip_ext_update_t* rx_data = (gossip_ext_update_t*) ext->data;

    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    uint32_t cur_ms = dessert_timeval2ms(&curtime);
    uint32_t update_ms = cur_ms - last_update_ms; // duration of the observation
    rx_data->tx_packets = gossip13_get_observations(&gossip13_tx_history, cur_ms - update_ms, cur_ms, NULL); // increment to include this packet
    rx_data->tau = dessert_timeval2ms(&gossip13_restart_interval);
    rx_data->restarts = restarts;
    rx_data->update_time_ms = dessert_timeval2ms(&gossip13_update_interval);
    rx_data->observation_time_ms = update_ms;
    rx_data->rx_packets = gossip13_get_observations(&gossip13_rx_history, cur_ms - update_ms, cur_ms, NULL);
    rx_data->mean_dist = gossip13_get_observations_mean(&gossip13_hops_history, cur_ms - update_ms, cur_ms, NULL);
    rx_data->eccentricity = gossip13_get_eccentricity(cur_ms-update_ms, cur_ms);
    rx_data->nodes = gossip13_get_num_nodes(cur_ms-update_ms, cur_ms);
    last_update_ms = cur_ms;
//     dessert_notice("tx=%u, rx=%u", rx_data->tx_packets, rx_data->rx_packets);
}

/** Send a packet with information about the last update interval
 *
 */
void gossip13_send_update() {
    if(!gossip13_send_updates) {
        return;
    }

    struct ether_header *eth;
    dessert_ext_t  *ext;
    dessert_msg_t *msg;

    dessert_msg_new(&msg);
    msg->ttl = 0xFF; // unneccessary
    addSeq(msg);

    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_addext(msg, &ext, EXT_HOPS, sizeof(gossip_ext_hops_t));
    ((gossip_ext_hops_t*) ext->data)->hops = 1;
    gossip13_seq2((gossip_ext_hops_t*) ext->data);

    gossip13_add_ext(msg);
    dessert_msg_dummy_payload(msg, hello_size);

    dessert_meshsend(msg, NULL);
    gossip13_schedule_retransmission(msg, true);

    if(gossip13_unify_schedule) {
        uint32_t cur_ms = dessert_cur_ms();
        uint32_t last_update_ms = dessert_timeval2ms(&(gossip13_schedule.last_rx_update));
        if(_dessert_sysif
            && memcmp(gossip13_schedule.mac, _dessert_sysif->hwaddr, ETHER_ADDR_LEN) != 0
            && cur_ms - last_update_ms > MAX_MISSED_UPDATES * gossip13_schedule.update_ms) {
            dessert_warn("resetting schedule leader");
            gossip13_write_sysaddr(); // re-set MAC
        }
    }
}

/** Calculate a random delay
 *
 */
void inline gossip13_delayed_schedule(struct timeval* scheduled, struct timeval* interval) {
    struct timeval delay;
    gettimeofday(scheduled, NULL);
    uint32_t delay_ms = ((double)random()/RAND_MAX) * dessert_timeval2ms(interval);
    dessert_ms2timeval(delay_ms, &delay);
    TIMEVAL_ADD(scheduled, delay.tv_sec, delay.tv_usec);
}

/*** periodic emission of data ***/

dessert_per_result_t gossip13_generate_data(void *data, struct timeval *scheduled, struct timeval *interval) {
    struct ether_header *eth;
    dessert_ext_t  *ext;
    dessert_msg_t *msg;

    dessert_msg_new(&msg);
    msg->ttl = 0xFF; // unneccessary
    addSeq(msg);

    dessert_msg_addext(msg, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
    eth = (struct ether_header*) ext->data;
    memcpy(eth->ether_shost, dessert_l25_defsrc, ETHER_ADDR_LEN);
    memcpy(eth->ether_dhost, ether_broadcast, ETHER_ADDR_LEN);

    dessert_msg_addext(msg, &ext, EXT_HOPS, sizeof(gossip_ext_hops_t));
    ((gossip_ext_hops_t*) ext->data)->hops = 1;
    gossip13_seq2((gossip_ext_hops_t*) ext->data);

    dessert_msg_dummy_payload(msg, hello_size);

    dessert_meshsend(msg, NULL);
    gossip13_schedule_retransmission(msg, true);

    return DESSERT_PER_KEEP;
}

inline void gossip13_activate_emission() {
    if(gossip13_emission_interval.tv_sec > 0 || gossip13_emission_interval.tv_usec > 0) {
        struct timeval scheduled;
        gossip13_delayed_schedule(&scheduled, &gossip13_emission_interval);
        gossip13_task_emission = dessert_periodic_add(gossip13_generate_data, NULL, &scheduled, &gossip13_emission_interval);
    }
    else {
        gossip13_task_emission = NULL;
    }
}

inline void gossip13_deactivate_emission() {
    if(gossip13_task_emission) {
        dessert_periodic_del(gossip13_task_emission);
        gossip13_task_emission = NULL;
    }
}

/*** start and stop ***/

void gossip13_start() {
    if(!gossip13_task_update) {
        gossip13_write_sysaddr();
        if(gossip13_tx_history.observations == NULL) {
            /* ### */
            pthread_rwlock_wrlock(&(gossip13_tx_history.lock));
            gossip13_new_observation(&(gossip13_tx_history.observations));
            pthread_rwlock_unlock(&(gossip13_tx_history.lock));
        }
        if(gossip13_rx_history.observations == NULL) {
            /* ### */
            pthread_rwlock_wrlock(&(gossip13_rx_history.lock));
            gossip13_new_observation(&(gossip13_rx_history.observations));
            pthread_rwlock_unlock(&(gossip13_rx_history.lock));
        }
        if(gossip13_hops_history.observations == NULL) {
            pthread_rwlock_wrlock(&(gossip13_hops_history.lock));
            gossip13_new_observation_mean(&(gossip13_hops_history.observations));
            pthread_rwlock_unlock(&(gossip13_hops_history.lock));
        }
        if(gossip13_txtotal_history.observations == NULL) {
            /* ### */
            pthread_rwlock_wrlock(&(gossip13_txtotal_history.lock));
            gossip13_new_observation(&(gossip13_txtotal_history.observations));
            pthread_rwlock_unlock(&(gossip13_txtotal_history.lock));
        }
        struct timeval curtime;
        gettimeofday(&curtime, NULL);
        last_update_ms = dessert_timeval2ms(&curtime);
        struct timeval scheduled;

        gossip13_delayed_schedule(&scheduled, &gossip13_update_interval);
        gossip13_task_update = dessert_periodic_add(gossip13_update_data, NULL, &scheduled, &gossip13_update_interval);

        gossip13_delayed_schedule(&scheduled, &gossip13_observations_interval);
        gossip13_task_observations = dessert_periodic_add(gossip13_store_observations, NULL, &scheduled, &gossip13_observations_interval);

        gossip13_delayed_schedule(&scheduled, &gossip13_cleanup_interval);
        gossip13_task_cleanup = dessert_periodic_add(gossip13_cleanup_observations, NULL, &scheduled, &gossip13_cleanup_interval);

        gossip13_activate_emission();
        dessert_info("started gossip13 tasks");
        return;
    }
    dessert_warn("gossip13 tasks already started");
}

void gossip13_stop() {
    if(gossip13_task_update) {
        gossip13_deactivate_emission();
        dessert_periodic_del(gossip13_task_update);
        gossip13_task_update = NULL;
        dessert_periodic_del(gossip13_task_observations);
        gossip13_task_observations = NULL;
        dessert_periodic_del(gossip13_task_cleanup);
        gossip13_task_cleanup = NULL;
        dessert_info("stopped gossip13 tasks");
        return;
    }
    dessert_info("gossip13 tasks were not started");
}

/*** CLI Commands ***/

void _gossip13_show_history(struct cli_def *cli, history_t* hist) {
    struct timeval curtime;
    observation_t* obs;
    /*###*/
    pthread_rwlock_rdlock(&(hist->lock));
    gettimeofday(&curtime, NULL);
    uint32_t cur_ms = dessert_timeval2ms(&curtime);
    LL_FOREACH(hist->observations, obs) {
        uint32_t end_ms = dessert_timeval2ms(&(obs->end));
        uint32_t start_ms = dessert_timeval2ms(&(obs->start));
        if(end_ms == 0) {
            end_ms = cur_ms;
        }

        cli_print(cli, "[%10d - %10d], obs = %10u", start_ms-cur_ms, end_ms-cur_ms, obs->packets);
    }
    pthread_rwlock_unlock(&(hist->lock));
    /*###*/
}

void _gossip13_show_history_mean(struct cli_def *cli, history_mean_t* hist) {
    struct timeval curtime;
    observation_mean_t* obs;
    /*###*/
    pthread_rwlock_rdlock(&(hist->lock));
    gettimeofday(&curtime, NULL);
    uint32_t cur_ms = dessert_timeval2ms(&curtime);
    LL_FOREACH(hist->observations, obs) {
        uint32_t end_ms = dessert_timeval2ms(&(obs->end));
        if(end_ms == 0) {
            end_ms = cur_ms;
        }
        cli_print(cli, "[%10d - %10d], mean = %10f, values=%10u", dessert_timeval2ms(&(obs->start))-cur_ms, end_ms-cur_ms, obs->mean, obs->count);
    }
    pthread_rwlock_unlock(&(hist->lock));
    /*###*/
}

int gossip13_show_tx_history(struct cli_def *cli, char *command, char *argv[], int argc) {
    _gossip13_show_history(cli, &gossip13_tx_history);
    return CLI_OK;
}

int gossip13_show_rx_history(struct cli_def *cli, char *command, char *argv[], int argc) {
    _gossip13_show_history(cli, &gossip13_rx_history);
    return CLI_OK;
}

int gossip13_show_hops_history(struct cli_def *cli, char *command, char *argv[], int argc) {
    _gossip13_show_history_mean(cli, &gossip13_hops_history);
    return CLI_OK;
}

int gossip13_show_data(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc <= 0) {
        return CLI_ERROR;
    }
    char* shost_str = argv[0];
    mac_addr shost;
    dessert_parse_mac2(shost_str, &shost);

    seqlog_t* s;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    HASH_FIND(hh, seqlog, shost, ETHER_ADDR_LEN, s);
    if(s) {
//         history_t* hist = s->observed_history;
//         _gossip13_show_history(cli, hist);
        _gossip13_show_history(cli, s->rx_history);
    }
    else {
        cli_print(cli, "found no entry in sequence log");
    }
    pthread_rwlock_unlock(&seqlog_lock);
    return CLI_OK;
}

int gossip13_stats(struct cli_def *cli, char *command, char *argv[], int argc) {
    #define FLOAT "%7.3f"
    seqlog_t* s;
    uint32_t cur_ms = dessert_cur_ms();
    uint32_t update_ms = MAX_UPDATE_INTERVAL_MS;
    /*###*/
    pthread_rwlock_rdlock(&seqlog_lock);
    uint16_t i;
    cli_print(cli,"epoch [0, %u]\n", MAX_UPDATE_INTERVAL_MS);
    cli_print(cli, "%3s   %17s   %5s   %7s   %7s | %8s   %7s   %17s   %17s   %5s   %5s   %6s   %10s", "#", "MAC", "dist", "obs", "frac", "update", "rx", "tx", "totaltx", "dist", "ecc", "nodes", "age");
    for(s=seqlog, i=0; s != NULL; s=s->hh.next, i++) {
        double tx_total = gossip13_get_observations(s->total_tx_history, cur_ms - update_ms, cur_ms, NULL);
        double quality_rx = 0;
        double rx = gossip13_get_observations(s->rx_history, cur_ms - update_ms, cur_ms, &quality_rx);
        double obs = gossip13_get_observations(s->observed_history, cur_ms - update_ms, cur_ms, NULL);
        double quality_tx = 0;
        double tx = gossip13_get_observations(s->tx_history, cur_ms - update_ms, cur_ms, &quality_tx);
        double dist = gossip13_get_observations_mean(s->my_hops_history, cur_ms - update_ms, cur_ms, NULL);
        uint8_t nodes = gossip13_get_max(s->nodes_history, cur_ms-update_ms, cur_ms);
        uint32_t age_ms = gossip13_get_age(s->tx_history);
        double ecc = gossip13_get_observations_mean(s->eccentricity_history, cur_ms - update_ms, cur_ms, NULL);
        double mean_dist = gossip13_get_observations_mean(s->mean_dist_history, cur_ms - update_ms, cur_ms, NULL);
        cli_print(cli, "%3d   " MAC "   %5.2f   " FLOAT "   " FLOAT " | %8d   " FLOAT " (" FLOAT ")   " FLOAT " (" FLOAT ")   " FLOAT "   %5.2f   %5.2f   %6d   %10u", i, EXPLODE_ARRAY6(s->addr), dist, obs, ((float)obs)/tx, s->update_time_ms, rx, quality_rx, tx, quality_tx, tx_total, mean_dist, ecc, nodes, age_ms);
    }
    pthread_rwlock_unlock(&seqlog_lock);
    /*###*/
    return CLI_OK;
}

int gossip13_mystats(struct cli_def *cli, char *command, char *argv[], int argc) {
    #undef FLOAT
    #define FLOAT "%14.3f"
    uint32_t durations[] = {0, 1*MAX_UPDATE_INTERVAL_MS, 2*MAX_UPDATE_INTERVAL_MS, 3*MAX_UPDATE_INTERVAL_MS, 4*MAX_UPDATE_INTERVAL_MS, 5*MAX_UPDATE_INTERVAL_MS};
    uint16_t i;
    uint32_t cur_ms = dessert_cur_ms();

    double tx_per_ms = 1.0/dessert_timeval2ms(&gossip13_emission_interval);
    double updates_per_ms = 1.0/dessert_timeval2ms(&gossip13_update_interval);

    cli_print(cli, "\n%12s   %15s   %14s   %14s   %14s" , "[tx]", "epoch [s]", "obs", "frac", "confidence");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        double quality = 0;
        double obs = gossip13_get_observations(&gossip13_tx_history, start_ms, end_ms, &quality);
        double frac = obs/((tx_per_ms*MAX_UPDATE_INTERVAL_MS)+(updates_per_ms*MAX_UPDATE_INTERVAL_MS));
        cli_print(cli, "%12s   %6u - %6u   " FLOAT "   " FLOAT "   " FLOAT,
            "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, obs, frac, quality);
    }

    cli_print(cli, "\n%12s   %15s   %14s   %14s   %14s" , "[rx]", "epoch [s]", "obs", "frac", "confidence");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        double quality = 0;
        double obs = gossip13_get_observations(&gossip13_rx_history, start_ms, end_ms, &quality);
        double tx = gossip13_get_tx_packets(cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS, cur_ms - durations[i]);
        cli_print(cli, "%12s   %6u - %6u   " FLOAT "   " FLOAT "   " FLOAT,
            "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, obs, obs/tx, quality);
    }

    cli_print(cli, "\n%12s   %15s   %14s" , "[txtotal]", "epoch [s]", "obs");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        double obs = gossip13_get_total_packets(start_ms, end_ms);
        double tx = gossip13_get_observations(&gossip13_txtotal_history, start_ms, end_ms, NULL);
        cli_print(cli, "%12s   %6u - %6u   " FLOAT,
            "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, obs+tx);
    }

    cli_print(cli, "\n%12s   %15s   %14s" , "[nodes]", "epoch [s]", "nodes");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        uint32_t obs = gossip13_get_num_nodes(start_ms, end_ms);
        cli_print(cli, "%12s   %6u - %6u   %14u", "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, obs);
    }

    cli_print(cli, "\n%12s   %15s   %14s" , "[hops]", "epoch [s]", "distance");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        double obs = gossip13_get_observations_mean(&gossip13_hops_history, start_ms, end_ms, NULL);
        cli_print(cli, "%12s   %6u - %6u   " FLOAT, "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, obs);
    }

    cli_print(cli, "\n%12s   %15s   %14s" , "[eccentr.]", "epoch [s]", "distance");
    for(i=0; i < sizeof(durations)/sizeof(durations[0]); i++) {
        uint32_t start_ms = cur_ms - durations[i]-MAX_UPDATE_INTERVAL_MS;
        uint32_t end_ms = cur_ms - durations[i];
        double ecc = gossip13_get_eccentricity(start_ms, end_ms);
        cli_print(cli, "%12s   %6u - %6u   " FLOAT, "", durations[i]/1000, (durations[i]+MAX_UPDATE_INTERVAL_MS)/1000, ecc);
    }

    return CLI_OK;
}

int gossip13_set_deadline(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..65535]\n", command);
        return CLI_ERROR;
    }
    uint16_t new = (uint16_t) strtoul(argv[0], NULL, 10);
    if(new == 0) {
        cli_print(cli, "ERROR: zero deadline or parsing failed: %s", argv[0]);
        return CLI_ERROR;
    }
    gossip13_deadline_ms = new;
    cli_print(cli, "set deadline to: %u ms\n", gossip13_deadline_ms);
    dessert_notice("set deadline to: %u ms\n", gossip13_deadline_ms);
    return CLI_OK;
}

int gossip13_show_deadline(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u ms\n", gossip13_deadline_ms);
    return CLI_OK;
}

int gossip13_set_restarts(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..255]\n", command);
        return CLI_ERROR;
    }
    uint8_t new = (uint8_t) strtoul(argv[0], NULL, 10);
    restarts = new;
    return CLI_OK;
}

int gossip13_show_restarts(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u\n", restarts);
    return CLI_OK;
}

int gossip13_set_tau(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..65535]\n", command);
        return CLI_ERROR;
    }
    uint16_t new = (uint16_t) strtoul(argv[0], NULL, 10);
    if(new > 0) {
        dessert_ms2timeval(new, &gossip13_restart_interval);
        cli_print(cli, "set restart interval to: %u ms\n", dessert_timeval2ms(&gossip13_restart_interval));
        dessert_notice("set restart interval to: %u ms", dessert_timeval2ms(&gossip13_restart_interval));
    }
    return CLI_OK;
}

int gossip13_show_tau(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u ms\n", dessert_timeval2ms(&gossip13_restart_interval));
    return CLI_OK;
}

int gossip13_show_send_updates(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%s\n", gossip13_send_updates ? "true" : "false");
    return CLI_OK;
}

int gossip13_set_send_updates(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(!strncmp("true", argv[0], 4)  || !strncmp("on", argv[0], 4) || !strncmp("1", argv[0], 4)) {
        gossip13_send_updates = true;
    }
    else if(!strncmp("false", argv[0], 4) || !strncmp("off", argv[0], 4) || !strncmp("0", argv[0], 4)) {
        gossip13_send_updates = false;
    }
    cli_print(cli, "update packets %s\n", gossip13_send_updates ? "activated" : "deactivated");
    return CLI_OK;
}


int gossip13_set_update(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..65535]\n", command);
        return CLI_ERROR;
    }
    uint16_t new = (uint16_t) strtoul(argv[0], NULL, 10);
    if(new > 0) {
        if(new > MAX_UPDATE_INTERVAL_MS) {
            cli_print(cli, "update interval to large: limited to %u ms\n", MAX_UPDATE_INTERVAL_MS);
            dessert_warn("update interval to large: limited to %u ms", dessert_timeval2ms(&gossip13_update_interval));
            new = MAX_UPDATE_INTERVAL_MS;
        }
        dessert_ms2timeval(new, &gossip13_update_interval);
        cli_print(cli, "set update interval to: %u ms\n", dessert_timeval2ms(&gossip13_update_interval));
        dessert_notice("set update interval to: %u ms", dessert_timeval2ms(&gossip13_update_interval));
        if(gossip13_task_update) {
            dessert_periodic_del(gossip13_task_update);
            struct timeval curtime;
            gettimeofday(&curtime, NULL);
            last_update_ms = dessert_timeval2ms(&curtime);

            struct timeval scheduled;
            gossip13_delayed_schedule(&scheduled, &gossip13_update_interval);
            gossip13_task_update = dessert_periodic_add(gossip13_update_data, NULL, &scheduled, &gossip13_update_interval);
        }
    }
    else {
        cli_print(cli, "invalid update interval");
        dessert_warn("invalid update interval");
    }
    return CLI_OK;
}

int gossip13_show_update(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u ms\n", dessert_timeval2ms(&gossip13_update_interval));
    return CLI_OK;
}

int gossip13_set_observation(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..65535]\n", command);
        return CLI_ERROR;
    }
    uint16_t new = (uint16_t) strtoul(argv[0], NULL, 10);
    if(new > 0) {
        dessert_ms2timeval(new, &gossip13_observations_interval);

        pthread_rwlock_wrlock(&(gossip13_tx_history.lock));
        pthread_rwlock_unlock(&(gossip13_tx_history.lock));
        pthread_rwlock_wrlock(&(gossip13_rx_history.lock));
        pthread_rwlock_unlock(&(gossip13_rx_history.lock));

        cli_print(cli, "set observation interval to: %u ms\n", dessert_timeval2ms(&gossip13_observations_interval));
        dessert_notice("set observation interval to: %u ms\n", dessert_timeval2ms(&gossip13_observations_interval));
        if(gossip13_task_observations) {
            dessert_periodic_del(gossip13_task_observations);
            struct timeval scheduled;
            gossip13_delayed_schedule(&scheduled, &gossip13_observations_interval);
            gossip13_task_observations = dessert_periodic_add(gossip13_store_observations, NULL, &scheduled, &gossip13_observations_interval);
        }
    }
    return CLI_OK;
}

int gossip13_show_observation(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u ms\n", dessert_timeval2ms(&gossip13_observations_interval));
    return CLI_OK;
}

int gossip13_set_emission(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [0..65535]\n", command);
        return CLI_ERROR;
    }

    if(gossip13_task_emission) {
        dessert_periodic_del(gossip13_task_emission);
        gossip13_task_emission = NULL;
    }
    uint16_t new = (uint16_t) strtoul(argv[0], NULL, 10);
    dessert_ms2timeval(new, &gossip13_emission_interval);
    cli_print(cli, "set emission interval to: %u ms\n", dessert_timeval2ms(&gossip13_emission_interval));
    dessert_notice("set emission interval to: %u ms\n", dessert_timeval2ms(&gossip13_emission_interval));

    if(gossip == gossip_13 && new > 0) {
        struct timeval scheduled;
        gossip13_delayed_schedule(&scheduled, &gossip13_emission_interval);
        gossip13_task_emission = dessert_periodic_add(gossip13_generate_data, NULL, &scheduled, &gossip13_emission_interval);
    }
    return CLI_OK;
}

int gossip13_show_emission(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%u ms\n", dessert_timeval2ms(&gossip13_emission_interval));
    return CLI_OK;
}

int gossip13_set_seq2(struct cli_def *cli, char *command, char *argv[], int argc) {
    if(argc != 1) {
        cli_print(cli, "usage %s [true, false]\n", command);
        return CLI_ERROR;
    }
    if(strcmp(argv[0], "true") == 0) {
        gossip13_drop_seq2_duplicates = true;
    }
    else if(strcmp(argv[0], "false") == 0) {
        gossip13_drop_seq2_duplicates = false;
    }
    else {
        cli_print(cli, "invalid parameter: %s\n", argv[0]);
        return CLI_ERROR;
    }
    return CLI_OK;
}

int gossip13_show_seq2(struct cli_def *cli, char *command, char *argv[], int argc) {
    cli_print(cli, "%s\n", gossip13_drop_seq2_duplicates ? "true" : "false");
    return CLI_OK;
}
#endif
