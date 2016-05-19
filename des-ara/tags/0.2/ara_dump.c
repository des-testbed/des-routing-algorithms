/******************************************************************************
 Copyright 2009, Philipp Schmidt, Freie Universitaet Berlin (FUB).
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

#include "ara.h"

FILE *ar_packetdump;

/** dump packets via tun interface **/
int tundump_cb (struct ether_header *eth, size_t len, dessert_msg_proc_t *proc, dessert_tunif_t *tunif, dessert_frameid_t id)
{
    dessert_msg_t *msg;
    char buf[1024];
    
    dessert_msg_ethencap(eth, len, &msg);
    dessert_msg_dump(msg, DESSERT_MAXFRAMELEN, buf, 1024);
    dessert_msg_destroy(msg);
    fprintf(ar_packetdump, 
            "--- begin packet #%lu on %s ---\n\n%s\n--- end packet #%lu on %s ---\n",
            (unsigned long) id, (tunif != NULL)?(tunif->if_name):"NULL",
            buf, (unsigned long) id, (tunif != NULL)?(tunif->if_name):"NULL");
    
    return DESSERT_MSG_KEEP;
    
}

/** dump packets via one gossiping interface **/
int meshdump_cb(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id)
{
    char buf[1024];
    
    dessert_msg_proc_dump(msg, len, proc, buf, 1024);
    fprintf(ar_packetdump, 
            "--- begin packet #%lu on %s ---\n\n%s\n--- end packet #%lu on %s ---\n",
            (unsigned long) id, (iface != NULL)?(iface->if_name):"NULL",
            buf, (unsigned long) id, (iface != NULL)?(iface->if_name):"NULL");
    
    return DESSERT_MSG_KEEP;
    
}

/** CLI command - config mode - packetdump $file */
int cli_packetdump(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    
    if(argc != 1) {
        cli_print(cli, "usage %s [filename]\n", command);
        return CLI_ERROR;
    }
    if(ar_packetdump != NULL)
    {
        cli_print(cli, "packet dump already running - use no packetdump first to stop it\n");
        return CLI_ERROR;
    }
    ar_packetdump = fopen(argv[0], "a");
    if(ar_packetdump != NULL) {
        dessert_info("started packet dump to file %s", argv[0]);
        dessert_tunrxcb_add(tundump_cb, 20);
        dessert_meshrxcb_add(meshdump_cb, 20);
        return(CLI_OK);
    } else {
        dessert_err("opening packet dump file %s failed", argv[0]);
        cli_print(cli, "opening packet dump file %s failed\n", argv[0]);
        return CLI_ERROR;
    }
}

/** CLI command - config mode - no packetdump */
int cli_nopacketdump(struct cli_def *cli, char *command, char *argv[], int argc) 
{
    if(ar_packetdump!=NULL) {
        dessert_tunrxcb_del(tundump_cb);
        dessert_meshrxcb_del(meshdump_cb);
        fclose(ar_packetdump);
        dessert_info("stopped packet dump");
        return CLI_OK;
        
    }
    return CLI_ERROR;
}

