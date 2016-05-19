#include "../dsr.h"
#include <string.h>
#include <unistd.h>

dsr_linkcache_t* lc = NULL;

int main(int argc, char** argv) {
    uint8_t n_1[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
    uint8_t n_2[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
    uint8_t n_3[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };
    uint8_t n_4[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };
    uint8_t n_5[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x05 };
    uint8_t n_6[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };

    dsr_blacklist_add_node(n_1);
    dsr_blacklist_add_node(n_2);
    dsr_blacklist_add_node(n_3);
    dsr_blacklist_add_node(n_4);
    dsr_blacklist_add_node(n_5);
    dsr_blacklist_add_node(n_6);

    cleanup_blacklist(NULL, NULL, NULL);

    dsr_blacklist_remove_node(n_1);
    dsr_blacklist_remove_node(n_2);
    dsr_blacklist_remove_node(n_3);
    dsr_blacklist_remove_node(n_4);
    dsr_blacklist_remove_node(n_5);
    dsr_blacklist_remove_node(n_6);

    dsr_blacklist_add_node(n_1);
    dsr_blacklist_add_node(n_1);

    dsr_blacklist_remove_node(n_1);
    dsr_blacklist_remove_node(n_1);

    cleanup_blacklist(NULL, NULL, NULL);

    dsr_blacklist_add_node(n_1);
    dsr_blacklist_add_node(n_2);
    dsr_blacklist_add_node(n_3);
    dsr_blacklist_add_node(n_4);
    dsr_blacklist_add_node(n_5);
    dsr_blacklist_add_node(n_6);

    cleanup_blacklist(NULL, NULL, NULL);

    sleep(61);

    cleanup_blacklist(NULL, NULL, NULL);

    sleep(121);

    cleanup_blacklist(NULL, NULL, NULL);

    return 0;
}
