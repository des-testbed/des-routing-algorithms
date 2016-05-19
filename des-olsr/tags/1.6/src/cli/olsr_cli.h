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

#include <libcli.h>

int cli_set_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tc_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_rt_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_max_missed_tc(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_max_missed_hello(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_rc_metric(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_willingness(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_window_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_port(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_show_rc_metric(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_max_missed_tc(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_max_missed_hello(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_ns(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_ns_so(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_ls(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_2hns(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt_so(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc);
