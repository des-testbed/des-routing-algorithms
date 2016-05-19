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

#include "helper.h"
#include "config.h"

int hf_seq_comp_i_j(uint16_t i, uint16_t j) {
    if(i == j) {
        return 0;
    }

    uint16_t diff = i - j;

    if(diff < ((SEQNO_MAX) >> 1)) {
        return 1;
    }

    return -1;
}

uint8_t hf_sparce_time(float time) {
    float af = time * C_INV_COEFF;
    uint8_t b = 0;

    while(af >= (1 << 4)) {
        af /= 2;
        b += 1;
    }

    uint8_t a = af - 1;
    return (a << 4) | b;
}

float hf_parse_time(uint8_t time) {
    float a = (time >> 4) & ((1 << 4) - 1);
    uint8_t b = time & ((1 << 4) - 1);
    float x = (1 + a) * (1 << b);
    return x / C_INV_COEFF;
}

int hf_compare_tv(const struct timeval* tv1, const struct timeval* tv2) {
    if((tv1->tv_sec == tv2->tv_sec) && (tv1->tv_usec == tv2->tv_usec)) {
        return 0;
    }

    if(tv1->tv_sec > tv2->tv_sec) {
        return 1;
    }

    if(tv2->tv_sec > tv1->tv_sec) {
        return -1;
    }

    if(tv1->tv_usec > tv2->tv_usec) {
        return 1;
    }
    else {
        return -1;
    }
}

int hf_add_tv(const struct timeval* tv1, const struct timeval* tv2, struct timeval* sum) {
    sum->tv_sec = tv1->tv_sec + tv2->tv_sec;
    __suseconds_t usec_sum = tv1->tv_usec + tv2->tv_usec;

    if(usec_sum >= 1000000) {
        sum->tv_sec += 1;
        sum->tv_usec = usec_sum - 1000000;
    }
    else {
        sum->tv_usec = usec_sum;
    }

    return true;
}

int hf_diff_tv(const struct timeval* tv1, const struct timeval* tv2, struct timeval* diff) {
    diff->tv_sec = tv1->tv_sec - tv2->tv_sec;
    int32_t usec_diff = tv1->tv_usec - tv2->tv_usec;

    if(usec_diff < 0) {
        diff->tv_sec -= 1;
        diff->tv_usec = usec_diff + 1000000;
    }
    else {
        diff->tv_usec = usec_diff;
    }

    return true;
}

int hf_mul_tv(const struct timeval* tv, float x, struct timeval* mul) {
    mul->tv_sec = tv->tv_sec * x + ((int32_t)(tv->tv_usec * x)) % 1000000;
    mul->tv_usec = (tv->tv_usec * x) - ((int32_t)(tv->tv_usec * x)) % 1000000;
    return true;
}
