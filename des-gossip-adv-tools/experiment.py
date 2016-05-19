#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import getopt
from subprocess import Popen,PIPE,STDOUT
import pssh
import time
import datetime
from psshlib import psshutil
import tarfile
import re
import shof
from shutil import rmtree, copytree
import random

userpath = "/home/shof/gossip"
hosts = "all"
#hosts = "/home/shof/host-mini.txt"
dest = "10.0.0.254"
timeout = int(60)
pid = ""
errors = []
fiveghz = int(2)
cellerror = False
delayedtimer = int(180)

def usage():
	print "USAGE: experiment.py [options]"
	print "\t-h,\t--help\t\t\tshow help"
	print "\t-v\t\t\t\tbe verbose"
	print "\t-d,\t--monitor\t\tspecify if the source host shut dump his traffic into a log-file"
	print "\t-g,\t--gossip=\t\tspecify the gossip mode"
	print "\t-i,\t--interfaces=\t\tspecifiy how many interfaces to use"	
	print "\t-k\t\t\t\tspecify the first k hops where the probability is 100%"
	print "\t-m,\t--min-msg-threshold=\tspecify the threshold during timeout and send the packet send again if the amount is lower as the threshold"
	print "\t-n,\t--neighbors-threshold=\tspecify the number of neighbors, if the node didn't receive this threshold, send the packet again"
	print "\t-o,\t--olsr\t\t\tspecify if the amount of available host should be checked be for the experiments starts"
	print "\t-p,\t--packages=\t\tspecify the amount of packets to send"
	print "\t--p,\t\t\t\tspecify the probability, only if you want to run one execution"	
	print "\t--p2\t\t\t\tspecify the second probability"
	print "\t--pmin\t\t\t\tspecify the p_min probability for gossip8"
	print "\t--pmax\t\t\t\tspecify the p_max probability for gossip8"	
	print "\t-s,\t--src=\t\t\tspecify the source host"
	print "\t\t--batch\t\t\tturn batch mode on (no user interaction needed)"
	print "\t\t--cleanup=\t\tspecify the interval (in seconds) for the cleanup process which cleans the list of neighbors up"
	print "\t\t--delayed\t\trun the discovery only on startup" 
	print "\t\t--diameter=\t\tspecify the diameter for the zone"
	print "\t\t--eval\t\t\tspecify if evaluation should start after finished experiment"
	print "\t\t--end=\t\t\tspecify end probability if you only want to run over a interval and not over the full 100 percent"
	print "\t\t--hello=\t\tspecify the timeout (in seconds) for the hello messages"
	print "\t\t--hosts=\t\tspecify the hosts list on which the experiments should run: Values: t9,a6,all,2xx"
	print "\t\t--l25h\t\t\tspecify if we want the layer 2.5 header"
	print "\t\t--moin=\t\t\tspecify the node with the monitor mode interface"
	print "\t\t--noloop\t\tspecify if loop detection should be disabled"
	print "\t\t--reboot\t\treboot all routers befor excecution"
	print "\t\t--start=\t\tspecify the start probability if you don't want to start at 0.01 percent"
	print "\t\t--seq\t\t\tspecify if seq should be enabled"
	print "\t\t--step=\t\t\tspecify the steps"
	print "\t\t--timeout=\t\tspecify the timeout (in milliseconds) for receiving replies from the neighbors"
	print "\t\t--ttl=\t\t\tspecify ttl"
	print "\t\t--tmax=\t\t\tspecify t_max for gossip_7"
	print "\t\t--5g\t\t\tenable only the 5GHz Band"

#------------------------------------- remote commands ---------------------------------------------------------------------------------------
def do_pssh(cmd):
	global timeout
	global hosts
	
	pssh_cmd = "parallel-ssh -p 5 -t %d -h %s" % (timeout, hosts)
	pssh_cmd = pssh_cmd.split(' ')
	args, flags = pssh.parsecmdline(pssh_cmd + [cmd])
	cmdline = " ".join(args)
        _hosts, ports, users = psshutil.read_hosts(flags["hosts"])
        psshutil.patch_users(_hosts, ports, users, flags["user"])
        pssh.do_pssh(_hosts, ports, users, cmdline, flags)

def do_ssh(host, cmd):
	cmdline = "ssh root@%s.des-mesh.net %s" % (host, cmd)
	p = Popen(cmdline, shell=True, stdout=PIPE, stderr=PIPE, close_fds=True)
	r = p.communicate()
	print r[0]
	print r[1]
	
def do_qssh(host, cmd):
	cmdline = "ssh root@%s.des-mesh.net %s" % (host, cmd)
	p = Popen(cmdline, shell=True)

def do_pissh(host,cmd):
	global pid
	cmdline = "ssh root@%s.des-mesh.net '%s'" % (host, cmd)
	process = Popen(cmdline, shell=True, stdin=PIPE, stdout=PIPE)
	pid = process.stdout.readline()

def do_cellssh():
	global timeout
	global hosts
	global cellerror
	cmdline = "parallel-ssh -p 5 -i -l root -t %d -h %s 'uptime' < /dev/null | grep user | wc -l" % (timeout, hosts)
	process = Popen(cmdline, shell=True, stdout=PIPE, stderr=PIPE, close_fds=True)
	r = process.communicate()	
	hostscount = r[0]
	cmdline2 = "parallel-ssh -p 5 -i -l root -t %d -h %s 'iwconfig wlan0 | grep Cell' < /dev/null | grep '16:EB:FF:18:C8:6F' | wc -l" % (timeout, hosts)
	process2 = Popen(cmdline2, shell=True, stdout=PIPE, stderr=PIPE, close_fds=True)
	r = process2.communicate()
	cells = r[0]
	cellerror = False
	if (int(cells) != int(hostscount)):
		cellerror = True
		print "WARN: Have %d Nodes but only %d have the right cell-id" % (int(hostscount), int(cells))
#------------------------------------- remote commands ---------------------------------------------------------------------------------------

#------------------------------------- interface commands ------------------------------------------------------------------------------------
def wlan_interface(inter, state):
	cmd_wlan2 = "if%s wlan%s" % (state, inter)
	do_pssh(cmd_wlan2)
	cmd_wlan = "ifconfig wlan%s %s" % (inter, state)
	do_pssh(cmd_wlan)

def restore(inter):
	cmd_restore = "/usr/local/bin/set_ap.py wlan%s" % inter
	do_pssh(cmd_restore)		
#------------------------------------- interface commands ------------------------------------------------------------------------------------

#------------------------------------- desser cli commands -----------------------------------------------------------------------------------
def setTag(host,tag):
	print "INFO: setting tag %s" % (tag)
	cmd_tag = "dessert_exec localhost:4519 -c \\\"tag %s\\\"" % (tag)
	do_ssh(host, cmd_tag)

def setP(p):
	print "INFO: setting p=%f" % (p)
	cmd_setp = "dessert_exec localhost:4519 -c \\\"set p %f\\\"" % (p)
	do_pssh(cmd_setp)

def setGossip(mode):
	print "INFO: setting gossip mode %d" % mode
	cmd_setgossip = "dessert_exec localhost:4519 -c \\\"set gossip %d\\\"" % (mode)
	do_pssh(cmd_setgossip)

def setk(k):
	print "INFO: setting k %d" % k
	cmd_setk = "dessert_exec localhost:4519 -c \\\"set k %d\\\"" % (k)
	do_pssh(cmd_setk)
	
def setp2(p2):
	print "INFO: setting p2 %f" % p2
	cmd_setp2 = "dessert_exec localhost:4519 -c \\\"set p2 %f\\\"" % (p2)
	do_pssh(cmd_setp2)

def setn(n):
	print "INFO: setting n %d" % n
	cmd_setn = "dessert_exec localhost:4519 -c \\\"set n %d\\\"" % (n)
	do_pssh(cmd_setn)

def setm(m):
	print "INFO: setting m %d" % m
	cmd_setm = "dessert_exec localhost:4519 -c \\\"set m %d\\\"" % (m)
	do_pssh(cmd_setm)
	
def settimeout(timeout):
	print "INFO: setting t %d" % timeout
	cmd_settimeout = "dessert_exec localhost:4519 -c \\\"set timeout %d\\\"" % (timeout)
	do_pssh(cmd_settimeout)

def sethello(hello):
	print "INFO: setting hello-interval %d" % hello
	cmd_hello = "dessert_exec localhost:4519 -c \\\"set h_interval %d\\\"" % (hello)
	do_pssh(cmd_hello)

def setcleanup(cleanup):
	print "INFO: setting cleanup-interval %d" % cleanup
	cmd_setcleanup = "dessert_exec localhost:4519 -c \\\"set clean_interval %d\\\"" % (cleanup)
	do_pssh(cmd_setcleanup)

def setzonehello(zoneh):
	print "INFO: setting zone-hello-interval %d" % zoneh
	cmd_setzoneh = "dessert_exec localhost:4519 -c \\\"set zoneh_interval %d\\\"" % (zoneh)
	do_pssh(cmd_setzoneh)

def setzonecleanup(zonec):
	print "INFO: setting zone-cleanup-interval %d" % zonec
	cmd_setzonec = "dessert_exec localhost:4519 -c \\\"set zonec_interval %d\\\"" % (zonec)
	do_pssh(cmd_setzonec)
	
def setdiameter(diameter):
	print "INFO: setting zone diameter %d" % diameter
	cmd_setdiameter = "dessert_exec localhost:4519 -c \\\"set diameter %d\\\"" % (diameter)
	do_pssh(cmd_setdiameter)		

def setttl(ttl):
	print "INFO: setting ttl %d" % ttl
	cmd_setttl = "dessert_exec localhost:4519 -c \\\"set ttl %d\\\"" % (ttl)
	do_pssh(cmd_setttl)	
	
def setseq():
	print "INFO: setting seq on"
	cmd_setseq = "dessert_exec localhost:4519 -c \\\"set seq on\\\""
	do_pssh(cmd_setseq)		

def setloop(checkloop):
	print "INFO: setting check for loops %d" % checkloop
	cmd_setcheckloop = "dessert_exec localhost:4519 -c \\\"set loop %d\\\"" % (checkloop)
	do_pssh(cmd_setcheckloop)	

def setl25h(l25h):
	if (l25h == 1):
		head="on"
	else:
		head="off"
	print "INFO: setting l25h %s" % head
	cmd_setl25h = "dessert_exec localhost:4519 -c \\\"set use25h %s\\\"" % (head)
	do_pssh(cmd_setl25h)
	
def gssp(interface):
	cmd_gssp = "dessert_exec localhost:4519 -c \\\"interface gssp wlan%d\\\"" % (interface)
	do_pssh(cmd_gssp)
	
def settmax(tmax):
	print "INFO: setting T_MAX %d" % tmax
	cmd_settmax = "dessert_exec localhost:4519 -c \\\"set tmax %d\\\"" % (tmax)
	do_pssh(cmd_settmax)

def starthelper():
	print "INFO: starting helpers" 
	cmd_starthelper = "dessert_exec localhost:4519 -c \\\"start_helper\\\"" 
	do_pssh(cmd_starthelper)

def stophelper():
	print "INFO: stopping helpers" 
	cmd_stophelper = "dessert_exec localhost:4519 -c \\\"stop_helper\\\"" 
	do_pssh(cmd_stophelper)	

def stopneighbormon():
	print "INFO: stopping neighbor monitoring" 
	cmd_stopneighbormon = "dessert_exec localhost:4519 -c \\\"stop_neighbormon\\\"" 
	do_pssh(cmd_stopneighbormon)	

def setpmin(pmin):
	print "INFO: setting pmin=%f" % (pmin)
	cmd_setpmin = "dessert_exec localhost:4519 -c \\\"set pmin %f\\\"" % (pmin)
	do_pssh(cmd_setpmin)

def setpmax(pmax):
	print "INFO: setting pmax=%f" % (pmax)
	cmd_setpmax = "dessert_exec localhost:4519 -c \\\"set pmax %f\\\"" % (pmax)
	do_pssh(cmd_setpmax)		
	
#------------------------------------- dessert cli commands ---------------------------------------------------------------------------	

#------------------------------------- gossip modes -----------------------------------------------------------------------------------
def gossip0():
	print "INFO: setting p"
	setk(0)
	
def gossip1(k):
	print "INFO: setting p and k"
	setk(k)
		
def gossip2(k, p2, n, hello, cleanup):
	print "INFO: setting p, k, p2, n, hello and cleanup"
	setk(k)	
	setp2(p2)
	setn(n)
	sethello(hello)
	setcleanup(cleanup)
	
def gossip3(k, m, t):
	print "INFO: setting p, k, m and timeout"
	setk(k)
	setm(m)
	settimeout(t)

def gossip4(k, zoneh, zonec, diameter):
	print "INFO: setting p, k, hello, cleanup and diameter"
	setk(k)
	setzonehello(zoneh)
	setzonecleanup(zonec)
	setdiameter(diameter)	
	
def gossip5(k, t, p2, hello, cleanup):	
	print "INFO: setting p, k, timeout, p2, hello and cleanup"
	setk(k)
	settimeout(t)
	setp2(p2)
	sethello(hello)
	setcleanup(cleanup)
	
def gossip6(m, t, k):
	print "INFO: setting p, k, m and timeout"
	setm(m)
	setk(k)
	settimeout(t)
	
def gossip7(m,tmax):
	print "INFO: setting p, m and tmax"
	setm(m)
	settmax(tmax)			

def gossip8(hello, cleanup, p_min, p_max):
	print "INFO: setting p, p_min, p_max, hello and cleanup"
	sethello(hello)
	setcleanup(cleanup)
	setpmin(p_min)
	setpmax(p_max)

def gossip9(n, hello, cleanup):
	print "INFO: setting n, hello and cleanup"
	setn(n)
	sethello(hello)
	setcleanup(cleanup)

def gossip11(m, t, k):
	print "INFO: setting p, k, m and timeout"
	setm(m)
	setk(k)
	settimeout(t)	
#------------------------------------- gossip modes -----------------------------------------------------------------------------------
#------------------------------------- helper function --------------------------------------------------------------------------------
def selhosts(name):
	global hosts
	if (name == "t9"):
		hosts = "%s/hosts/t9-hosts" % (userpath)
	elif (name == "a6"):
		hosts = "%s/hosts/a6-hosts" % (userpath)
	elif (name == "2xx"):
		hosts = "%s/hosts/student-hosts" % (userpath)
	elif (name == "custom"):
		hosts = "%s/hosts/custom-hosts" % (userpath)
	else:
		print "WARNING: none of your selection matches, selecting system default hosts for you!"
		hosts = "/etc/meshrouters"
		name = "system"	
	print "INFO: select %s-hosts" % (name)	

def fixipv6(inter):
	print "INFO: Fixing ipv6 - interfaces for tap%s" % (inter)
	cmd_fix = "/usr/local/bin/des-gossip-adv-fixipv6.sh tap%s" % (inter)
	do_pssh(cmd_fix)

def removeDir(path):
    if os.path.isdir(path):
        rmtree(path)

def ping(source, packages, dest):
	print "INFO: pinging"
	cmd_ping = "arping -i tap0 -c %d %s" % (packages, dest)
	do_ssh(source, cmd_ping)

def calcTime(options):
	gossiptime = 0
	if (options["gossip"] == 1 ):
		gossiptime = 1
	elif (options["gossip"] == 2 ):
		gossiptime = 6
	elif (options["gossip"] == 3 ):
		gossiptime = 3			
	elif (options["gossip"] == 4 ):
		gossiptime = 4
	elif (options["gossip"] == 5 ):
		gossiptime = 5
	elif (options["gossip"] == 6):
		gossiptime = 3	
	elif (options["gossip"] == 7):
		gossiptime = 2		
	elif (options["gossip"] == 8):
		gossiptime = 4		
	elif (options["gossip"] == 9):
		gossiptime = 3
	elif (options["gossip"] == 11):
		gossiptime = 3				
	
	olsrtime = 0
	if ( options["o"] == True ):	
		olsrtime = 15 + 5 + 300
	
	l25htime = 0
	if ( options["l25h"] == 1 ):
		l25htime = 15
			
	seqtime = 0
	if (options["seq"] == True ):
		seqtime = 15
		
	restoretime = 0
	if (options["restore"] == True ):
		restoretime = 15 * options["i"]
			
	looptime = 0
	if (options["loop"] == False ):
		looptime = 15
		
	reboottime = 0
	if (options["reboot"] == True ):
		reboottime = 315	
	
	# interface up/down, gssp, gossip stop rm, start, sleep, ttl, gossip mode, 
	preparation = (2 * (options["i"] * (2 * 15))) + 15 + 15 + 15 + 15 + 5 + 15 + 15 * gossiptime + olsrtime + l25htime + seqtime + looptime + reboottime + restoretime
	
	monitortime = 0
	if ( options["d"] == True ):
		# monitor config, interface down, dumpcopy
		monitortime = 5 + 5 + 5 + 3 + 5 + 5 + 5 + 5 + 30

	# stop daemon, sync, monitor down, sleep, list dir, copy dir, tar
	finishing = 15 + 15 + monitortime + 30 + 10 + 20 + 10				
	# tag, setup, data, set p
	one_execution = 15 + options["packages"]  + 15
	# dependent on step
	if ((float(options["step"])) == 0.00):
		all_executions = float(one_execution) * 1.0 + preparation + finishing
	else:
		all_executions = (float(one_execution) * 1.0/options["step"]) + preparation + finishing
	return all_executions


def optionscalc(options):
	string = ''
	if (options["step"] > 0.00):
		string = 'steps: ' + str(options["step"])
	if (options["k"] > 0):
		string = string + ' | k: ' 	+ str(options["k"])
	if (options["p2"] > 0.00):
		string = string + ' | p2: ' 	+ str(options["p2"])
	if (options["m"] > 0):
		string = string + ' | m: ' 	+ str(options["m"])
	if (options["n"] > 0):
		string = string + ' | n: ' 	+ str(options["n"])
	if (options["timeout"] > 0):
		string = string + ' | timeount: ' 	+ str(options["timeout"])
	if (options["diameter"] > 0):
		string = string + ' | zone-diameter: ' 	+ str(options["diameter"])
	if (options["p"] > 0.0):
		string = string + ' | p: ' 	+ str(options["p"])
	if (options["hello-interval"] > 0):
		string = string + ' | hello-interval: ' 	+ str(options["hello-interval"])
	if (options["cleanup-interval"] > 0):
		string = string + ' | cleanup-interval: ' 	+ str(options["cleanup-interval"])
	if (options["gossip"] == 7):
		string = string + ' | tmax: ' 	+ str(options["tmax"])
	if (options["gossip"] == 8):
		string = string + ' | p_min: ' 	+ str(options["p_min"])
	if (options["gossip"] == 8):
		string = string + ' | p_max: ' 	+ str(options["p_max"])
	if (options["start"] > 0.01):
		string = string + ' | start: ' 	+ str(options["start"])
	if (options["end"] > 1.01):
		string = string + ' | end: ' 	+ str(options["end"])
	string = string + ' |'																				
	return string

def systemcalc(options):
	sstring = ''
	if (options["d"] == True):
		sstring = sstring + ' | monitor: ' 	+ str(options["d"])
		if len(options["moin"]) > 0:
			sstring = sstring + ' | monitor-interface: ' 	+ str(options["moin"])	
	if (options["o"] == True):
		sstring = sstring + ' | olsr: ' 	+ str(options["o"])
	if (options["5g"] == True):
		sstring = sstring + ' | 5g: ' 	+ str(options["5g"])
	if (options["reboot"] == True):
		sstring = sstring + ' | reboot: ' 	+ str(options["reboot"])
	if (options["eval"] == True):
		sstring = sstring + ' | eval: ' 	+ str(options["eval"])
	if len(sstring) > 1:	
		sstring = sstring + ' |'	
	return sstring
		
#------------------------------------- helper function --------------------------------------------------------------------------------
	
def main():
	try:
		shortopts = "s:vhk:m:g:n:i:cod"
		longopts = ["src=", "help", "packages=", "step=", "timeout=", "p2=", "min-msg-threshold=", "gossip=", "neighbors-threshold=", "interfaces=", "olsr", "monitor", "hello=", "cleanup=", "ttl=", "seq", "noloop" , "diameter=", "eval", "l25h", "batch", "5g", "reboot", "p=", "tmax=", "restore", "pmin=", "pmax=", "start=", "end=", "hosts=", "delayed" , "moin="]
		opts, args = getopt.getopt(sys.argv[1:], shortopts, longopts)
	except getopt.GetoptError, err:
		print(str(err))
		usage()
		sys.exit(os.EX_USAGE)

	options = {}
	options["verbose"] = False
	options["gossip"] = int(0)
	options["packages"] = int(0)
	options["timeout"] = int(0)
	options["step"] = float(0.0)
	options["k"] = int(0)
	options["p2"] = float(0)
	options["m"] = int(0)
	options["n"] = int(0)
	options["i"] = int(1)
	options["o"] = False
	options["d"] = False
	options["hello-interval"] = int(0)
	options["cleanup-interval"] = int(0)
	options["ttl"] = int(0)
	options["seq"] = False
	options["loop"] = True
	options["diameter"] = int(0)
	options["eval"] = False
	options["l25h"] = int(0)
	options["batch"] = False
	options["5g"] = False
	options["reboot"] = False
	options["p"] = float(0.0)
	options["tmax"] = int(31)
	options["restore"] = False
	options["p_min"] = float(0.4)
	options["p_max"] = float(0.9)
	options["start"] = float(0.01)
	options["end"] = float(1.01)
	source = "t9-157t"
	options["hosts"] = ""
	options["delayed"] = False
	options["moin"] = ""
 	for o,v in opts:
		if o == "-h" or o == "--help":
			usage()
			sys.exit(os.EX_USAGE)
		elif o == "-s" or o == "--src":
			source = v
		elif o == "-v":
			options["verbose"] = True
		elif o == "--p2":
			options["p2"] = float(v)
		elif o == "--packages":
			options["packages"] = int(v)
		elif o == "-g" or o == "--gossip":
			options["gossip"] = int(v)
		elif o == "-k":
			options["k"] = int(v)
		elif o == "-m" or o == "--min-msg-threshold":
			options["m"] = int(v)
		elif o == "-n" or o == "--neighbors-threshold":
			options["n"] = int(v)
		elif o == "-i" or o == "--interfaces":
			options["i"] = int(v)	
		elif o == "-o" or o == "--olsr":
			options["o"] = True	
		elif o == "-d" or o == "--monitor":
			options["d"] = True	
		elif o == "--step":
			options["step"] = float(v)
		elif o == "--timeout":
			options["timeout"] = int(v)
		elif o == "--hello":
			options["hello-interval"] = int(v)
		elif o == "--cleanup":
			options["cleanup-interval"] = int(v)						
		elif o == "--ttl":
			options["ttl"] = int(v)		
		elif o == "--seq":
			options["seq"] = True		
		elif o == "--noloop":
			options["loop"] = False
		elif o == "--diameter":
			options["diameter"] = int(v)
		elif o == "--eval":
			options["eval"] = True
		elif o == "--l25h":
			options["l25h"] = int(1)
		elif o == "--batch":
			options["batch"] = True		
		elif o == "--5g":
			options["5g"] = True
		elif o == "--reboot":
			options["reboot"] = True
		elif o == "--p":
			options["p"] = float(v)
		elif o == "--tmax":
			options["tmax"] = int(v)
		elif o == "--restore":
			options["restore"] = True
		elif o == "--pmin":
			options["p_min"] = float(v)
		elif o == "--pmax":
			options["p_max"] = float(v)
		elif o == "--start":
			options["start"] = float(v)
		elif o == "--end":
			options["end"] = float(v)
		elif o == "--hosts":
			options["hosts"] = v
		elif o == "--delayed":
			options["delayed"] = True
		elif o == "--moin":
			options["moin"] = v																								


	lt = time.localtime()
	# Extract time tuple
	year, month, day, hour, min = lt[0:5]
	sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

	# allow logging to file and on screen
	tee = Popen(["tee", "/home/shof/experimente/%04i-%02i-%02i-%02i-%02i-gossip%s-%s-log.txt" %(year, month, day, hour, min, options["gossip"], source)], stdin=PIPE)
	os.dup2(tee.stdin.fileno(), sys.stdout.fileno())
	os.dup2(tee.stdin.fileno(), sys.stderr.fileno())

	if (options["ttl"] > 0 and options["loop"] == True):
		print "\n\033[0;31mWARNING:\033[m Incompatible parameter combination detected, fixing this\n"
		options["loop"] == False

	optionssystem = systemcalc(options)
	print "INFO: Using following system settings: source: %s | hosts: %s | delayed: %d | restore: %d | interface: %d | %s " % (source, options["hosts"], options["delayed"], options["restore"], options["i"], optionssystem)
	
	optionprint = optionscalc(options)
	print "INFO: Using the following daemon parameter: gossip: %d | packages: %d | ttl: %d | seq: %d | loop: %d | l25h: %d | %s" % (options["gossip"], options["packages"], options["ttl"], options["seq"], options["loop"], options["l25h"], optionprint)

	timec = calcTime(options)
	print "QUESTION: experiment will take approx. %d seconds. Continue?" % timec
	finished = datetime.datetime.today() + datetime.timedelta(seconds=timec)
	print "\t finished at: " + str(finished)
	if (options["batch"] == False):
		input = ""
		while True:
			input = raw_input("[y,n]: ")
			if input == "y" or input == "yes":
		   		break;
			elif input == "n" or input == "no":
				sys.exit(1)
				
	selhosts(options["hosts"])			
		
	if (options["reboot"] == True):
		print "INFO: reboot routers"
		cmd_reboot = "/sbin/reboot"
		hostfile= open("%s" % (hosts), "r") 
		for line in hostfile: 
		    #print line
		    do_pssh(cmd_reboot)
		    time.sleep(20)
		hostfile.close()
		time.sleep(300)
		cmd_uptime = "/usr/bin/uptime"
		do_pssh(cmd_uptime)
		if (options["batch"] == False):
			print "\t Do you want to continue?"
			input = ""
			while True:
				input = raw_input("[y,n]: ")
				if input == "y" or input == "yes":
			   		break;
				elif input == "n" or input == "no":
					sys.exit(1)	
	ti = 0	
	while ( ti < 3 ):
		print "INFO: disable wlan%s interfaces on hosts" %(ti)
		state = "down"
		wlan_interface(ti, state)
		ti += 1
		
	print "INFO: disable running olsrd"	
	cmd_check = "/etc/init.d/olsrd stop"
	do_pssh(cmd_check)
	
	if (options["5g"] == False):
		ti = 0
		while ( ti < options["i"] ):
			print "INFO: enable wlan%s interfaces on hosts" % (ti)
			state = "up"
			wlan_interface(ti, state)
			ti += 1
	else:
		state = "up"
		wlan_interface(fiveghz, state)			

	if ( options["d"] == True ):
		print "INFO: start capture interface"
		cmd_mon = "iw dev wlan0 interface add wlanm type monitor"
		do_ssh(options["moin"], cmd_mon)
		cmd_mon2 = "ifconfig wlanm up"
		do_ssh(options["moin"], cmd_mon2)
		lt = time.localtime()
		year, month, day = lt[0:3]
		cmd_tcpdump = "/usr/bin/tshark -i wlanm -w /root/gossip-%s-%04i-%02i-%02i.dump" % (options["moin"], year, month, day)
		do_qssh(options["moin"], cmd_tcpdump)
		time.sleep(3)
		cmd_pid = "pidof tshark"
		do_pissh(options["moin"], cmd_pid)
		
	if ( options["o"] == True ):
		print "INFO: starting olsr daemon do discover amount of hosts"
		cmd_check = "/etc/init.d/olsrd restart"
		do_pssh(cmd_check)
		time.sleep(300)
		cmd_check = "/etc/init.d/olsrd stop"
		do_pssh(cmd_check)
		cmd_route = "route | grep wlan0 | wc -l"
		do_ssh(source,cmd_route)
	
	print "INFO: deleting old log files"
	# New version: delete log also on routers which are not up
	cmd_rm = "rm /var/log/des-gossip-adv.log"
	do_pssh(cmd_rm)	

	print "INFO: restore cell id"
	if (options["restore"] == True):
		if (options["5g"] == False):
			ti = 0
			while ( ti < options["i"] ):
				print "INFO: restoring cell-id wlan%s interfaces on hosts" % (ti)
				state = "down"
				wlan_interface(ti, state)								
				state = "up"
				wlan_interface(ti, state)				
				restore(ti)
				ti += 1
		else:
			state = "down"
			wlan_interface(fiveghz, state)								
			state = "up"
			wlan_interface(fiveghz, state)			
			restore(fiveghz)	

	do_cellssh()
	# Temp kernel 2.6.29 fix
	#cellerror = False
	if (cellerror == True):
		if (options["batch"] == False):
			print "\t Cell mismatch: Do you want to continue or re-reset the cell id?"
			input = ""
			while True:
				print cellerror
				if (cellerror == True):
					input = ""
					input = raw_input("[y,n,r]: ")
					if input == "y" or input == "yes":
				   		break;
					elif input == "n" or input == "no":
						sys.exit(1)
					elif input == "r":
						if (options["5g"] == False):
							ti = 0
							while ( ti < options["i"] ):
								print "INFO: restoring cell-id wlan%s interfaces on hosts" % (ti)
								state = "down"
								wlan_interface(ti, state)								
								state = "up"
								wlan_interface(ti, state)
								restore(ti)								
								ti += 1
						else:
							state = "down"
							wlan_interface(fiveghz, state)								
							state = "up"
							wlan_interface(fiveghz, state)							
							restore(fiveghz)
						do_cellssh()	
				else:
					break
		else:
			print "\t Cell mismatch: Make five attemps to fix the cell-id mismatch!"
			for i in xrange(5):
				print "\tAttemp: %d" % (i+1)
				if (options["5g"] == False):
						ti = 0
						while ( ti < options["i"] ):
							print "INFO: restoring cell-id wlan%s interfaces on hosts" % (ti)
							state = "down"
							wlan_interface(ti, state)								
							state = "up"
							wlan_interface(ti, state)														
							restore(ti)
							ti += 1
				else:
					state = "down"
					wlan_interface(fiveghz, state)								
					state = "up"
					wlan_interface(fiveghz, state)				
					restore(fiveghz)
				do_cellssh()
				if (cellerror == False): break	
					

	print "INFO: starting routing daemon and fix IPv6 messages"
	cmd_clean = "killall -9 des-*"
	do_pssh(cmd_clean)
	cmd_pid = "rm /var/run/des-gossip-adv.pid"
	do_pssh(cmd_pid)
	time.sleep(10)
	cmd_start = "/etc/init.d/des-gossip-adv start && /usr/local/bin/des-gossip-adv-fixipv6.sh tap0" 
	#cmd_start = "/etc/init.d/des-gossip-adv start"
	do_pssh(cmd_start)
			
	print "INFO: waiting for 5s"
	time.sleep(5)

	if ( options["l25h"] == 1 ):
		setl25h(options["l25h"])
				
	if (options["seq"] == True ):
		setseq()
		
	if (options["loop"] == False ):
		setloop(0)		
		
	print "INFO: setting options based on gossip mode"
	setGossip(options["gossip"])
	if (options["gossip"] == 0 ):
		gossip0()
	elif (options["gossip"] == 1 ):	
            	gossip1(options["k"])
        elif (options["gossip"] == 2 ):    	
            	gossip2(options["k"], options["p2"], options["n"], options["hello-interval"], options["cleanup-interval"])
        elif (options["gossip"] == 3 ):    	 
            	gossip3(options["k"], options["m"], options["timeout"])
        elif (options["gossip"] == 4 ):    	 
               	gossip4(options["k"], options["hello-interval"], options["cleanup-interval"], options["diameter"])
        elif (options["gossip"] == 5 ): 
              	gossip5(options["k"], options["timeout"], options["p2"], options["hello-interval"], options["cleanup-interval"])
        elif (options["gossip"] == 6 ):    	 
            	gossip6(options["m"], options["timeout"], options["k"])      	
        elif (options["gossip"] == 7 ):    	 
            	gossip7(options["m"], options["tmax"]) 
        elif (options["gossip"] == 8 ):    	 
            	gossip8(options["hello-interval"], options["cleanup-interval"], options["p_min"], options["p_max"]) 
        elif (options["gossip"] == 9 ):    	 
            	gossip9(options["n"], options["hello-interval"], options["cleanup-interval"])
        elif (options["gossip"] == 11 ):    	 
            	gossip6(options["m"], options["timeout"], options["k"])               	             	
	
 	if (options["delayed"] == True and (options["gossip"] == 2 or options["gossip"] == 4 or options["gossip"] == 5 or options["gossip"] == 8 or options["gossip"] == 9)):
 		print "INFO: Waiting %d seconds to run neighborhood discover after timeout it will be stopped" % (delayedtimer)
		time.sleep(delayedtimer)
		stopneighbormon()
	if (options["ttl"] > 0):
		setttl(options["ttl"])	
				
	step = options["step"]
	packages = options["packages"]
	# step block
	if (options["step"] == float(0.0) and options["gossip"] < 8):
			p = options["p"]
			setTag(source, "%.3f"%(p))
			setP(p)
			ping(source,packages, dest)
	elif (options["gossip"] >= 8):
			setTag(source, "adaptive")
			ping(source,packages, dest)	
	else:	
		if options["start"] > float(0.01):
			p = options["start"]
		else:
			p = step
				
		while (p < options["end"]):
			setTag(source, "%.3f"%(p))
			setP(p)
			ping(source,packages, dest)
			p += step
	# wait 30s for cached packets				
	if (options["gossip"] == 3 or options["gossip"] == 5 or options["gossip"] == 6 or options["gossip"] == 7 or options["gossip"] == 11):
		time.sleep(30);
	print "INFO: stopping routing daemon"	
	cmd_stop = "/etc/init.d/des-gossip-adv stop"
	do_pssh(cmd_stop)
	print "INFO: disable wlan interfaces on hosts"
	ti -= 1	
	while ( ti > -1 ):
		state = "down"
		wlan_interface(ti, state)
		ti -= 1	
		
	if ( options["d"] == True ):
		# terminate() only > Python 2.5
		cmd_kill = "kill -9 %s" % (pid)
		do_ssh(options["moin"],cmd_kill)
		cmd_wlanm_down = "ifconfig wlanm down"
		do_ssh(options["moin"],cmd_wlanm_down)
		cmd_mondel = "iw dev wlanm del"
		do_ssh(options["moin"],cmd_mondel)
	
	print "INFO: sync logfiles"
	cmd_sync = "sync" 
	do_pssh(cmd_sync)
	time.sleep(30)	
	
	lt = time.localtime()
	# Extract time tuple
	year, month, day = lt[0:3]
	# catch IOErrors like file exists or full filesystem
	# logfiles are real big :>
	try: 
		print "INFO: copy log-files"
		if (os.path.isdir("%s/logs/%04i-%02i-%02i/%s" % (userpath, year, month, day, options["gossip"]))):
			try:
		               	print "INFO: make a backup of old files"
		               	rcount = random.uniform(1, 10)  # Random float x, 1.0 <= x < 10.0
		               	copytree("%s/logs/%04i-%02i-%02i/%s" % (userpath, year, month, day, options["gossip"]), "%s/logs/%04i-%02i-%02i/%s-backup-%f" % (userpath, year, month, day, options["gossip"], rcount))
				rmtree("%s/logs/%04i-%02i-%02i/%s" % (userpath, year, month, day, options["gossip"]), True) 
		        except (IOError, os.error), why:
		        	errors.append((str(why)))	
			
		routers = os.listdir("/testbed/export/data")
		re_host = re.compile(r"(t|a){1}\d-k*\d{2,3}[a-z]*")
		for router in routers:
	   		if not re_host.match(router):
				print "INFO:\t\tno match for dir: %s" % (router)
	   			continue
	   		file = "/testbed/export/data/" + router + "/var/log/des-gossip-adv.log"
	  		try:
	   			ifile = open(file, 'r')
	   		except IOError, msg:
				print "\033[0;31mWARNING:\033[m \tUnable to open file %s" % file
				continue
	   		os.makedirs("%s/logs/%04i-%02i-%02i/%d/%s" % (userpath, year, month, day, options["gossip"], router))
	   		time.sleep(1)
	   		file2 = "%s/logs/%04i-%02i-%02i/%d/%s/des-gossip-adv.log" % (userpath, year, month, day, options["gossip"], router)
	  	 	os.system ("cp %s %s" % (file, file2))
	except (IOError, os.error):
		print "\033[0;31mERORR:\033[m Logfiles not copied!"		  	 	
	
	if ( options["d"] == True ):
		try:
			dump = "/testbed/export/rootfs/root/gossip-%04i-%02i-%02i.dump" % (year, month, day)
			os.system ("cp %s %s" % (dump, userpath))
		except (IOError, os.error):
			print "\033[0;31mWARNING:\033[m \tUnable to open file %s" % (dump)
			
	try:	
		s = os.statvfs('/home')
		free_space =(s.f_bavail * s.f_frsize) / 1024
		#print "Space: %s" %(free_space)
		if (free_space <= 3000000):
			print "\033[0;31mWARNING:\033[m \t Only %s MB avaible!" % (free_space / (1024))
			print "Cleaning up old files"
			try:
				os.remove("%s/*.tar") % (userpath)
			except (IOError, os.error):
				print "\033[0;31mWARNING:\033[m \t Could not delete any files"	
					
		print "INFO: tar log-files"
		tar = tarfile.open("%s/%04i-%02i-%02i-gossip%s-%s.tar" % (userpath, year, month, day, options["gossip"], source), "w" )
		tar.add("../logs/%04i-%02i-%02i/%s/" % (year, month, day, options["gossip"]))
		tar.close()
	except (IOError, os.error):
		print "\033[0;31mWARNING:\033[m \tUnable to tar files"
	
	sendok = shof.sendlog(options["gossip"], userpath, options["batch"], source)
		
	if (sendok == True):
		try:
			# delete old log
			os.remove("%s/%04i-%02i-%02i-gossip%s-%s.tar" % (userpath, year, month, day, options["gossip"], source))
			removeDir("%s/logs/%04i-%02i-%02i/%s" % (userpath, year, month, day, options["gossip"]))
		except (IOError, os.error):
			print "\033[0;31mWARNING:\033[m \tLog file not deleted"
		
	if ( options["eval"] == True ):
		try:
			print "INFO: starting evaluation"
			file_name= "%s-%d-%04i-%02i-%02i" % (source, options["gossip"],year ,month ,day)
			os.system ("%s/python/logeval.py -s %s -o %s.pdf" % (userpath, source, file_name))
		except (IOError, os.error):
			print "\033[0;31mWARNING:\033[m \tCould not open file: %s" % (file_name)
	else:
		print "INFO: cleanup logfiles"
		cmd_rm = "rm /var/log/des-gossip-adv.log"
		do_pssh(cmd_rm)		
		
	print "INFO: FINISHED"
	 	
if __name__ == "__main__":
  main()
