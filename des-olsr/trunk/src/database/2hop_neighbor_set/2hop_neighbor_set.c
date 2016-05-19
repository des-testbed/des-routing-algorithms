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

#include <stdio.h>
#include "../timeslot.h"
#include "../../config.h"
#include "2hop_neighbor_set.h"

typedef struct _1hopn_to_2hopns {
    uint8_t                _1hop_neighbor[ETH_ALEN]; // key
    olsr_2hns_neighbor_t*   _2hop_neighbors;
    timeslot_t*             ts;
    UT_hash_handle          hh;
} _1hopn_to_2hopns_t;

typedef struct _2hopn_to_1hopns {
    uint8_t                _2hop_neighbor[ETH_ALEN]; // key
    olsr_2hns_neighbor_t*   _1hop_neighbors;
    UT_hash_handle          hh;
} _2hopn_to_1hopns_t;

_1hopn_to_2hopns_t*         _1hset_entrys = NULL;
_2hopn_to_1hopns_t*         _2hset_entrys = NULL;


void purge_2h_neighbor(struct timeval* timestamp, void* src_object, void* object) {
    _1hopn_to_2hopns_t* _1hop_tuple = src_object;
    olsr_2hns_neighbor_t* _2h_neighbor = object;

    _2hopn_to_1hopns_t* _2hop_tuple;
    HASH_FIND(hh, _2hset_entrys, _2h_neighbor->ether_addr, ETH_ALEN, _2hop_tuple);

    if(_2hop_tuple != NULL) {
        olsr_2hns_neighbor_t*	_1h_neighbor;
        HASH_FIND(hh, _2hop_tuple->_1hop_neighbors, _1hop_tuple->_1hop_neighbor, ETH_ALEN, _1h_neighbor);
        HASH_DEL(_2hop_tuple->_1hop_neighbors, _1h_neighbor);
        free(_1h_neighbor);

        if(HASH_COUNT(_2hop_tuple->_1hop_neighbors) == 0) {
            HASH_DEL(_2hset_entrys, _2hop_tuple);
            free(_2hop_tuple);
        }
    }

    HASH_DEL(_1hop_tuple->_2hop_neighbors, _2h_neighbor);
    free(_2h_neighbor);

    if(HASH_COUNT(_1hop_tuple->_2hop_neighbors) == 0) {
        HASH_DEL(_1hset_entrys, _1hop_tuple);
        timeslot_destroy(_1hop_tuple->ts);
        free(_1hop_tuple);
    }
}

int neighbor_entry_create(olsr_2hns_neighbor_t** entry_out, uint8_t ether_addr[ETH_ALEN]) {
    olsr_2hns_neighbor_t* entry = malloc(sizeof(olsr_2hns_neighbor_t));

    if(entry == NULL) {
        return false;
    }

    memcpy(entry->ether_addr, ether_addr, ETH_ALEN);
    *entry_out = entry;
    return true;
}

int _1hop_to_2hop_entry_create(_1hopn_to_2hopns_t** entry_out, uint8_t _1hop_neighbor_addr[ETH_ALEN]) {
    _1hopn_to_2hopns_t* entry = malloc(sizeof(_1hopn_to_2hopns_t));

    if(entry == NULL) {
        return false;
    }

    memcpy(entry->_1hop_neighbor, _1hop_neighbor_addr, ETH_ALEN);
    entry->_2hop_neighbors = NULL;
    *entry_out = entry;

    if(timeslot_create(&entry->ts, entry, purge_2h_neighbor) == false) {
        free(entry);
        return false;
    }

    return true;
}

int _2hop_to_1hop_entry_create(_2hopn_to_1hopns_t** entry_out, uint8_t _2hop_neighbor_addr[ETH_ALEN]) {
    _2hopn_to_1hopns_t* entry = malloc(sizeof(_2hopn_to_1hopns_t));

    if(entry == NULL) {
        return false;
    }

    memcpy(entry->_2hop_neighbor, _2hop_neighbor_addr, ETH_ALEN);
    entry->_1hop_neighbors = NULL;
    *entry_out = entry;
    return true;
}

int olsr_db_2hns_add2hneighbor(uint8_t _1hop_neighbor_addr[ETH_ALEN],
                               uint8_t _2hop_neighbor_addr[ETH_ALEN], uint8_t link_quality, struct timeval* purge_time) {
    _1hopn_to_2hopns_t* 	_1hset_entry;
    _2hopn_to_1hopns_t*		_2hset_entry;
    olsr_2hns_neighbor_t*	_1h_neighbor;
    olsr_2hns_neighbor_t*	_2h_neighbor;

    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        if(_1hop_to_2hop_entry_create(&_1hset_entry, _1hop_neighbor_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, _1hset_entrys, _1hset_entry->_1hop_neighbor, ETH_ALEN, _1hset_entry);
    }

    HASH_FIND(hh, _1hset_entry->_2hop_neighbors, _2hop_neighbor_addr, ETH_ALEN, _2h_neighbor);

    if(_2h_neighbor == NULL) {
        if(neighbor_entry_create(&_2h_neighbor, _2hop_neighbor_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, _1hset_entry->_2hop_neighbors, _2h_neighbor->ether_addr, ETH_ALEN, _2h_neighbor);
    }

    _2h_neighbor->link_quality = link_quality;
    timeslot_addobject(_1hset_entry->ts, purge_time, _2h_neighbor);
    HASH_FIND(hh, _2hset_entrys, _2hop_neighbor_addr, ETH_ALEN, _2hset_entry);

    if(_2hset_entry == NULL) {
        if(_2hop_to_1hop_entry_create(&_2hset_entry, _2hop_neighbor_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, _2hset_entrys, _2hset_entry->_2hop_neighbor, ETH_ALEN, _2hset_entry);
    }

    HASH_FIND(hh, _2hset_entry->_1hop_neighbors, _1hop_neighbor_addr, ETH_ALEN, _1h_neighbor);

    if(_1h_neighbor == NULL) {
        if(neighbor_entry_create(&_1h_neighbor, _1hop_neighbor_addr) == false) {
            return false;
        }

        HASH_ADD_KEYPTR(hh, _2hset_entry->_1hop_neighbors, _1h_neighbor->ether_addr, ETH_ALEN, _1h_neighbor);
    }

    _1h_neighbor->link_quality = link_quality;
    timeslot_purgeobjects(_1hset_entry->ts);
    return true;
}

olsr_2hns_neighbor_t* olsr_db_2hns_get2hneighbors(uint8_t _1hop_neighbor_addr[ETH_ALEN]) {
    _1hopn_to_2hopns_t* _1hset_entry;
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return NULL;
    }

    timeslot_purgeobjects(_1hset_entry->ts);

    // search one more time since entry can be deleted
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return NULL;
    }

    return _1hset_entry->_2hop_neighbors;
}

olsr_2hns_neighbor_t* olsr_db_2hns_get1hneighbors(uint8_t _2hop_neighbor_addr[ETH_ALEN]) {
    _2hopn_to_1hopns_t* _2hset_entry;
    HASH_FIND(hh, _2hset_entrys, _2hop_neighbor_addr, ETH_ALEN, _2hset_entry);

    if(_2hset_entry == NULL) {
        return NULL;
    }

    return _2hset_entry->_1hop_neighbors;
}

olsr_2hns_neighbor_t* olsr_db_2hns_get1hnset() {
    olsr_2hns_neighbor_t* _1hn_set = NULL;
    _1hopn_to_2hopns_t* _1h_entry = _1hset_entrys;

    while(_1h_entry != NULL) {
        olsr_2hns_neighbor_t* eth_addr;
        neighbor_entry_create(&eth_addr, _1h_entry->_1hop_neighbor);
        HASH_ADD_KEYPTR(hh, _1hn_set, eth_addr->ether_addr, ETH_ALEN, eth_addr);
        _1h_entry = _1h_entry->hh.next;
    }

    return _1hn_set;
}

olsr_2hns_neighbor_t* olsr_db_2hns_get2hnset() {
    olsr_2hns_neighbor_t* _2hn_set = NULL;
    _2hopn_to_1hopns_t* _2h_entry = _2hset_entrys;

    while(_2h_entry != NULL) {
        olsr_2hns_neighbor_t* eth_addr;
        neighbor_entry_create(&eth_addr, _2h_entry->_2hop_neighbor);
        HASH_ADD_KEYPTR(hh, _2hn_set, eth_addr->ether_addr, ETH_ALEN, eth_addr);
        _2h_entry = _2h_entry->hh.next;
    }

    return _2hn_set;
}

void olsr_db_2hns_del1hneighbor(uint8_t _1hop_neighbor_addr[ETH_ALEN]) {
    _1hopn_to_2hopns_t* _1hset_entry;
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return;
    }

    olsr_2hns_neighbor_t* _2hset_entry_addr = _1hset_entry->_2hop_neighbors;

    while(_2hset_entry_addr != NULL) {
        _2hopn_to_1hopns_t* _2hset_entry;
        HASH_FIND(hh, _2hset_entrys, _2hset_entry_addr->ether_addr, ETH_ALEN, _2hset_entry);

        if(_2hset_entry != NULL) {
            olsr_2hns_neighbor_t* _1hset_entry_addr;
            HASH_FIND(hh, _2hset_entry->_1hop_neighbors, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry_addr);

            if(_1hset_entry_addr != NULL) {
                HASH_DEL(_2hset_entry->_1hop_neighbors, _1hset_entry_addr);
                free(_1hset_entry_addr);
            }

            if(HASH_COUNT(_2hset_entry->_1hop_neighbors) == 0) {
                HASH_DEL(_2hset_entrys, _2hset_entry);
                free(_2hset_entry);
            }
        }

        HASH_DEL(_1hset_entry->_2hop_neighbors, _2hset_entry_addr);
        free(_2hset_entry_addr);
        _2hset_entry_addr = _1hset_entry->_2hop_neighbors;
    }

    HASH_DEL(_1hset_entrys, _1hset_entry);
    timeslot_destroy(_1hset_entry->ts);
    free(_1hset_entry);
}

void olsr_db_2hns_del2hneighbor(uint8_t _2hop_neighbor_addr[ETH_ALEN]) {
    _2hopn_to_1hopns_t* _2hset_entry;
    HASH_FIND(hh, _2hset_entrys, _2hop_neighbor_addr, ETH_ALEN, _2hset_entry);

    if(_2hset_entry == NULL) {
        return;
    }

    olsr_2hns_neighbor_t* _1hset_entry_addr = _2hset_entry->_1hop_neighbors;

    while(_1hset_entry_addr != NULL) {
        _1hopn_to_2hopns_t* _1hset_entry;
        HASH_FIND(hh, _1hset_entrys, _1hset_entry_addr->ether_addr, ETH_ALEN, _1hset_entry);

        if(_1hset_entry != NULL) {
            olsr_2hns_neighbor_t* _2hset_entry_addr;
            HASH_FIND(hh, _1hset_entry->_2hop_neighbors, _2hop_neighbor_addr, ETH_ALEN, _2hset_entry_addr);

            if(_2hset_entry_addr != NULL) {
                HASH_DEL(_1hset_entry->_2hop_neighbors, _2hset_entry_addr);
                timeslot_deleteobject(_1hset_entry->ts, _2hset_entry_addr);
                free(_2hset_entry_addr);
            }

            if(HASH_COUNT(_1hset_entry->_2hop_neighbors) == 0) {
                HASH_DEL(_1hset_entrys, _1hset_entry);
                timeslot_destroy(_1hset_entry->ts);
                free(_1hset_entry);
            }
        }

        HASH_DEL(_2hset_entry->_1hop_neighbors, _1hset_entry_addr);
        free(_1hset_entry_addr);
        _1hset_entry_addr = _2hset_entry->_1hop_neighbors;
    }

    HASH_DEL(_2hset_entrys, _2hset_entry);
    free(_2hset_entry);
}

/**
 * Drops all of old associated 2hop neighbors from this 1hop host
 */
void olsr_db_2hns_clear1hn(uint8_t _1hop_neighbor_addr[ETH_ALEN]) {
    _1hopn_to_2hopns_t* _1hset_entry;
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return;
    }

    timeslot_purgeobjects(_1hset_entry->ts);
}

uint8_t olsr_db_2hns_getlinkquality(uint8_t _1hop_neighbor_addr[ETH_ALEN],
                                    uint8_t _2hop_neighbor_addr[ETH_ALEN]) {
    _1hopn_to_2hopns_t* _1hset_entry;
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return 0;
    }

    timeslot_purgeobjects(_1hset_entry->ts);

    // search one more time since entry can be deleted
    HASH_FIND(hh, _1hset_entrys, _1hop_neighbor_addr, ETH_ALEN, _1hset_entry);

    if(_1hset_entry == NULL) {
        return 0;
    }

    // search for 2hop entry
    olsr_2hns_neighbor_t* _2hop_entry;
    HASH_FIND(hh, _1hset_entry->_2hop_neighbors, _2hop_neighbor_addr, ETH_ALEN, _2hop_entry);

    if(_2hop_entry == NULL) {
        return 0;
    }

    return _2hop_entry->link_quality;
}

// ------------------- reporting -----------------------------------------------

int olsr_db_2hns_report1hto2h(char** str_out) {
    int report_str_len = 42;
    _1hopn_to_2hopns_t* current_entry = _1hset_entrys;
    char* output;
    char entry_str[report_str_len  + 1];

    size_t str_count = 0;

    while(current_entry != NULL) {
        str_count += HASH_COUNT(current_entry->_2hop_neighbors) + 1;
        current_entry = current_entry->hh.next;
    }

    current_entry = _1hset_entrys;

    output = malloc(sizeof(char) * report_str_len * (3 + str_count) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+-------------------+\n");
    strcat(output, "| 1hop-n. main addr | 2hop-n. main addr |\n");
    strcat(output, "+-------------------+-------------------+\n");

    while(current_entry != NULL) {
        olsr_2hns_neighbor_t* neigbors = current_entry->_2hop_neighbors;
        int flag = false;

        while(neigbors != NULL) {
            if(flag == false) {
                snprintf(entry_str, report_str_len + 1, "| " MAC " | " MAC " |\n", EXPLODE_ARRAY6(current_entry->_1hop_neighbor), EXPLODE_ARRAY6(neigbors->ether_addr));
                flag = true;
            }
            else {
                snprintf(entry_str, report_str_len + 1, "|                   | " MAC " |\n", EXPLODE_ARRAY6(neigbors->ether_addr));
            }

            strcat(output, entry_str);
            neigbors = neigbors->hh.next;
        }

        strcat(output, "+-------------------+-------------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return 3 + str_count;
}

int olsr_db_2hns_report2hto1h(char** str_out) {
    int report_str_len = 42;
    _2hopn_to_1hopns_t* current_entry = _2hset_entrys;
    char* output;
    char entry_str[report_str_len  + 1];

    size_t str_count = 0;

    while(current_entry != NULL) {
        str_count += HASH_COUNT(current_entry->_1hop_neighbors) + 1;
        current_entry = current_entry->hh.next;
    }

    current_entry = _2hset_entrys;

    output = malloc(sizeof(char) * report_str_len * (3 + str_count) + 1);

    if(output == NULL) {
        return false;
    }

    // initialize first byte to \0 to mark output as empty
    *output = '\0';
    strcat(output, "+-------------------+-------------------+\n");
    strcat(output, "| 2hop-n. main addr | 1hop-n. main addr |\n");
    strcat(output, "+-------------------+-------------------+\n");

    while(current_entry != NULL) {
        olsr_2hns_neighbor_t* neigbors = current_entry->_1hop_neighbors;
        int flag = false;

        while(neigbors != NULL) {
            if(flag == false) {
                snprintf(entry_str, report_str_len + 1, "| " MAC " | " MAC " |\n", EXPLODE_ARRAY6(current_entry->_2hop_neighbor), EXPLODE_ARRAY6(neigbors->ether_addr));
                flag = true;
            }
            else {
                snprintf(entry_str, report_str_len + 1, "|                   | " MAC " |\n", EXPLODE_ARRAY6(neigbors->ether_addr));
            }

            strcat(output, entry_str);
            neigbors = neigbors->hh.next;
        }

        strcat(output, "+-------------------+-------------------+\n");
        current_entry = current_entry->hh.next;
    }

    *str_out = output;
    return 3 + str_count;
}

int olsr_db_2hns_report(char** str_out) {
    char* _1hto2h, *_2hto1h;
    int _1hstr_count = olsr_db_2hns_report1hto2h(&_1hto2h);
    int _2hstr_count = olsr_db_2hns_report2hto1h(&_2hto1h);

    if(_1hstr_count == 0 || _2hstr_count == 0) {
        if(_2hstr_count == 0) {
            free(_2hto1h);
        }

        if(_1hstr_count == 0) {
            free(_1hto2h);
        }

        return false;
    }

    int report_str_len = 41 + 41 + 2 + 1;
    char* output;
    int report_str_count = (_1hstr_count > _2hstr_count) ? _1hstr_count : _2hstr_count;

    output = malloc(sizeof(char) * report_str_len * report_str_count + 1);

    if(output == NULL) {
        free(_1hto2h);
        free(_2hto1h);
        return false;
    }

    int i = 0;
    int j = 0;
    int x = 0;

    while(_1hto2h[i] != '\0' || _2hto1h[j] != '\0') {
        if(_1hto2h[i] == '\0') {
            memset(output + x, ' ', 41);
            x += 41;
        }
        else {
            memcpy(output + x, _1hto2h + i, 41);
            x += 41;
            i += 42;
        }

        memset(output + x, ' ', 2);
        x += 2;

        if(_2hto1h[j] == '\0') {
            memset(output + x, ' ', 41);
            x += 41;
        }
        else {
            memcpy(output + x, _2hto1h + j, 41);
            x += 41;
            j += 42;
        }

        output[x++] = '\n';
    }

    output[x] = '\0';
    free(_1hto2h);
    free(_2hto1h);
    *str_out = output;
    return true;
}

