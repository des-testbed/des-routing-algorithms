#include <dessert.h>

dessert_cb_result_t lsr_sys2mesh(dessert_msg_t *msg, uint32_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id);
dessert_cb_result_t lsr_process_ttl(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_drop_errors(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_process_hello(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_process_tc(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_forward_multicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_forward_unicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_mesh2sys(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_unhandled(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result_t lsr_loopback(dessert_msg_t *msg, uint32_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id);

int lsr_send_randomized(dessert_msg_t *msg);

int lsr_send(dessert_msg_t *msg, dessert_meshif_t *iface);
