#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -*- Mode: python -*-

import logging
import sys
import shlex
import time
from subprocess import Popen, PIPE, call


class DesTestbedControl:
    def __init__(self, user = "root"):
        self.user = user
        self.LOGGER = logging.getLogger("GossipExpRem")

    def do_pssh(self, cmd, hosts, user="root", timeout = 0, pipe_list1 = "", pipe_list2 = "", processes = 10):
        pssh_str = ""
        #  parallel-ssh -p 10 -i -l root -t 0 -Ha3-020-H t9-157t-H t9-113 "sudo ping -I tap0 -c 1000 -s 56 -i 0.200000 10.0.0.254"
        if str(type(hosts)) == "<type 'list'>":
            hosts = "-H " + " -H ".join(hosts)
            pssh_str = "parallel-ssh -p %d -i -l %s -t %d %s" % (int(processes), self.user, timeout, hosts)
        elif str(type(hosts)) == "<type 'str'>":
            pssh_str = "parallel-ssh -p %d -i -l %s -t %d -h %s" % (int(processes), self.user, timeout, hosts)
        self.LOGGER.debug("Sending pssh command: %s \"%s\"" % (pssh_str, cmd))
        args = shlex.split(pssh_str)
        args.append(cmd)
        p1 = Popen(args, stdout = PIPE)
        if len(pipe_list1) > 0:
            p2 = Popen(pipe_list1, stdin = p1.stdout, stdout = PIPE)
            if len(pipe_list2) > 0:
                p3 = Popen(pipe_list2, stdin = p2.stdout, stdout = PIPE)
                r = p3.communicate()
            else:
                r = p2.communicate()
        else:
            r = p1.communicate()
        return r

    def do_ssh(self, cmd, host, user="root", pipe_list1 = "", pipe_list2 = ""):
        ssh_string = "ssh %s@%s" % (self.user, host)
        args = shlex.split(ssh_string)
        args.append(cmd)
        p1 = Popen(args, stdout = PIPE)
        if len(pipe_list1) > 0:
            p2 = Popen(pipe_list1, stdin = p1.stdout, stdout = PIPE)
            if len(pipe_list2) > 0:
                p3 = Popen(pipe_list2, stdin = p2.stdout, stdout = PIPE)
                r = p3.communicate()
            else:
                r = p2.communicate()
        else:
            r = p1.communicate()
        return r

    def check_availability(self, hostsfile):
        self.LOGGER.info("Checking hosts for availability...")
        result, resulterr = self.do_pssh("sudo uptime", hostsfile, pipe_list1 = ["grep", "FAILURE"])
        if len(result) == 0:
            self.LOGGER.info("All hosts are available.")
            return []
        else:
            result_list = result.splitlines()
            self.LOGGER.warning("%d hosts are unavailable..." % len(result_list))
            for l in result_list:
                self.LOGGER.debug(l)
            return result_list

    def create_online_hostlist(self, hostsfile, new_hostsfiles):
        new_hostfile = file(new_hostfile, "w")
        host_file = ""
        ifile = file(host_file, "r")
        host_list = ifile.readlines()
        for i in range(0, len(host_list)):
            host_list[i] = host_list[i].rstrip("\n")
        result_list = remote.check_availability(host_file)
        if len(result_list) > 0:
            for l in result_list:
                host = l.split(" ")[3]
                host_list.remove(host)
            for i in range(0, len(host_list)):
                host_list[i] = host_list[i] + "\n"
            new_hostfile.writelines(host_list)
            return host_list
        else:
            self.LOGGER.info("Congratulations. All routers are available. No changes to hostlist made.")
            return host_list

    def reboot_hosts(self, hostfile):
        self.LOGGER.info("Rebooting hosts...")
        for i in range(1,3):
            result, resulterr = self.do_pssh("reboot", hostfile, pipe_list1 = ["grep", "FAILURE"])
            if len(result) == 0:
                self.LOGGER.info("Successfully sent the reboot command to all hosts. Reboot takes a while!")
                break
            else:
                result_list = result.splitlines()
                self.LOGGER.debug("Sending reboot command failed with following hosts:")
                for host in result_list:
                    self.LOGGER.debug(host)
                if i == 3:
                    return False
        run = True
        tries = 0
        while (run == True):
            self.LOGGER.info("Waiting for hosts to get back up...")
            time.sleep(30)
            result = self.check_availability(hostfile)
            if len(result) == 0:
                self.LOGGER.info("All hosts rebooted successfully!")
                return True
            else:
                tries = tries + 1
                self.LOGGER.warning("%d hosts did not reboot yet. Retrying (%d)..." % (len(result), tries))

    def assign_ip(self, iface, hosts):
        if iface == "eth1":
            ip = "$(calc_ip 2)"
        result, resulterr = self.do_pssh("sudo ifconfig %s %s" % (iface, ip), hosts, pipe_list1 = ["grep", "FAILURE"])
        return result

    def ping(self, interface, source_list, target, packets, packetsize = 56, interval = 1.0):
        source_str = ",".join(source_list)
        processes = len(source_list)
        self.LOGGER.info("Pinging %s from %s..." % (target, source_str))
        ping_cmd = "sudo ping -I %s -c %d -s %d -i %f %s" % (interface, int(packets), int(packetsize), float(interval), target)
        self.do_pssh(ping_cmd, source_list)

    def grid_interface_state(self, iface, state, hosts):
        result, resulterr = self.do_pssh("sudo ifconfig %s %s" % (iface, state), hosts, pipe_list1 = ["grep", "FAILURE"])
        if len(result) == 0:
            self.LOGGER.debug("All \"%s\" interfaces now \"%s\"." % (iface,state))
            return True
        else:
            result_list = result.splitlines()
            for error in result_list:
                self.LOGGER.debug("Interface state change failed. Error: %s" % error)
                return False

    def grid_interface_setip(self, iface, hosts, ip = ""):
        if (ip == ""):
            ip = "$(calc_ip 2)"
        result, resulterr = self.do_pssh("sudo ifconfig %s %s" % (iface, ip), hosts, pipe_list1 = ["grep", "FAILURE"])
        if len(result) == 0:
            return True
        else:
            result_list = result.splitlines()
            for error in result_list:
                self.LOGGER.debug("Setting ip failed. Error:" % error)
                return False

    def testbed_interface_state(self, iface, state, hosts):
        result, resulterr = self.do_pssh("sudo if%s %s" % (state, iface), hosts, pipe_list1 = ["grep", "FAILURE"])
        if len(result) == 0:
            self.LOGGER.debug("All \"%s\" interfaces now \"%s\"." % (iface,state))
            return True
        else:
            result_list = result.splitlines()
            for error in result_list:
                self.LOGGER.debug("ERROR: %s ." % error)
                return False

    def start_stop_daemon(self, daemon, state, hosts, daemon_init = ""):
        if state not in ["start", "stop"]:
            self.LOGGER.error("Parameter \"state\" must be either \"start\" or \"stop\".")
            return False
        if state == "stop":
            self.LOGGER.debug("...%s stopping..." % (daemon))
        else:
            self.LOGGER.debug("...%s starting..." % (daemon))
        if len(daemon_init) == 0:
            result, resulterr = self.do_pssh("sudo /etc/init.d/%s %s" % (daemon, state), hosts, pipe_list1 = ["grep", "FAILURE"])
        elif len(daemon_init) > 0:
            self.LOGGER.debug("sudo %s %s" % (daemon_init, state))
            result, resulterr = self.do_pssh("sudo %s %s" % (daemon_init, state), hosts, pipe_list1 = ["grep", "FAILURE"])
        if len(result) == 0:
            self.LOGGER.debug("State of daemon \"%s\" is now \"%s\"" % (daemon, state))
            return True
        else:
            result_list = result.splitlines()
            for error in result_list:
                self.LOGGER.debug("Start/stop of %s failed. Error: %s!." % (daemon, error))
                if (state == "start"):
                    return False
                elif (state == "stop"):
                    self.LOGGER.debug("Friendly stopping of %s failed. Sending KILL signal.")
                    result, resulterr = self.do_pssh("sudo killall %s " % (daemon), hosts)
                    return True

    def delete_logfiles(self, hosts):
        try:
            retcode = call("find /home/ende/testbed/des-gossip-adv/ -name des-gossip-adv.log -delete", shell=True)
            return True
        except OSError, e:
            self.LOGGER.debug("Deletion of old logs failed: %s" % e)
            return False

    # TODO: FIX
    def restore_cellid(self, iface, hosts):
        result, resulterr = self.do_pssh("sudo /home/ende/bin/set_ap.py wlan%s" % iface, hosts, pipe_list1 = ["grep", "FAILURE"])
        result_list = result.splitlines()
        for error in result_list:
            self.LOGGER.debug("Start/stop of %s failed. Error: %s!." % (daemon, error))
            if (state == start):
                return False
            elif (state == stop):
                self.LOGGER.debug("Friendly stopping of %s failed. Sending KILL signal.")
                result, resulterr = self.do_pssh("sudo killall %s " % (daemon), hosts)
                return True


