#include "../dsr.h"

dsr_linkcache_t* lc = NULL;

int main(int argc, char** argv) {
    uint8_t n_1[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
    uint8_t n_2[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
    uint8_t n_3[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };
    uint8_t n_4[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };
    uint8_t n_5[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x05 };
    uint8_t n_6[ETHER_ADDR_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };

    dsr_linkcache_init(n_1);

    dsr_linkcache_add_link(n_1, n_4, 100);
    dsr_linkcache_add_link(n_1, n_3, 100);
    dsr_linkcache_add_link(n_4, n_5, 100);
    dsr_linkcache_add_link(n_3, n_5, 100);
    dsr_linkcache_add_link(n_5, n_4, 100);
    dsr_linkcache_add_link(n_5, n_6, 100);
    dsr_linkcache_add_link(n_5, n_1, 100);
    dsr_linkcache_add_link(n_6, n_5, 100);
    dsr_linkcache_add_link(n_6, n_2, 100);

    dsr_linkcache_print();

    dsr_linkcache_remove_link(n_1, n_4);
    dsr_linkcache_remove_link(n_1, n_3);
    dsr_linkcache_remove_link(n_4, n_5);
    dsr_linkcache_remove_link(n_3, n_5);
    dsr_linkcache_remove_link(n_5, n_4);
    dsr_linkcache_remove_link(n_5, n_6);
    dsr_linkcache_remove_link(n_5, n_1);
    dsr_linkcache_remove_link(n_6, n_5);
    dsr_linkcache_remove_link(n_6, n_2);

    dsr_linkcache_print();
    return 0;
}
