#include "../dsr.h"
#include <string.h>





/*
 * ---ORIGINAL MSG---------------------------
 * prev hop index [2]
 * prev hop       [00:00:00:00:00:02]
 * next hop index [3]
 * next hop       [00:00:00:00:00:03]
 * segments left  [7]
 * ------------------------------------------
 *
 * ---MSG SEND-------------------------------
 * prev hop index [4]
 * prev hop       [00:00:00:00:00:04]
 * next hop index [5]
 * next hop       [00:00:00:00:00:05]
 * segments left  [5]
 * ------------------------------------------

 * ---ERROR MSG------------------------------
 * prev hop index [6]
 * prev hop       [00:00:00:00:00:03]
 * next hop index [7]
 * next hop       [00:00:00:00:00:02]
 * segments left  [3]
 * ------------------------------------------
 * error dest     [55:55:55:55:55:55]
 * error source   [00:00:00:00:00:04]
 * error tsi      [00:00:00:00:00:05]
 * ------------------------------------------
 *
 */


int main(int argc, char** argv) {
    dsr_path_t* path_orig = malloc(sizeof(dsr_path_t));
    dsr_path_t* path_revd = malloc(sizeof(dsr_path_t));
    uint8_t p_orig[10 * ETHER_ADDR_LEN] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
                                           0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
                                          };
    uint8_t p_temp[10 * ETHER_ADDR_LEN];
    uint8_t p_revd[10 * ETHER_ADDR_LEN];

    dsr_patharray_reverse(p_revd, p_orig, 10);
    dsr_patharray_reverse(p_temp, p_revd, 10);
    assert(ADDR_N_CMP(p_orig, p_temp, 10) == 0);


    ADDR_N_CPY(path_orig->address, p_orig, 10);
    path_orig->len = 10;
    path_orig->weight = 10;
    path_orig->next = NULL;
    path_orig->prev = NULL;

    dsr_path_reverse(path_revd, path_orig);
    dsr_patharray_reverse(p_temp, path_revd->address, 10);
    assert(ADDR_N_CMP(path_orig->address, p_temp, 10) == 0);


    /* ONLY TESTS ABOVE */

    dessert_msg_t* msg;
    dessert_msg_new(&msg);

    dsr_source_ext_t* source = dsr_msg_add_source_ext(msg, path_orig, 7);

    ADDR_CPY(msg->l2h.ether_shost,  dsr_source_previous_hop_begin(source));
    ADDR_CPY(msg->l2h.ether_dhost,  dsr_source_indicated_next_hop_begin(source));

    /* RECEIVING MEESSAGE*/
    printf("---ORIGINAL MSG---------------------------\n");
    printf("prev hop index [%d]\n", dsr_source_previous_hop_index(source));
    printf("prev hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_previous_hop_begin(source)));
    printf("next hop index [%d]\n", dsr_source_indicated_next_hop_index(source));
    printf("next hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_indicated_next_hop_begin(source)));
    printf("segments left  [%d]\n", source->segments_left);
    printf("------------------------------------------\n\n");
    /* PARSING MESSAGE  - WITH INTERFACE CHANGE */

    source->segments_left--;
    source->segments_left--;

    /* SENDING AND STORING IN MAINTENANCE BUFFER */

    ADDR_CPY(msg->l2h.ether_dhost,  dsr_source_indicated_next_hop_begin(source));
    printf("---MSG SEND-------------------------------\n");
    printf("prev hop index [%d]\n", dsr_source_previous_hop_index(source));
    printf("prev hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_previous_hop_begin(source)));
    printf("next hop index [%d]\n", dsr_source_indicated_next_hop_index(source));
    printf("next hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_indicated_next_hop_begin(source)));
    printf("segments left  [%d]\n", source->segments_left);
    printf("------------------------------------------\n\n");

    /* GENERATE ERROR MSG */

    dsr_path_t* error_path;
    dsr_path_new_from_reversed_patharray(&error_path, source->address, dsr_source_get_address_count(source), 0);

    dessert_msg_t* error_msg;
    dessert_msg_new(&error_msg);

    int error_next_hop_index = dsr_path_get_index(msg->l2h.ether_shost, error_path);
    int error_segments_left = dsr_source_get_address_count(source) - error_next_hop_index;

    dsr_source_ext_t* error_source = dsr_msg_add_source_ext(error_msg, error_path, error_segments_left);
    free(error_path);
    ADDR_CPY(error_msg->l2h.ether_shost,  dsr_source_previous_hop_begin(error_source));
    ADDR_CPY(error_msg->l2h.ether_dhost,  dsr_source_indicated_next_hop_begin(error_source));
    printf("---ERROR MSG------------------------------\n");
    printf("prev hop index [%d]\n", dsr_source_previous_hop_index(error_source));
    printf("prev hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_previous_hop_begin(error_source)));
    printf("next hop index [%d]\n", dsr_source_indicated_next_hop_index(error_source));
    printf("next hop       [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_indicated_next_hop_begin(error_source)));
    printf("segments left  [%d]\n", error_source->segments_left);
    printf("------------------------------------------\n");
    printf("error dest     [" MAC "]\n", EXPLODE_ARRAY6(ADDR_IDX(error_source, dsr_source_get_address_count(error_source) - 1)));
    printf("error source   [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_previous_hop_begin(source)));
    printf("error tsi      [" MAC "]\n", EXPLODE_ARRAY6(dsr_source_indicated_next_hop_begin(source)));
    printf("------------------------------------------\n\n");
    /* cleanup */
    dessert_msg_destroy(msg);
    dessert_msg_destroy(error_msg);
    free(path_orig);
    free(path_revd);

    return 0;
}
