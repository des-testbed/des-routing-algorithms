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

#include "batman_sw.h"
#include <stdio.h>
#include "../../config.h"
#include "../../helper.h"

int batman_create_new_sw_element(batman_sw_element_t** sw_el_out, uint16_t seq_num) {
    batman_sw_element_t* new_el;

    new_el = malloc(sizeof(batman_sw_element_t));

    if(new_el == NULL) {
        return false;
    }

    new_el->next = NULL;
    new_el->prev = NULL;
    new_el->seq_num = seq_num;
    *sw_el_out = new_el;
    return true;
}

int batman_sw_create(batman_sw_t** swout, uint8_t ws) {
    batman_sw_t* sw;
    sw = malloc(sizeof(batman_sw_t));

    if(sw == NULL) {
        return false;
    }

    sw->head = NULL;
    sw->tail = NULL;
    sw->size = 0;
    sw->window_size = ws;
    *swout = sw;
    return true;
}

int batman_sw_destroy(batman_sw_t* sw) {
    batman_sw_element_t* temp_el;
    batman_sw_element_t* search_el = sw->tail;

    while(search_el != NULL) {
        temp_el = search_el;
        search_el = search_el->next;
        free(temp_el);
    }

    free(sw);
    return true;
};

int batman_sw_dropsn(batman_sw_t* sw, uint16_t seq_num) {
    batman_sw_element_t* search_el = sw->tail;

    while(search_el != NULL && batman_hf_seq_i_graterorequals_j(seq_num, search_el->seq_num + sw->window_size) == true) {
        search_el = search_el->next;

        if(search_el != NULL) {
            search_el->prev = NULL;
        }

        free(sw->tail);

        if(sw->tail == sw->head) {
            sw->tail = sw->head = search_el;
        }
        else {
            sw->tail = search_el;
        }

        sw->size--;
    }

    return true;
}

int batman_sw_addsn(batman_sw_t* sw, uint16_t seq_num) {
    batman_sw_element_t* new_el;

    if((sw->head != NULL) &&
       (batman_hf_seq_i_graterorequals_j(sw->head->seq_num, seq_num + sw->window_size) == true)) {
        return true;
    }

    if(sw->size == 0) {
        if(batman_create_new_sw_element(&new_el, seq_num) == false) {
            return false;
        }

        sw->head = sw->tail = new_el;
        sw->size = 1;
        return true;
    }

    // insert new element to appropriate place
    batman_sw_element_t* search_el = sw->head;

    while(search_el->prev != NULL && batman_hf_seq_i_graterorequals_j(search_el->seq_num, seq_num) == true) {
        if(search_el->seq_num == seq_num) {
            return true;
        }

        // we search for an smaller element
        search_el = search_el->prev;
    }

    if(search_el->seq_num == seq_num) {
        return true;
    }

    if(batman_create_new_sw_element(&new_el, seq_num) == false) {
        return false;
    }

    if(batman_hf_seq_i_graterorequals_j(search_el->seq_num, seq_num) == false) {
        // insert new element after search element
        new_el->prev = search_el;
        new_el->next = search_el->next;
        search_el->next = new_el;

        if(new_el->next != NULL) {
            new_el->next->prev = new_el;
        }

        if(sw->head == search_el) {
            sw->head = new_el;
        }
    }
    else {
        // insert new element befor search element
        new_el->prev = search_el->prev;
        new_el->next = search_el;
        search_el->prev = new_el;

        if(new_el->prev != NULL) {
            new_el->prev->next = new_el;
        }

        if(sw->tail == search_el) {
            sw->tail = new_el;
        }
    }

    sw->size++;

    // drop all elements out of WINDOW_SIZE range
    batman_sw_dropsn(sw, sw->head->seq_num);
    return true;
}

void batman_sw_report(batman_sw_t* sw) {
    printf("----------Sliding Window -------------\n");
    printf("window size : %i\n", sw->size);
    printf("max sequence number : %i\n", (uint)sw->head->seq_num);
    batman_sw_element_t* search_el = sw->tail;
    printf("elements : (");

    while(search_el != NULL) {
        printf(" %i ", search_el->seq_num);
        search_el = search_el->next;
    }

    printf(")\n");
}
