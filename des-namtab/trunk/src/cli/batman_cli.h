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
#ifdef ANDROID
#include <sys/time.h>
#endif


#include <libcli.h>

/** change ogm interval */
int batman_cli_change_ogmint(struct cli_def* cli, char* command, char* argv[], int argc);

int batman_cli_beverbose(struct cli_def* cli, char* command, char* argv[], int argc);

int batman_cli_ogmprecursormode(struct cli_def* cli, char* command, char* argv[], int argc);

int batman_cli_print_brt(struct cli_def* cli, char* command, char* argv[], int argc);

int batman_cli_print_irt(struct cli_def* cli, char* command, char* argv[], int argc);

int batman_cli_print_rt(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_cfgsysif(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_addmeshif(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_setport(struct cli_def* cli, char* command, char* argv[], int argc);

int cli_setrouting_log(struct cli_def* cli, char* command, char* argv[], int argc);
