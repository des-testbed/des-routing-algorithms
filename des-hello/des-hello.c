/******************************************************************************
 Copyright 2011, The DES-SERT Team, Freie Universitaet Berlin (FUB).
 All rights reserved.

 These sources were originally developed by Philipp Schmidt
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
 *******************************************************************************/

#include <dessert.h>
#include <string.h>
#include <time.h>

#define HELLO_EXT_TYPE (DESSERT_EXT_USER + 4)

mac_addr *hwaddr_follow = NULL;

int periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_debug("sending hello");
	dessert_msg_t* hello_msg;
	dessert_ext_t* ext;

	// create new HELLO message with hello_ext.
	dessert_msg_new(&hello_msg);
	hello_msg->ttl = 2;

	dessert_msg_addext(hello_msg, &ext, HELLO_EXT_TYPE, 0);

	if(dessert_meshsend(hello_msg, NULL) != DESSERT_OK) {
		fputs("FAIL\n", stderr);
	}
	dessert_msg_destroy(hello_msg);
	return 0;
}

int handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t* hallo_ext;

	if (dessert_msg_getext(msg, &hallo_ext, HELLO_EXT_TYPE, 0) != 0) {
		msg->ttl--;
		if (msg->ttl >= 1) { // send hello msg back
			dessert_debug("received hello req from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_shost));
			memcpy(msg->l2h.ether_dhost, msg->l2h.ether_shost, ETH_ALEN);
			dessert_meshsend(msg, iface);
			dessert_debug("send hello req back to " MAC, EXPLODE_ARRAY6(msg->l2h.ether_shost));
		} else {
			if (memcmp(iface->hwaddr, msg->l2h.ether_dhost, ETH_ALEN) == 0) {
				dessert_debug("received hello resp from " MAC, EXPLODE_ARRAY6(msg->l2h.ether_shost));
			}
		}
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

/**
* Send dessert message via all registered interfaces.
**/
int toMesh(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *tunif, dessert_frameid_t id) {
	dessert_meshsend(msg, NULL);
	return DESSERT_MSG_DROP;
}

int periodic_report_follow(void *data, struct timeval *scheduled, struct timeval *interval) {

	return dessert_log_monitored_neighbour(*hwaddr_follow);
}

static int cli_cmd_follow(struct cli_def *cli, char *command, char *argv[], int argc) {

	if(*hwaddr_follow == NULL) {
		hwaddr_follow = calloc(1, sizeof(hwaddr_follow));
	} else {
		cli_print(cli, "FOLLOW - already following MAC [" MAC "]", EXPLODE_ARRAY6((*hwaddr_follow)));
		return CLI_ERROR;
	}

	struct timeval follow_interval_t;
	follow_interval_t.tv_sec = 1;
	follow_interval_t.tv_usec = 0;
	
	int valid = -1;
	if(argc >= 1) {
		valid = dessert_parse_mac(argv[0], hwaddr_follow);
	} else {
		cli_print(cli, "FOLLOW - no MAC Address given...");
		return CLI_ERROR;
	}

	if(valid >= 0) {
		cli_print(cli, "FOLLOW - MAC Address: [%s]", argv[0]);
	} else {
		cli_print(cli, "FOLLOW - MAC Address not valid: [%s]", argv[0]);
		return CLI_ERROR;
	}
	
	if(argc >= 3) {
		follow_interval_t.tv_sec = atoi(argv[1]);
		follow_interval_t.tv_usec = atoi(argv[2]);
		cli_print(cli, "FOLLOW - set interval: %lldsec + %lldusec", (long long) follow_interval_t.tv_sec, (long long) follow_interval_t.tv_usec);
	} else {
		cli_print(cli, "FOLLOW - no interval...taking default: %lldsec + %lldusec", (long long) follow_interval_t.tv_sec, (long long) follow_interval_t.tv_usec);
	}

	cli_print(cli, "FOLLOW - writing RSSI-info for neighbour [%s] in log file", argv[0]);

	dessert_periodic_add(periodic_report_follow, NULL, NULL, &follow_interval_t);

	return CLI_OK;
}

int main(int argc, char *argv[]) {

	/* initialize daemon with correct parameters */
	FILE *cfg = NULL;
	if ((argc == 2) && (strcmp(argv[1], "-nondaemonize") == 0)) {
		dessert_info("starting HELLO in non daemonize mode");
		dessert_init("DESX", 0xEE, DESSERT_OPT_NODAEMONIZE);
		char cfg_file_name[] = "./des-hello.cli";
		cfg = fopen(cfg_file_name, "r");
		if (cfg == NULL) {
			printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		dessert_info("starting HELLO in daemonize mode");
		cfg = dessert_cli_get_cfg(argc, argv);
		dessert_init("DESX", 0xEE, DESSERT_OPT_DAEMONIZE);
	}

	/* initalize logging */
	dessert_logcfg(DESSERT_LOG_STDERR | DESSERT_LOG_FILE);
	
	/* cli initialization */
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

	cli_register_command(dessert_cli, NULL, "follow", cli_cmd_follow, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "configure a neighbour to follow -> output to logfile");

	/* registering callbacks */
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
	dessert_meshrxcb_add(handle_hello, 40);

	dessert_sysrxcb_add(toMesh, 100);

	/* registering periodic tasks */
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = 1;
	hello_interval_t.tv_usec = 0;
	dessert_periodic_add(periodic_send_hello, NULL, NULL, &hello_interval_t);
	dessert_info("starting periodic send hello - interval set to %d.%ds", hello_interval_t.tv_sec, hello_interval_t.tv_usec);

	/* running cli & daemon */
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	dessert_cli_run();
	dessert_run();

	return EXIT_SUCCESS;
}
