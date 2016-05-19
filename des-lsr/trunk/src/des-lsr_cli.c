#include "des-lsr.h"
#include "des-lsr_items.h"
#include <pthread.h>

int cli_set_hello_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	hello_interval = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_send_hello);
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = hello_interval / 1000;
	hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
	periodic_send_hello = dessert_periodic_add(send_hello, NULL, NULL, &hello_interval_t);
	dessert_notice("setting HELLO interval to %d ms\n", hello_interval);
    return CLI_OK;
}

int cli_show_hello_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "HELLO interval = %d ms\n", hello_interval);
    return CLI_OK;
}

int cli_set_tc_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	tc_interval = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_send_tc);
	struct timeval tc_interval_t;
	tc_interval_t.tv_sec = tc_interval / 1000;
	tc_interval_t.tv_usec = (tc_interval % 1000) * 1000;
	periodic_send_tc = dessert_periodic_add(send_tc, NULL, NULL, &tc_interval_t);
	dessert_notice("setting TC interval to %d ms\n", tc_interval);
    return CLI_OK;
}

int cli_show_tc_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "TC interval = %d ms\n", tc_interval);
    return CLI_OK;
}

int cli_set_refresh_list(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	nh_refresh_interval = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_refresh_nh);
	struct timeval refresh_neighbor_t;
	refresh_neighbor_t.tv_sec = nh_refresh_interval / 1000;
	refresh_neighbor_t.tv_usec = (nh_refresh_interval % 1000) * 1000;
	periodic_refresh_nh = dessert_periodic_add(refresh_list, NULL, NULL, &refresh_neighbor_t);
	dessert_notice("setting NH refresh interval to %d ms\n", nh_refresh_interval);
    return CLI_OK;
}

int cli_show_refresh_list(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "NH refresh interval = %d ms\n", nh_refresh_interval);
    return CLI_OK;
}

int cli_set_refresh_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	rt_refresh_interval = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_refresh_rt);
	struct timeval refresh_rt_t;
	refresh_rt_t.tv_sec = rt_refresh_interval / 1000;
	refresh_rt_t.tv_usec = (rt_refresh_interval % 1000) * 1000;
	periodic_refresh_rt = dessert_periodic_add(refresh_rt, NULL, NULL, &refresh_rt_t);
	dessert_notice("setting RT refresh interval to %d ms\n", rt_refresh_interval);
    return CLI_OK;
}

int cli_show_refresh_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "RT refresh interval = %d ms\n", rt_refresh_interval);
    return CLI_OK;
}

int cli_set_nh_entry_age(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	nh_entry_age = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_notice("setting NH entry age to %d ms\n", rt_refresh_interval);
    return CLI_OK;
}

int cli_show_nh_entry_age(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "NH entry age = %d ms\n", nh_refresh_interval);
    return CLI_OK;
}

int cli_set_rt_entry_age(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	rt_entry_age = (u_int16_t) strtoul(argv[0], NULL, 10);
	dessert_notice("setting RT entry age to %d ms\n", rt_refresh_interval);
    return CLI_OK;
}

int cli_show_rt_entry_age(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "RT entry age = %d ms\n", rt_refresh_interval);
    return CLI_OK;
}

int cli_show_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	all_nodes_t *node = all_nodes_head;
	char rt[1000];

	if (!node) {
		return CLI_OK;
	}

	cli_print(cli, "##############################################################################################");
	cli_print(cli, "## MAC\t\t\t# NEXT HOP\t\t# DISTANCE\t# SEQ NR\t# ENTRY AGE ##");
	cli_print(cli, "##############################################################################################");

	while (node) {
		sprintf(rt, "## " MAC "\t# " MAC "\t# %d\t\t# %d\t\t# %d\t    ##\t",
				node->addr[0], node->addr[1], node->addr[2], node->addr[3],
				node->addr[4], node->addr[5], node->next_hop[0], node->next_hop[1],
				node->next_hop[2], node->next_hop[3], node->next_hop[4],
				node->next_hop[5], node->distance, node->seq_nr, node->entry_age);
		cli_print(cli, rt);
		node = node->hh.next;
	}

	cli_print(cli, "##############################################################################################");
	return CLI_OK;
}
