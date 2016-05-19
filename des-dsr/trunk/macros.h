/******************************************************************************
 Copyright 2009, 2010, David Gutzmann, Freie Universitaet Berlin (FUB).
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

#ifndef MACROS_H_
#define MACROS_H_

#define HASH_FOREACH(hh,head,el)                                               \
    for(el=head;el;el=(el->hh).next)

#define ADDR_IDX_STRUCT(_struct, _index) (((uint8_t *)(_struct.address)) + ((_index) * ETHER_ADDR_LEN))
#define ADDR_IDX(          _ptr, _index) (((uint8_t *)  (_ptr->address)) + ((_index) * ETHER_ADDR_LEN))

#define ADDR_CPY(   _to, _from    ) memcpy( (_to), (_from),        ETHER_ADDR_LEN)
#define ADDR_N_CPY( _to, _from, _n) memcpy( (_to), (_from), (_n) * ETHER_ADDR_LEN)
#define ADDR_CMP(    _a,    _b    ) memcmp(  (_a),    (_b),        ETHER_ADDR_LEN)
#define ADDR_N_CMP(  _a,    _b, _n) memcmp(  (_a),    (_b), (_n) * ETHER_ADDR_LEN)

/** compares two struct timeval - a>=b==1, a<=b==-1 - >= later, <= earlier */
static inline int TIMEVAL_COMPARE(struct timeval* a, struct timeval* b) {
    if(a->tv_sec > b->tv_sec) {
        return 1;
    }
    else if(a->tv_sec < b->tv_sec) {
        return -1;
    }
    else if(a->tv_usec > b->tv_usec) {
        return 1;
    }
    else if(a->tv_usec < b->tv_usec) {
        return -1;
    }

    return 0;
}

static inline void TIMEVAL_ADD_SAFE(struct timeval* __tv, __time_t __sec, __suseconds_t __usec) {
    (__tv)->tv_sec += __sec;
    (__tv)->tv_usec += __usec;

    while((__tv)->tv_usec >= 1000000) {
        ++(__tv)->tv_sec;
        (__tv)->tv_usec -= 1000000;
    }
}

# define TIMEVAL_SUB(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)
#endif /* MACROS_H_ */
