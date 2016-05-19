#include "lsr_cli.h"
#include "../lsr_config.h"
#include "../periodic/lsr_periodic.h"
#include "../database/lsr_database.h"

int cli_set_hello_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	hello_interval = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_send_hello);
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = hello_interval / 1000;
	hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
	periodic_send_hello = dessert_periodic_add(lsr_periodic_send_hello, NULL, NULL, &hello_interval_t);
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

	#if 0
	tc_interval = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_send_tc);
	struct timeval tc_interval_t;
	tc_interval_t.tv_sec = tc_interval / 1000;
	tc_interval_t.tv_usec = (tc_interval % 1000) * 1000;
	periodic_send_tc = dessert_periodic_add(send_tc, NULL, NULL, &tc_interval_t);
	dessert_notice("setting TC interval to %d ms\n", tc_interval);
	#endif
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

	#if 0
	neighbor_aging_interval = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_refresh_nh);
	struct timeval refresh_neighbor_t;
	refresh_neighbor_t.tv_sec = neighbor_aging_interval / 1000;
	refresh_neighbor_t.tv_usec = (neighbor_aging_interval % 1000) * 1000;
	periodic_refresh_nh = dessert_periodic_add(refresh_list, NULL, NULL, &refresh_neighbor_t);
	dessert_notice("setting NH refresh interval to %d ms\n", neighbor_aging_interval);
	#endif
	return CLI_OK;
}

int cli_show_refresh_list(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "NH refresh interval = %d ms\n", neighbor_aging_interval);
	return CLI_OK;
}

int cli_set_refresh_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	#if 0
	node_aging_interval = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_periodic_del(periodic_refresh_rt);
	struct timeval refresh_rt_t;
	refresh_rt_t.tv_sec = node_aging_interval / 1000;
	refresh_rt_t.tv_usec = (node_aging_interval % 1000) * 1000;
	periodic_refresh_rt = dessert_periodic_add(refresh_rt, NULL, NULL, &refresh_rt_t);
	dessert_notice("setting RT refresh interval to %d ms\n", node_aging_interval);
	#endif
    return CLI_OK;
}

int cli_show_refresh_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "RT refresh interval = %d ms\n", node_aging_interval);
	return CLI_OK;
}

int cli_set_neighbor_lifetime(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	neighbor_lifetime = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_notice("setting NH entry age to %d ms\n", node_aging_interval);
	return CLI_OK;
}

int cli_show_neighbor_lifetime(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "NH entry age = %d ms\n", neighbor_aging_interval);
	return CLI_OK;
}

int cli_set_node_lifetime(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval]\n", command);
		return CLI_ERROR;
	}

	node_lifetime = (uint16_t) strtoul(argv[0], NULL, 10);
	dessert_notice("setting RT entry age to %d ms\n", node_aging_interval);
	return CLI_OK;
}

int cli_show_node_lifetime(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "RT entry age = %d ms\n", node_aging_interval);
	return CLI_OK;
}

int cli_show_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	char *output = lsr_db_topology_to_string();
	cli_print(cli, "%s", output);
	free(output);
	return CLI_OK;
}

int cli_show_nt(struct cli_def *cli, char *command, char *argv[], int argc) {
	neighbor_info_t *neighbor_list = NULL;
	int neighbor_count = 0;
	lsr_db_dump_neighbor_table(&neighbor_list, &neighbor_count);

	cli_print(cli,  "#######################################################");
	cli_print(cli,  "## NEIGHBOR L25                 # WEIGHT  # LIFETIME ##");
	cli_print(cli,  "#######################################################");

	for(int i = 0; i < neighbor_count; ++i) {
		cli_print(cli, "## " MAC "\t# %d\t# %d",
				EXPLODE_ARRAY6(neighbor_list[i].addr), neighbor_list[i].weight, neighbor_list[i].lifetime);
	}

	return CLI_OK;
}
