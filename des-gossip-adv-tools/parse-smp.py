#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
import gzip
import multiprocessing
from multiprocessing import Pool, Lock
import os, sys
import re
import datetime
import time

import matplotlib
matplotlib.use('Agg')
from helpers import commits
from index import create_indexes

re_time = re.compile(r"^(\d{4})-(\d{1,2})-(\d{1,2}) (\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?$")

_re_time = r'\d{4}-\d{1,2}-\d{1,2} \d{1,2}:\d{1,2}:\d{1,2}(?:\.\d{1,3})?'
_re_func = r'\s*(?:\(.*@.*\))?'
_re_mac = r'\w{2}:\w{2}:\w{2}:\w{2}:\w{2}:\w{2}'

# received data packet
# [rx] host=%s, if=%M (%s), prev=%M, src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x, hops=%d
re_rx_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[rx\] host=(.*), if=(%s) \((\w+)\), prev=(%s), src=(%s), dst=(%s), ttl=(\d+), seq=(\d+), flags=(\w+), hops=(\d+)(?:, rssi=(-?\d+))?%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_mac, _re_func))

# received HELLO
# [rh] host=%s, if=%M (%s), prev=%M, src=%M, ttl=%d, seq=%d, flags=%#x, hops=%d
re_rh_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[rh\] host=(.*), if=(%s) \((\w+)\), prev=(%s), src=(%s), ttl=(\d+), seq=(\d+), flags=(\w+), hops=(\d+)(?:, rssi=(-?\d+))?%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_func))

# packet sent by the source
# [tx] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x
re_tx_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[tx\] host=(.*), if=(%s) \((\w+)\), src=(%s), dst=(%s), ttl=(\d+), seq=(\d+), flags=(\w+)%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_func))

# forwarded data packet
# [fw] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x
re_fw_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[fw\] host=(.*), if=(%s) \((\w+)\), src=(%s), dst=(%s), ttl=(\d+), seq=(\d+), flags=(\w+)%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_func))

# forwarded HELLO packet
# [fh] host=%s, if=%M (%s), prev=%M, src=%M, ttl=%d, seq=%d, flags=%#x
re_fh_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[fh\] host=(.*), if=(%s) \((\w+)\), prev=(%s), ttl=(\d+), seq=(\d+), flags=(\w+)%s$" % (_re_time, _re_mac, _re_mac, _re_func))

# HELLO packet sent by source
# [he] host=%s, if=%M (%s), ttl=%d, seq=%d, flags=%#x
re_he_new = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[he\] host=(.*), if=(%s) \((\w+)\), ttl=(\d+), seq=(\d+), flags=(\w+)%s$" % (_re_time, _re_mac, _re_func))

# tag inserted between individual configurations
# [tag] host=%s, id=%s, p=%f, p2=%f, k=%d, n=%d, m=%d, timeout=%d, gossip=%d, helloTTL=%d, hello_interval=%d, helloSize=%d, cleanup_interval=%d, T_MAX=%d, p_min=%f, p_max=%f
re_tag = re.compile(r"^(%s)\+\d{2}\.\d+ INFO:\s*\[tag\] host=(.*), id=(.+), p=(.+), p2=(.+), k=(\d+), n=(\d+), m=(\d+), timeout=(\d+), gossip=(\d+), helloTTL=(\d+), hello_interval=(\d+), helloSize=(\d+), cleanup_interval=(\d+), T_MAX=(\d+), p_min=(\d\.\d+), p_max=(\d\.\d+), nhdp_hi=(\d+), nhdp_ht=(\d+), mpr_minpdr=(\d\.\d+)(?:, restarts=(-?\d+))?(?:, tau=(-?\d+))?%s$" % (_re_time, _re_func))

# current value of the adaptive propability
# [ap] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x, p=%f
re_ap = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[ap\] host=(.*), if=(%s) \((\w+)\), src=(%s), dst=(%s), ttl=(\d+), seq=(\d+), flags=(\w+), p=(.+)%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_func))

# NHDP/MPR HELLO containing information about number of neighbors and number of chosen mprs
# [nhdp_he] host=t9-155, if=00:1f:1f:09:06:4f (wlan0), ttl=1, seq=2373, flags=0x42, n=6, mpr=4
re_nhdp_he = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[nhdp_he\] host=(.*), if=(%s) \((\w+)\), ttl=(\d+), seq=(\d+), flags=(\w+), n=(\d+), n2=(\d+), mpr=(\d+), mprs=(\d+), size=(\d+)%s$" % (_re_time, _re_mac, _re_func))
# received NHDP/MPR HELLO
re_nhdp_rh = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[nhdp_rh\] host=(.*), if=(%s) \((\w+)\), prev=(%s), src=(%s), ttl=(\d+), seq=(\d+), flags=(\w+)(?:, hops=(-?\d+))?(?:, rssi=(-?\d+))?%s$" % (_re_time, _re_mac, _re_mac, _re_mac, _re_func))
re_nhdp_mprsel = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[mprsel\].*%s$" % (_re_time, _re_func))

re_nhdp_n2 = re.compile(r"(%s)\+\d{2}\.\d+ INFO:\s*\[n2\].*%s$" % (_re_time, _re_func))

def mac2int(mac):
    octets = [int(o, 16) << 8*i for i, o in enumerate(mac.split(':'))]
    return sum(octets)

#@commits
def parse_file((logfile, options, lock)):
    """
    Parses a single log file
    """
    nhdp_host = ''
    start = options["start"]
    end = options["end"]
    conn = sqlite3.connect(options['db'], timeout=3600)
    c = conn.cursor()
    curr_tag_key = None
    store_dst = False
    truncate_ifnames = True
    tags = 0
    data = dict()
    for l in ['rx', 'tx', 'fw', 'he', 'rh', 'fh', 'nhdp_he', 'nhdp_mpr_selectors', 'nhdp_mpr_n2']:
        data[l] = list()

    if logfile.endswith('.gz'):
        ifile = gzip.open(logfile, 'r')
    else:
        ifile = open(logfile, 'r')
    log_host = ifile.name.split('/')[-2]
    logger = multiprocessing.get_logger()
    ch = logging.StreamHandler()
    ch.setLevel(options['loglevel'])
    formatter = logging.Formatter('%(levelname)s [%(funcName)s] %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)

    counter = 0
    logger.info('(%s) parsing...', log_host)
    for num, line in enumerate(ifile):
        logger.debug('parsing line: %s', line)
        if line[0] == ' ':
            logging.warning('skipping line')
            continue

        rx_match = re_rx_new.match(line)
        if rx_match:
            logger.debug('rx_match: %s', line)
            #           2       3   4         5       6       7       8       9         10       11
            # [rx] host=%s, if=%M (%s), prev=%M, src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x, hops=%d
            time = parseTime(rx_match.group(1))
            if start > time or time > end:
                continue
            host = rx_match.group(2)
            rx_if = mac2int(rx_match.group(3))
            rx_ifname = rx_match.group(4)
            if truncate_ifnames:
                rx_ifname = rx_ifname.replace('wlan', '')
            prev = mac2int(rx_match.group(5))
            src = mac2int(rx_match.group(6))
            dst = None
            if store_dst:
                dst = mac2int(rx_match.group(7))
            seq = int(rx_match.group(9))
            hops = int(rx_match.group(11))
            try:
                rssi = int(rx_match.group(12))
            except TypeError:
                rssi = None
            t = (time, host, rx_if, rx_ifname, prev, src, dst, seq, hops, rssi)
            data['rx'].append(t)
            counter += 1
            continue

        rh_match = re_rh_new.match(line)
        if rh_match:
            logger.debug('rh_match: %s', line)
            #           2       3   4         5       6       7       8       9         10
            # [rh] host=%s, if=%M (%s), prev=%M, src=%M, ttl=%d, seq=%d, flags=%#x, hops=%d
            time = parseTime(rh_match.group(1))
            if start > time or time > end:
                continue
            host = rh_match.group(2)
            rh_if = mac2int(rh_match.group(3))
            rh_ifname = rh_match.group(4)
            if truncate_ifnames:
                rh_ifname = rh_ifname.replace('wlan', '')
            prev = mac2int(rh_match.group(5))
            src = mac2int(rh_match.group(6))
            seq = int(rh_match.group(8))
            hops = int(rh_match.group(10))
            try:
                rssi = int(rh_match.group(11))
            except TypeError:
                rssi = None
            t = (time, host, rh_if, rh_ifname, prev, src, seq, hops, rssi)
            data['rh'].append(t)
            counter += 1
            continue

        fw_match = re_fw_new.match(line)
        if fw_match:
            logger.debug('fw_match: %s', line)
            #           2       3   4        5       6       7       8          9
            # [fw] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x
            time = parseTime(fw_match.group(1))
            if start > time or time > end:
                continue
            host = fw_match.group(2)
            tx_if = mac2int(fw_match.group(3))
            tx_ifname = fw_match.group(4)
            if truncate_ifnames:
                tx_ifname = tx_ifname.replace('wlan', '')
            src = mac2int(fw_match.group(5))
            dst = None
            if store_dst:
                dst = mac2int(fw_match.group(6))
            seq = int(fw_match.group(8))
            t = (time, host, tx_if, tx_ifname, src, dst, seq)
            data['fw'].append(t)
            counter += 1
            continue

        tx_match = re_tx_new.match(line)
        if tx_match:
            logger.debug('tx_match: %s', line)
            #           2       3   4       5       6       7       8           9
            # [tx] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x
            time = parseTime(tx_match.group(1))
            if start > time or time >  end:
                continue
            if curr_tag_key == None:
                logger.warning('skipping tx packet, tag not set')
                continue
            host = tx_match.group(2)
            tx_if = mac2int(tx_match.group(3))
            tx_ifname = tx_match.group(4)
            if truncate_ifnames:
                tx_ifname = tx_ifname.replace('wlan', '')
            src = mac2int(tx_match.group(5))
            dst = None
            if store_dst:
                dst = mac2int(tx_match.group(6))
            seq = int(tx_match.group(8))
            t = (time, host, tx_if, tx_ifname, src, dst, seq, curr_tag_key)
            data['tx'].append(t)
            counter += 1
            continue

        fh_match = re_fh_new.match(line)
        if fh_match:
            logger.debug('fh_match: %s', line)
            #           2       3   4        5       6       7       8          9
            # [fh] host=%s, if=%M (%s), prev=%M, src=%M, ttl=%d, seq=%d, flags=%#x
            time = parseTime(fh_match.group(1))
            if start > time or time >  end:
                continue
            host = fh_match.group(2)
            tx_if = mac2int(fh_match.group(3))
            tx_ifname = fh_match.group(4)
            if truncate_ifnames:
                tx_ifname = tx_ifname.replace('wlan', '')
            prev = mac2int(fh_match.group(5))
            src = fh_match.group(6)
            seq = int(fh_match.group(8))
            t = (time, host, tx_if, tx_ifname, src, dst, seq)
            data['fh'].append(t)
            counter += 1
            continue

        nhdp_he_match = re_nhdp_he.match(line)
        if nhdp_he_match:
            logger.debug('nhdp_he_match: %s', line)
            #                2       3   4        5       6          7     8     9     10       11       12
            # [nhdp_he] host=%s, if=%M (%s), ttl=%d, seq=%d, flags=%#x, n=%d, n2=%d, mpr=%d, mprs=%d, size=%d
            time = parseTime(nhdp_he_match.group(1))
            if start > time or time > end:
                continue
            if curr_tag_key == None:
                logging.debug('discarding matched nhdp_he because of empty tag_key')
                continue
            host = nhdp_he_match.group(2)
            nhdp_host = host
            tx_if = mac2int(nhdp_he_match.group(3))
            tx_ifname = nhdp_he_match.group(4)
            seq = int(nhdp_he_match.group(6))
            num_n1 = int(nhdp_he_match.group(8))
            num_n2 = int(nhdp_he_match.group(9))
            num_mpr = int(nhdp_he_match.group(10))
            num_mprs = int(nhdp_he_match.group(11))
            size = int(nhdp_he_match.group(12))

            t1 = (time, host, tx_if, tx_ifname, seq, curr_tag_key)
            t2 = (time, host, seq, num_n1, num_n2, num_mpr, num_mprs, size, curr_tag_key)
            data['he'].append(t1)
            data['nhdp_he'].append(t2)
            counter += 1
            continue

        nhdp_rh_match = re_nhdp_rh.match(line)
        if nhdp_rh_match:
            logger.debug('nhdp_rh_match: %s', line)
            #              2       3   4         5       6       7       8       9         10
            # [nhdp_rh] host=%s, if=%M (%s), prev=%M, src=%M, ttl=%d, seq=%d, flags=%#x, hops=%d
            time = parseTime(nhdp_rh_match.group(1))
            if start > time or time > end:
                continue
            host = nhdp_rh_match.group(2)
            nhdp_host = host
            rh_if = mac2int(nhdp_rh_match.group(3))
            rh_ifname = nhdp_rh_match.group(4)
            if truncate_ifnames:
                rh_ifname = rh_ifname.replace('wlan', '')
            prev = mac2int(nhdp_rh_match.group(5))
            src = mac2int(nhdp_rh_match.group(6))
            seq = int(nhdp_rh_match.group(8))
            try:
                hops = int(nhdp_rh_match.group(10))
            except TypeError:
                hops = 0
            try:
                rssi = int(nhdp_rh_match.group(11))
            except TypeError:
                rssi = None
            t = (time, host, rh_if, rh_ifname, prev, src, seq, hops, rssi)
            data['rh'].append(t)
            counter += 1
            continue

        re_nhdp_mprsel_match = re_nhdp_mprsel.match(line)
        if re_nhdp_mprsel_match:
            time = parseTime(re_nhdp_mprsel_match.group(1))
            mpr_selectors = re.findall(r"(%s)" % _re_mac, line)
            if curr_tag_key == None:
                continue
            if nhdp_host == '':
                logger.warning("nhdp_host not set! skipping.")
                continue
            if start > time or time > end:
                continue
            if len(mpr_selectors) == 0:
                t = (time, nhdp_host, 0, curr_tag_key)
                data['nhdp_mpr_selectors'].append(t)
                continue
            for mpr_selector in mpr_selectors:
                t = (time, nhdp_host, mac2int(mpr_selector), curr_tag_key)
                data['nhdp_mpr_selectors'].append(t)
            counter += 1
            continue

        re_nhdp_n2_match = re_nhdp_n2.match(line)
        if re_nhdp_n2_match:
            time = parseTime(re_nhdp_n2_match.group(1))
            n2s = re.findall(r"(%s)" % _re_mac, line)
            if curr_tag_key == None:
                continue
            if nhdp_host == '':
                logger.warning("nhdp_host not set! skipping.")
                continue
            if start > time or time > end:
                continue
            if len(n2s) == 0:
                t = (time, nhdp_host, 0, curr_tag_key)
                continue
            for n2 in n2s:
                t = (time, nhdp_host, mac2int(n2), curr_tag_key)
                data['nhdp_mpr_n2'].append(t)
            counter += 1
            continue

        he_match = re_he_new.match(line)
        if he_match:
            logging.debug('he_match')
            #           2       3   4       5       6           7
            # [he] host=%s, if=%M (%s), ttl=%d, seq=%d, flags=%#x
            time = parseTime(he_match.group(1))
            if start > time or time >  end:
                continue
            if curr_tag_key == None:
                continue
            host = he_match.group(2)
            tx_if = mac2int(he_match.group(3))
            tx_ifname = he_match.group(4)
            if truncate_ifnames:
                tx_ifname = tx_ifname.replace('wlan', '')
            seq = int(he_match.group(6))
            t = (time, host, tx_if, tx_ifname, seq, curr_tag_key)
            data['he'].append(t)
            counter += 1
            continue

        tag_match = re_tag.match(line)
        if tag_match:
            logger.debug('tag_match: %s', line)
            #            2      3      4      5     6     7     8           9         10           11                 12          13        14                   15        16      17          18         19           20
            # [tag] host=%s, id=%s, p=%f, p2=%f, k=%d, n=%d, m=%d, timeout=%d, gossip=%d, helloTTL=%d, hello_interval=%d, helloSize=%d, cleanup_interval=%d, T_MAX=%d, p_min=%f, p_max=%f, nhdp_hi=%d, nhdp_ht=%d, mpr_minpd=%f
            time = parseTime(tag_match.group(1))
            if start > time or time >  end:
                continue
            host = tag_match.group(2)
            tag_id = tag_match.group(3)
            p = float(tag_match.group(4))
            p2 = float(tag_match.group(5))
            k = int(tag_match.group(6))
            n = int(tag_match.group(7))
            m = int(tag_match.group(8))
            timeout = int(tag_match.group(9))
            gossip = int(tag_match.group(10))
            helloTTL = int(tag_match.group(11))
            hello_interval = int(tag_match.group(12))
            helloSize = int(tag_match.group(13))
            cleanup_interval = int(tag_match.group(14))
            T_MAX = int(tag_match.group(15))
            p_min = float(tag_match.group(16))
            p_max = float(tag_match.group(17))
            nhdp_hi = int(tag_match.group(18))
            nhdp_ht = int(tag_match.group(19))
            mpr_minpdr = float(tag_match.group(20))
            try:
                restarts = int(tag_match.group(21))
            except TypeError:
                restarts = 0
            try:
                tau = int(tag_match.group(22))
            except TypeError:
                tau = 0

            t = (None, time, tag_id, p, p2, k, n, m, timeout, gossip, helloTTL, hello_interval, helloSize, cleanup_interval, T_MAX, p_min, p_max, None, None, None, None, nhdp_hi, nhdp_ht, mpr_minpdr, restarts, tau)
            lock.acquire()
            c.execute('''
                SELECT key
                FROM tag
                WHERE id=?
                AND p=?
                AND p2=?
                AND k=?
                AND n=?
                AND m=?
                AND timeout=?
                AND gossip=?
                AND helloTTL=?
                AND hello_interval=?
                AND helloSize=?
                AND cleanup_interval=?
                AND T_MAX=?
                AND p_min=?
                AND p_max=?
                AND nhdp_hi=?
                AND nhdp_ht=?
                AND mpr_minpdr=?
                AND restarts=?
                AND tau=?
                ''',
                (tag_id, p, p2, k, n, m, timeout, gossip, helloTTL, hello_interval, helloSize, cleanup_interval, T_MAX, p_min, p_max, nhdp_hi, nhdp_ht, mpr_minpdr, restarts, tau))
            tag_entry = c.fetchone()
            if not tag_entry:
                c.execute('INSERT INTO tag VALUES (' + ','.join(['?' for _i in xrange(len(t))]) + ')', t)
                curr_tag_key = c.lastrowid
                conn.commit()
            else:
                curr_tag_key = tag_entry[0]
                tags += 1
            lock.release()
            counter += 1
            continue

        logging.debug('unmatched line: %s', line)

        #ap_match = re_ap.match(line)
        #if ap_match:
            #logger.debug('ap_match: %s', line)
            ##           2       3   4       5       6       7       8           9    10
            ## [ap] host=%s, if=%M (%s), src=%M, dst=%M, ttl=%d, seq=%d, flags=%#x, p=%f
            #time = parseTime(ap_match.group(1))
            #if start > time or time > end:
                #continue
            #host = ap_match.group(2)
            #tx_if = ap_match.group(3)
            #tx_ifname = ap_match.group(4)
            #src = ap_match.group(5)
            #dst = ap_match.group(6)
            #seq = int(ap_match.group(8))
            #ap = float(ap_match.group(10))
            #continue

    logger.info("parsed %d lines for (%s)" % (counter, log_host))
    if 'host' not in locals():
        logging.error('hostname not set: aborting')
        #sys.exit(1) # does not work with multiprocessing
        assert(0)
    if curr_tag_key == None:
        logging.error('curr_tag_key not set: aborting')
        #sys.exit(1) # does not work with multiprocessing
        assert(0)

    try:
        c.execute('INSERT INTO addr VALUES (?)',  (host,))
    except sqlite3.IntegrityError:
        pass
    for table in data.keys():
        dd = data[table]
        if len(dd) == 0:
            continue
        columns = len(dd[0])
        sql = 'INSERT INTO %s VALUES (%s)' % (table, ','.join(['?' for x in xrange(columns)]))
        for d in dd:
            try:
                c.execute(sql, d)
            except sqlite3.IntegrityError, err:
                logging.debug("Skipped sql: %s reason: %s" % (sql, err))
                continue
    conn.commit()
    logger.info("(%s) finished.", log_host)
    logger.removeHandler(ch)

def parse_files(options):
    """
    Parses multiple logfiles in subdirectories
    """
    logging.info('parsing files')
    log_root_dir = options['log_root_dir']
    routers = os.listdir(log_root_dir)
    pool = Pool(processes=options['processes'])
    logfiles = list()
    manager = multiprocessing.Manager()
    lock = manager.Lock()

    for i, router in enumerate(sorted(routers)):
        logging.info('testing %s (%d/%d)', router, i+1, len(routers))
        logfile = "%s/%s/%s.log" % (log_root_dir, router, options['daemon'])
        try:
            infile = open(logfile, 'r')
            logfiles.append((logfile, options, lock))
        except IOError, msg:
            try:
                logfile = logfile + '.gz'
                infile = gzip.open(logfile, 'rb')
                logfiles.append((logfile, options, lock))
            except IOError, msg:
                logging.warning('Unable to open file %s', logfile)
                continue

    infile.close()
    pool.map(parse_file, logfiles)

def parseTime(datestring):
    """
    Parse time from log message and return Python time object
    """
    match = re_time.match(datestring)
    if match:
        if match.group(7) == None:
            ms = 0
        else:
            ms = match.group(7)
        dt = datetime.datetime(year=int(match.group(1)), month=int(match.group(2)), day=int(match.group(3)), hour=int(match.group(4)), minute=int(match.group(5)), second=int(match.group(6)))
        return time.mktime(dt.timetuple())+int(ms)/1000.0
    else:
        logging.warning('time unspecified or wrong format %s' % (time))
        return None

def main():
    parser = argparse.ArgumentParser(description='Parse log files and store data in sqlite database')
    parser.add_argument('--start', default='2000-01-01 00:00:00', help='Evaluate only log messages after this timestamp')
    parser.add_argument('--end', default='2020-12-31 23:59:59', help='Evaluate only log messages before this timestamp')
    parser.add_argument('--db', default='./sqlite.db', help='name of the database where the parsed data shall be stored')
    parser.add_argument('--daemon', default='des-gossip-adv', help='name of the daemon that generated the log files')
    parser.add_argument('--logs', required=True, help='root directory where the logs are stored')
    parser.add_argument('--processes', '-p', type=int, default=1, help='number of parallel processes')
    parser.add_argument('--append', '-a', type=bool, default=False, help='Whether to append data to existing tables or not.')
    parser.add_argument('--sqlsync', default='FULL', help='Value of the PRAGMA synchronous, handed to the sqlite db. (OFF|NORMAL|FULL)')
    args = parser.parse_args()

    options = {}
    options["start"] = parseTime(args.start)
    options["end"] = parseTime(args.end)
    options['log_root_dir'] = args.logs
    options['daemon'] = args.daemon
    options['processes'] = args.processes
    options['loglevel'] = logging.INFO
    options['append'] = args.append
    options['sqlsync'] = args.sqlsync
    logging.basicConfig(level=options['loglevel'], format='%(levelname)s [%(funcName)s] %(message)s')

    logging.info('connecting to database')
    conn = sqlite3.connect(args.db)
    options['db'] = args.db
    c = conn.cursor()

    create_tables(conn, options)
    parse_files(options)
    create_indexes(conn)
    c.close()

def create_tables(conn, options):
    c = conn.cursor()
    logging.info('creating tables')
    ################################################################################
    # Tables for data
    ################################################################################
    if options['sqlsync'] == 'OFF':
        logging.warning('sqlite synchronous mode set to OFF! (check your power supply ;))')
        c.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        logging.warning('sqlite synchronous mode set to NORMAL! (check your power supply ;))')
        c.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    try:
        c.execute(''' \
            CREATE TABLE rx ( \
                time REAL NOT NULL, \
                host TEXT NOT NULL, \
                rx_if INTEGER NOT NULL, \
                rx_ifname TEXT NOT NULL, \
                prev INTEGER NOT NULL, \
                src INTEGER NOT NULL, \
                dst INTEGER, \
                seq INTEGER NOT NULL, \
                hops INTEGER NOT NULL, \
                rssi INTEGER \
            )''')
    except sqlite3.OperationalError:
        if options["append"] == False:
            logging.critical("Appending to existing tables disabled. Aborting.")
            sys.exit(0)
        else:
            logging.info("Appending data to existing tables! Waiting 10 seconds from now.")
            time.sleep(10)

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS tx ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            tx_if INTEGER NOT NULL, \
            tx_ifname TEXT NOT NULL, \
            src INTEGER NOT NULL, \
            dst INTEGER, \
            seq INTEGER NOT NULL, \
            tag INTEGER NOT NULL, \
            FOREIGN KEY(tag) REFERENCES tag(key), \
            FOREIGN KEY(host) REFERENCES addr(host), \
            PRIMARY KEY(time, host, seq) \
        )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS fw ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            tx_if INTEGER NOT NULL, \
            tx_ifname TEXT NOT NULL, \
            src INTEGER NOT NULL, \
            dst INTEGER, \
            seq INTEGER NOT NULL
        )''')

    ################################################################################
    # Tables for HELLOs
    ################################################################################
    c.execute(''' \
        CREATE TABLE IF NOT EXISTS rh ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            rx_if INTEGER NOT NULL, \
            rx_ifname TEXT NOT NULL, \
            prev INTEGER NOT NULL, \
            src INTEGER NOT NULL, \
            seq INTEGER NOT NULL, \
            hops INTEGER NOT NULL, \
            rssi INTEGER \
        )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS fh ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            tx_if INTEGER NOT NULL, \
            tx_ifname TEXT NOT NULL, \
            prev INTEGER NOT NULL, \
            src INTEGER NOT NULL, \
            seq INTEGER NOT NULL, \
            PRIMARY KEY(host, src, seq) \
        )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS he ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            tx_if INTEGER NOT NULL, \
            tx_ifname TEXT NOT NULL, \
            seq INTEGER NOT NULL, \
            tag INTEGER NOT NULL, \
            FOREIGN KEY(tag) REFERENCES tag(key), \
            PRIMARY KEY(time, host, seq) \
        )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS nhdp_he ( \
              time REAL NOT NULL, \
              host TEXT NOT NULL, \
              seq INTEGER NOT NULL, \
              num_n1 INTEGER NOT NULL, \
              num_n2 INTEGER NOT NULL, \
              num_mpr INTEGER NOT NULL, \
              num_mprs INTEGER NOT NULL, \
              size INTEGER NOT NULL, \
              tag INTEGER NOT NULL, \
              FOREIGN KEY(tag) REFERENCES tag(key), \
              PRIMARY KEY(time, host, seq) \
             )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS nhdp_mpr_selectors ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            mprselector INTEGER NOT NULL, \
            tag_key INTEGER NOT NULL, \
            PRIMARY KEY(time, host, mprselector), \
            FOREIGN KEY(tag_key) REFERENCES tag(key)
            )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS nhdp_mpr_n2 ( \
            time REAL NOT NULL, \
            host TEXT NOT NULL, \
            n2 INTEGER NOT NULL, \
            tag_key INTEGER NOT NULL, \
            PRIMARY KEY(time, host, n2), \
            FOREIGN KEY(tag_key) REFERENCES tag(key)
            )''')


    ################################################################################
    # Tables for Management
    ################################################################################
    # inserted at the beginning of an individual experiment, configuration of all parameters
    c.execute(''' \
        CREATE TABLE IF NOT EXISTS tag ( \
            key INTEGER PRIMARY KEY, \
            time REAL NOT NULL, \
            id TEXT NOT NULL, \
            p real NOT NULL, \
            p2 real NOT NULL, \
            k INTEGER NOT NULL, \
            n INTEGER NOT NULL, \
            m INTEGER NOT NULL, \
            timeout INTEGER NOT NULL, \
            gossip INTEGER NOT NULL, \
            helloTTL INTEGER NOT NULL, \
            hello_interval INTEGER NOT NULL, \
            helloSize INTEGER NOT NULL, \
            cleanup_interval INTEGER NOT NULL, \
            T_MAX INTEGER NOT NULL, \
            p_min real NOT NULL, \
            p_max real NOT NULL, \
            pkg_size INTEGER, \
            pkg_interval REAL, \
            jitter REAL, \
            channel INTEGER, \
            nhdp_hi INTEGER, \
            nhdp_ht INTEGER, \
            mpr_minpdr REAL, \
            restarts INTEGER, \
            tau INTEGER \
        )''')

    c.execute(''' \
        CREATE TABLE IF NOT EXISTS addr ( \
            host TEXT NOT NULL, \
            PRIMARY KEY(host)
        )''')
    conn.commit()

if __name__ == "__main__":
  main()
