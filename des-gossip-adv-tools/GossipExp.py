#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -*- Mode: python -*-

import logging
import sys
import time
import os
import re
import ConfigParser
import getopt
import shutil
import commands

from DesTestbedControl import DesTestbedControl
from GossipExpMisc import GossipExpMisc


class GossipExp:
    '''
    This class configures and runs experiments for any possible gossip variant.
    It can be configured with a single configuration file. The script creates
    configuration files in your home directory named ".des-gossip-adv" and
    ".des-gossip-adv.conf". (Watch out, they are hidden)
    Finally it stores the gathered data to a save place.
    '''
    def __init__(self):
        '''
        The init function just reads in the needed command line parameters.
        These are:
           * The location of the config file.
        '''
        try:
            opts, args = getopt.getopt(sys.argv[1:], "c:", ["config="])
            if len(opts) < 1:
                self.__print_usage()
                sys.exit(9)
        except getopt.GetoptError, err:
            print str(err)
            self.__print_usage()
            sys.exit(9)
        for o, a in opts:
            if o == "-c" or o == "--config":
                self.CONFIG_FILE = a
        self.MISC = GossipExpMisc()

    def __print_usage(self):
        '''
        Simply prints the usage string out.
        '''
        print "USAGE: GossipExp.py -c config_file"
        print "       GossipExp.py --config=config_file"

    def __validate_gossip0(self, experiment, parameters, cfg):
        '''
        This function validates all configuration parameters concerning only the
        gossip 0 variant.

        Args:
           experiment (str): The current experiment experiment.

           parameters (dict): The dictionary containing this experiments parameters.

           cfg (ConfigParser): The config parser instance.
        '''
        p_range = cfg.get(experiment, "p_range")
        p_range = p_range.rstrip("]")
        p_range = p_range.lstrip("[")
        p_values = p_range.split(",")
        p_min = float(p_values[0])
        p_max = float(p_values[1])
        p_delta = cfg.getfloat(experiment, "p_delta")
        if p_min > p_max:
            self.LOGGER.error("Config error: p_min > p_max!")
            sys.exit(9)
        if p_min == p_max:
            self.LOGGER.warning("p_min == p_max!")
        if p_max > 1:
            self.LOGGER.error("Config error: p_max > 1! (%f)" % p_range[1])
            sys.exit(9)
        if p_min != p_max:
            if p_delta == 0:
                self.LOGGER.error("p_min != p_max, you have to specify p_delta!")
                sys.exit(9)
            if ((p_max - p_min) / p_delta) % 1 != 0:
                self.LOGGER.error("P_delta value (%f) does not fit into p_range (%s)!" % (p_delta, [p_min,p_max]))
                sys.exit(9)
        parameters["p_min"] = p_min
        parameters["p_max"] = p_max
        parameters["p_delta"] = p_delta
        parameters["p_range"] = self.MISC.rangef(p_min, p_max, p_delta)

    def __write_daemon_config(self, options, experiment):
        '''
        This function writes the two config files used by the daemon to
        the users home directory.

        Args:
           options (dict): The dictionary containing all options.

           experiment (str): The current experiment experiment.
        '''
        config_target = options["testbed_dir"] + ".des-gossip-adv"
        config_location = os.path.expanduser("~") + "/.des-gossip-adv.conf"
        parameters_target = options["testbed_dir"] + ".des-gossip-adv.conf"
        try:
            os.remove(config_target)
        except OSError, err:
            pass
        try:
            os.remove(parameters_target)
        except OSError, err:
            pass
        # write new config to des-gossip-adv (general config)
        try:
            cfile = file(config_target, "w")
            cfile.write("DAEMON_OPTS=\"%s\"" % config_location)
            cfile.write(os.linesep)
            cfile.write("PIDFILE=\"/var/run/des-gossip-adv.pid\"")
            cfile.write(os.linesep)
            cfile.write("LOGFILE=%s" % options["daemon_logdir"])
            cfile.write(os.linesep)
            cfile.write("TAP_NAME=%s" % options["tap_name"])
            cfile.write(os.linesep)
            cfile.write("TAP_IP=%s" % options["tap_ip"])
            cfile.write(os.linesep)
            cfile.write("TAP_NETMASK=%s" % options["tap_netmask"])
            cfile.write(os.linesep)
            cfile.write("CLI_PORT=%s" % options["cli_port"])
            cfile.write(os.linesep)
            cfile.write("IFACE=%s" % ",".join(options[experiment]["iface_list"]))
            cfile.write(os.linesep)
            cfile.flush()
            cfile.close()
        except IOError, err:
            self.LOGGER.error("!%s" % err)
            sys.exit(9)
        # write new config to des-gossip-adv.conf (gossip specific config)
        try:
            cfile = file(parameters_target, "w")
            cfile.write("no logging stderr")
            cfile.write(os.linesep)
            cfile.write("logging ringbuffer 100")
            cfile.write(os.linesep)
            cfile.write("loglevel info")
            cfile.write(os.linesep)
            cfile.write("!enable rx logging")
            cfile.write(os.linesep)
            cfile.write("set logrx on")
            cfile.write(os.linesep)
            cfile.write("!enable tx logging")
            cfile.write(os.linesep)
            cfile.write("set logtx on")
            cfile.write(os.linesep)
            cfile.write("!gossip mode")
            cfile.write(os.linesep)
            cfile.write("set gossip %d" % options[experiment]["gossip"])
            cfile.write(os.linesep)
            cfile.write("!set gossip probability")
            cfile.write(os.linesep)
            cfile.write("set p %f" % options[experiment]["p"])
            cfile.write(os.linesep)
            cfile.write("tag %s" % experiment)
            cfile.write(os.linesep)
            cfile.flush()
            cfile.close()
        except IOError, err:
            self.LOGGER.error("!%s" % err)
            sys.exit(9)
        return True

    def __validate_config(self, options, cfg):
        '''
        This function validates all general configuration parameters.
        Furtheron it fetches a copy of the original daemon init script, puts it
        into the users home directory and inserts the correct paths.

        Args:
           options (dict): The dictionary containing all options.

           cfg (ConfigParser): The config parser instance.

        Returns:
           The filled options dictionary.
        '''
        try:
            userpath = os.path.expanduser("~")
            conf_file_path = userpath + "/.des-gossip-adv"
            options["testbed_dir"] = cfg.get("experiment", "testbed_dir").rstrip("/") + "/"
            init_script_path = options["testbed_dir"] + "des-gossip-adv.init"
            cmd = "scp t9-157t:/etc/init.d/des-gossip-adv %s" % init_script_path
            try:
                self.MISC.execute_command(cmd)
            except KeyboardInterrupt:
                self.LOGGER.warning("Keyboard Interrupt. Trying to copy file from \"grid10x10_a0\"!")
                cmd = "scp grid10x10_a0:/etc/init.d/des-gossip-adv %s" % init_script_path
                self.MISC.execute_command(cmd)
            cmd = "sed -i \"s/\/etc\/default\/$NAME/%s/g\" %s" % (conf_file_path.replace("/", "\/"), init_script_path)
            self.MISC.execute_command(cmd)
            options["archive_suffix"] = cfg.get("experiment", "archive_suffix")
            if len(options["archive_suffix"]) == 0:
                options["archive_suffix"] = ""
            options["archive_location"] = cfg.get("experiment", "archive_location")
            if len(options["archive_location"]) == 0:
                raise ConfigParser.NoOptionError("archive_location", "experiment")
            tap_name = cfg.get("daemon", "tap_name")
            if len(tap_name) == 0:
                raise ConfigParser.NoOptionError("tap_name", "daemon")
            options["tap_name"] = tap_name
            tap_ip = cfg.get("daemon", "tap_ip")
            if len(tap_ip) == 0:
                raise ConfigParser.NoOptionError("tap_ip", "daemon")
            options["tap_ip"] = tap_ip
            tap_netmask = cfg.get("daemon", "tap_netmask")
            if len(tap_netmask) == 0:
                raise ConfigParser.NoOptionError("tap_netmask", "daemon")
            options["tap_netmask"] = tap_netmask
            cli_port = cfg.get("daemon", "cli_port")
            if len(cli_port) == 0:
                raise ConfigParser.NoOptionsError("cli_port", "daemon")
            options["cli_port"] = cli_port
            daemon_logdir = cfg.get("daemon", "daemon_logdir")
            if len(daemon_logdir) == 0:
                raise ConfigParser.NoOptionError("daemon_logdir", "daemon")
            options["daemon_logdir"] = daemon_logdir
        except ConfigParser.NoSectionError, err:
            self.LOGGER.critical(err)
            sys.exit(9)
        except ConfigParser.NoOptionError, err:
            self.LOGGER.critical(err)
            sys.exit(9)
        exp = "exp"
        i = 0
        while True:
            i = i + 1
            experiment = exp + str(i)
            parameters = {}
            try:
                parameters["gossip"] = cfg.getint(experiment, "gossip")
                if parameters["gossip"] != 0:
                    self.LOGGER.critical("Currently only gossip mode 0 supported!")
                    sys.exit(9)
                #parameters["host_list"] = self.MISC.parse_hosts_file(cfg.get(experiment, "host_file"))
                valid_host_list = commands.getoutput("list-hostgroup meshrouters").splitlines()
                valid_host_list = valid_host_list + commands.getoutput("list-hostgroup grid10x10").splitlines()
                options["valid_host_list"] = valid_host_list
                host_list_str = cfg.get(experiment, "hosts")
                host_list = host_list_str.split(",")
                for host in host_list:
                    if host not in valid_host_list:
                        self.LOGGER.critical("\tHost \"%s\" is no valid host! Config experiment: %s" % (host, experiment))
                        sys.exit(9)
                parameters["host_list"] = host_list
                host_file = file(os.getcwd() + "/host_list.tmp", "w")
                host_file.writelines(map(lambda x: x + "\n", host_list))
                host_file.close()
                parameters["host_file"] = os.getcwd() + "/host_list.tmp"
                source_list = cfg.get(experiment, "sources").split(",")
                source_list = map(lambda x: x.rstrip("\n").lstrip(" "), source_list)
                for source in source_list:
                    if source not in source_list:
                        self.LOGGER.critical("Source \"%s\" is not in the list of participating hosts! Config experiment: %s." % (source, experiment))
                        sys.exit(9)
                    if source not in valid_host_list:
                        self.LOGGER.critical("Source \"%s\" is no valid host! Config experiment: %s." % (source, experiment))
                        sys.exit(9)
                parameters["source_list"] = source_list
                parameters["destination"] = cfg.get(experiment, "destination")
                try:
                    index = parameters["host_list"].index(parameters["destination"])
                except ValueError:
                    pass
                parameters["iface_list"] = cfg.get(experiment, "iface").split(",")
                parameters["interval"] = cfg.getfloat(experiment, "interval")
                parameters["packetsize"] = cfg.getint(experiment, "packetsize")
                parameters["packets"] = cfg.getint(experiment, "packets")
                if parameters["host_list"][0].find("grid") > -1:
                    parameters["grid_mode"] = True
                else:
                    parameters["grid_mode"] = False
                # check parameters for this gossip mode
                if parameters["gossip"] == 0:
                    self.__validate_gossip0(experiment, parameters, cfg)
                options[experiment] = parameters.copy()
            except ConfigParser.NoSectionError:
                if i == 1:
                    self.CRITICAL("No experiment defined. Exiting.")
                    sys.exit(9)
                self.LOGGER.debug("Config file validated successfully!")
                break;
            except ConfigParser.NoOptionError, err:
                self.LOGGER.debug(err)
                sys.exit(9)
        return options

    def __print_general_options(self, options):
        '''
        This function prints out the general options.

        Args:
           options (dict): The dictionary containing all options.
        '''
        self.LOGGER.info("----------------------------------------------------------------------------------------")
        self.LOGGER.info("Using the following options:")
        self.LOGGER.info("----------------------------------------------------------------------------------------")
        self.LOGGER.info("Debug:\t\t%s" % options["debug_level"])
        self.LOGGER.info("Debug dir:\t%s" % options["debug_dir"])
        self.LOGGER.info("Daemon log dir:\t%s" % options["daemon_logdir"])
        i = 0
        while True:
            i = i + 1
            experiment = "exp" + str(i)
            try:
                gossip = options[experiment]["gossip"]
            except KeyError:
                break
            options[experiment]["p"] = 0
            self.__print_specific_options(options, experiment)

    def __check_host_length(self, hostname):
        if len(hostname) > 8:
            hostname = hostname + "\t"
        else:
            hostname = hostname + "\t\t"
        return hostname

    def __print_specific_options(self, options, experiment):
        '''
        This function prints out configuration parameters for a given experiment experiment.

        Args:
           options (dict): The options dictionary.

           experiment (str): The used experiment experiment.
        '''
        gossip = int(options[experiment]["gossip"])
        try:
            gossip = options[experiment]["gossip"]
        except KeyError:
            return

        self.LOGGER.info("----------------------------------------------------------------------------------------")
        self.LOGGER.info("EXPERIMENT %s" % experiment)
        self.LOGGER.info("----------------------------------------------------------------------------------------")
        self.LOGGER.info("Gossip:\t\t%s" % (gossip))
        sources_list_tmp = []
        sources_list = options[experiment]["source_list"][:]
        while True:
            try:
                sources_list_tmp.append(sources_list.pop(0))
            except IndexError:
                self.LOGGER.info("Sources:\t%s" % ",".join(sources_list_tmp))
                break
            if len(sources_list_tmp) == 5:
                self.LOGGER.info("Sources:\t%s" % ",".join(sources_list_tmp))
        self.LOGGER.info("Destination:\t%s" % options[experiment]["destination"])
        tmp_host_list = options[experiment]["host_list"][:]
        while len(tmp_host_list) > 0:
            try:
                h1 = tmp_host_list.pop(0)
                h2 = tmp_host_list.pop(0)
                h3 = tmp_host_list.pop(0)
                h4 = tmp_host_list.pop(0)
                h5 = tmp_host_list.pop(0)
                h6 = tmp_host_list.pop(0)
                h7 = tmp_host_list.pop(0)
                h8 = tmp_host_list.pop(0)
                h1 = self.__check_host_length(h1)
                h2 = self.__check_host_length(h2)
                h3 = self.__check_host_length(h3)
                h4 = self.__check_host_length(h4)
                h5 = self.__check_host_length(h5)
                h6 = self.__check_host_length(h6)
                h7 = self.__check_host_length(h7)
                h8 = self.__check_host_length(h8)

                self.LOGGER.info("Hosts:\t\t%s%s%s%s%s%s%s%s" % (str(h1),str(h2),str(h3),str(h4),str(h5),str(h6),str(h7),str(h8)))
            except IndexError:
                break
        #self.LOGGER.info("Hosts:\t%s" % str(options[experiment]["host_list"]))
        #self.LOGGER.info("Hostfile:\t%s" % str(options[experiment]["host_file"]))
        self.LOGGER.info("Interface:\t%s" % ",".join(options[experiment]["iface_list"]))
        self.LOGGER.info("Packets:\t%s" % options[experiment]["packets"])
        self.LOGGER.info("Packetsize:\t%s" % options[experiment]["packetsize"])
        if gossip == 0:
            self.LOGGER.info("P min:\t\t%s" % options[experiment]["p_min"])
            self.LOGGER.info("P max:\t\t%s" % options[experiment]["p_max"])
            self.LOGGER.info("P delta:\t%s" % options[experiment]["p_delta"])
            self.LOGGER.info("P currently:\t%s" % options[experiment]["p"])
        elif gossip == 1:
            pass

    def __execute_experiment(self, options, experiment):
        '''
        This function executes one experiment.

        Args:
           options (dict): The options dict.

           experiment (str): The current experiment experiment.
        '''
        tbctrl = DesTestbedControl()
        for p in options[experiment]["p_range"]:
            options[experiment]["p"] = p
            if p == float(0):
                self.LOGGER.info("Skipping probability 0.")
                continue
            self.__print_specific_options(options, experiment)
            self.__write_daemon_config(options, experiment)
            result = tbctrl.reboot_hosts(options[experiment]["host_file"])
            if result == False:
                self.LOGGER.critical("Could not send reboot command to all hosts. Exiting.")
                sys.exit(9)
            for iface in options[experiment]["iface_list"]:
                if options[experiment]["grid_mode"] == True:
                    tbctrl.grid_interface_state(iface, "up", options[experiment]["host_file"])
                    tbctrl.assign_ip(iface, options[experiment]["host_file"])
                elif options[experiment]["grid_mode"] == False:
                    tbctrl.testbed_interface_state(iface, "up", options[experiment]["host_file"])
            daemon_init = os.path.expanduser("~") + "/des-gossip-adv.init"
            cmd_ipv6 = "sudo modprobe ipv6"
            tbctrl.do_pssh(cmd_ipv6, options[experiment]["host_file"])
            tbctrl.start_stop_daemon("des-gossip-adv", "start", options[experiment]["host_file"], daemon_init)
            tbctrl.ping(options["tap_name"], options[experiment]["source_list"], options[experiment]["destination"], options[experiment]["packets"], options[experiment]["packetsize"], options[experiment]["interval"])

    def __iterate_experiments(self, options):
        '''
        This function iterates over all configured experiments.

        Args:
           cfg (ConfigParser): The config parser instance.

           options (dict): The options dictionary.
        '''
        for experiment in options["experiment_list"]:
            self.LOGGER.info("Running experiment [%s]." % experiment)
            self.MISC.create_logdirs(options, experiment)
            self.__execute_experiment(options, experiment)
        self.LOGGER.info("All experiments run.")

    def main(self):
        options = {}
        # get a config parser
        cfg = ConfigParser.SafeConfigParser()
        # read the config file
        cfg.read(self.CONFIG_FILE)
        options["experiment_list"] = self.MISC.get_experiment_list(cfg)
        self.MISC.start_logging(options, cfg)
        self.LOGGER = logging.getLogger("GossipExp    ")
        self.__validate_config(options, cfg)
        self.__print_general_options(options)
        self.MISC.batch_check()
        self.__iterate_experiments(options)
        self.MISC.save_experiment_data(options)
        sys.exit(9)
        self.__batch_check()


if __name__ == "__main__":

    gossip = GossipExp()
    gossip.main()
    sys.exit(0)
