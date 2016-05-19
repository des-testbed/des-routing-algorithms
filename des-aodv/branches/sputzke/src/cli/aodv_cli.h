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

#ifdef ANDROID
#include <linux/if_ether.h>
#endif

int cli_set_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tracking_factor(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_gossip_p(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_dest_only(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_ring_search(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_metric(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_gossip(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_periodic_rreq_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_preemptive_rreq_signal_strength_threshold(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_show_gossip_p(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_preemptive_rreq_signal_strength_threshold(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_periodic_rreq_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_metric(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_gossip(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rreq_size(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tracking_factor(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_pdr_nt(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_show_neighbor_timeslot(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_packet_buffer_timeslot(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_data_seq_timeslot(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_send_rreq(struct cli_def* cli, char* command, char* argv[], int argc);

