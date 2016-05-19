#!/usr/bin/python

import re
import logging
import os, sys
import sqlite3
import datetime
import argparse
import matplotlib
matplotlib.use('Agg')
import matplotlib.cm as cm
from helpers import MyFig
import numpy
import pylab

# shorter version tcp, longer version udp with jtter, etc
re_server_result = re.compile(r'^(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d+),(\d+\.\d+\-\d+\.\d+),(\d+),(\d+)(?:,(\d+\.\d+),(\d+),(\d+),(\d+\.\d+),(\d+))?$')

re_client_sent = re.compile(r'^(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d+),(\d+\.\d+\-\d+\.\d+),(\d+),(\d+)$')

re_client_result = re.compile(r'^(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}),(\d+),(\d+),(\d+\.\d+\-\d+\.\d+),(\d+),(\d+)(?:,(\d+\.\d+),(\d+),(\d+),(\d+\.\d+),(\d+))?$')

fu_green = '#99CC00'
fu_blue = '#003366'
fu_red = '#CC0000'

def parse_files(options):
    """
    Parses multiple logfiles in subdirectories
    """
    logging.info('parsing files')
    log_root_dir = options['log_root_dir']
    cursor = options['db_conn'].cursor()
    routers = os.listdir(log_root_dir)

    for i, router in enumerate(sorted(routers)):
        logging.info('parsing %s (%d/%d)', router, i+1, len(routers))
        logs = os.listdir(log_root_dir+'/'+router)
        for j,log in enumerate(sorted(logs)):
            log_type = None
            if log.find('-server-') >= 0:
                log_type = 'server'
            elif log.find('-client-')  >= 0:
                log_type = 'client'

            if log_type:
                logging.info('\tparsing log %s (%d/%d)', log, j+1, len(logs))
                logfile = "%s/%s/%s" % (log_root_dir, router, log)
                try:
                    infile = open(logfile, 'r')
                except IOError, msg:
                    logging.warning('Unable to open file %s', logfile)
                    continue
                logging.debug('\t%s file %s', log_type.upper(), logfile)
                protocol = log.split('-%s-' % log_type)[0]
                parse_file(infile, cursor, protocol, log_type, options['protocol'])
            else:
               logging.debug('skipping %s', log)

def parse_file(f, cursor, protocol, log_type, l4proto):
    lines = sum([1 for line in f])
    f.seek(0, 0) # reset to first byte
    for i, line in enumerate(f):
        match = re_server_result.match(line)
        if match:
            time = match.group(1)
            time = datetime.datetime(year=int(time[0:4]), month=int(time[4:6]), day=int(time[6:8]), hour=int(time[8:10]), minute=int(time[10:12]), second=int(time[12:14]))
            server_ip = match.group(2)
            server_port = match.group(3)
            client_ip = match.group(4)
            client_port = match.group(5)
            _id = match.group(6)
            duration = float(match.group(7).split('-')[1])
            transferred = int(match.group(8))
            throughput = int(match.group(9))
            jitter = lost_packets = sent_packets = loss = _unknown = None
            if match.group(10):
                # udp match
                jitter = float(match.group(10))
                lost_packets = int(match.group(11))
                sent_packets = int(match.group(12))
                loss = float(match.group(13))
                _unknown = match.group(14)
            logging.info('\t\tclient=%s,\tthroughput=%.3f Mbps' % (client_ip, throughput/1000000.0))
            try:
                if log_type == 'server':
                    t = (time, protocol, server_ip, client_ip, duration, transferred, throughput, jitter, lost_packets, sent_packets, loss, _unknown)
                    cursor.execute('INSERT INTO server_results VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', t)
                else:
                    if l4proto == 'tcp':
                        if i == 0:
                            # switch
                            _tmp = client_ip
                            client_ip = server_ip
                            server_ip = _tmp
                            t = (time, protocol, server_ip, client_ip, duration, transferred, throughput, jitter, lost_packets, sent_packets, loss, _unknown)
                            cursor.execute('INSERT INTO client_results VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', t)
                        else:
                            logging.critical('To many lines in file')
                            assert(False)
                    elif l4proto == 'udp':
                        if i == 0:
                            # switch
                            _tmp = client_ip
                            client_ip = server_ip
                            server_ip = _tmp
                            l4proto = 'udp'
                            t = (time, protocol, server_ip, client_ip, duration, transferred, throughput, jitter, lost_packets, sent_packets, loss, _unknown)
                            cursor.execute('INSERT INTO client_stats VALUES (?,?,?,?,?,?,?)', t[0:7])
                        elif i == 1:
                            # no switch
                            t = (time, protocol, server_ip, client_ip, duration, transferred, throughput, jitter, lost_packets, sent_packets, loss, _unknown)
                            cursor.execute('INSERT INTO client_results VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', t)
                        else:
                            logging.critical('To many lines in file')
                            assert(False)
                    else:
                        logging.critical('Unknown transport layer protocol')
                        assert(False)
            except sqlite3.IntegrityError:
                logging.warn('\t\t### duplicate: server=%s, client=%s, protocol=%s ###', server_ip, client_ip, protocol)
        else:
            logging.debug('\tno match: %s', line)

def plot(options):
    def configure_axes(figs, protocols, protocol, i=0):
        for fig in figs:
            if len(protocols) > 1:
                fig.ax.set_xticks(range(0, len(protocols)))
                fig.ax.set_xlim(-1, len(protocols))
                fig.ax.set_xticklabels(protocols, rotation=90)
            else:
                fig.ax.set_xticks([i-1, i, i+1])
                fig.ax.set_xlim(i-1, i+1)
                fig.ax.set_xticklabels(['', protocol, ''])
            if protocol == 'udp':
                fig.ax.set_ylim(0, 1.05)
                fig.ax.set_yticks(pylab.linspace(0, 1, 11))
            else:
                fig.ax.set_ybound(lower=0.0)
            if hasattr(fig, 'ax_2nd'):
                fig.ax_2nd.set_ylabel('Fraction of Successful Sessions', fontsize=options['fontsize'])
                fig.ax_2nd.set_ylim(0, 1.05)
                fig.ax_2nd.set_yticks(pylab.linspace(0, 1, 11))

    cursor = options['db_conn'].cursor()
    options['prefix'] = 'all'
    color = options['color']

    protocols = [p[0] for p in cursor.execute('''
        SELECT DISTINCT(protocol)
        FROM server_results
        WHERE not protocol="des-dsr-hc"
        ORDER BY protocol
    ''').fetchall()]

    if options['protocol'] == 'udp':
        sessions = [s[0] for s in cursor.execute('''
            SELECT COUNT(protocol)
            FROM client_stats
            WHERE not protocol="des-dsr-hc"
            GROUP BY protocol
            ''').fetchall()]
        #assert(len(set(sessions)) == 1)
        sessions = max(sessions)
        ylabel = 'Normalized Throughput (UDP 1 Mbps CBR)'
    else:
        sessions = 25*24.0
        ylabel = 'TCP Throughput [Mbps]'

    fig_box_all = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', grid=True, twinx=True)
    fig_scatter_all = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', twinx=True, grid=True)
    color = options['color']

    fig_box_only = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', grid=True)
    fig_scatter_only = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', grid=True)
    fig_sessions_only = MyFig(options, xlabel='', ylabel='Fraction of Successful Sessions', aspect='auto', grid=True)

    for i, protocol in enumerate(protocols):
        fig_proto_box = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', twinx=True, grid=True)
        fig_proto_scatter = MyFig(options, xlabel='', ylabel=ylabel, aspect='auto', twinx=True, grid=True)

        if options['protocol'] == 'udp':
            # get the normalized throughput for UDP
            throughput_raw = cursor.execute('''
                SELECT CAST(r.throughput AS REAL)/l.throughput
                FROM client_stats AS l LEFT JOIN server_results AS r
                ON r.protocol = l.protocol AND l.server_ip = r.server_ip AND l.client_ip = r.client_ip
                WHERE l.protocol = ?
            ''', (protocol, )).fetchall()
            throughput_mbps = numpy.array([min(t, 1.0) for t, in throughput_raw if t])
        else:
            # just get the throughput for UDP
            throughput_raw = cursor.execute('''
                SELECT throughput
                FROM server_results
                WHERE protocol = ?
            ''', (protocol, )).fetchall()
            throughput_mbps  = numpy.array([t/10.0**6 for t, in throughput_raw])

        count = cursor.execute('''
            SELECT COUNT(throughput)
            FROM server_results
            WHERE protocol = ?
        ''', (protocol,)).fetchone()[0]

        if not len(throughput_mbps):
            logging.warn('no results for protocol %s', protocol)
            continue
        X = numpy.array([i for x in xrange(0, len(throughput_mbps))])
        bar_width = 0.21

        # draw scatter plot
        for fig in [fig_scatter_all, fig_scatter_only, fig_proto_scatter]:
            _bar_width = bar_width
            if not hasattr(fig, 'ax_2nd'):
                _bar_width = 0
            fig.ax.scatter(X-_bar_width, throughput_mbps, marker='x', color=fu_blue)

        # add fraction of sessions to second y-axis
        for fig in [fig_scatter_all, fig_box_all, fig_proto_box, fig_proto_scatter]:
            _bar_width = bar_width
            if not hasattr(fig, 'ax_2nd'):
                _bar_width = 0
            fig.ax_2nd.bar(i+_bar_width, float(count)/sessions, width=bar_width, alpha=0.5, color='white', edgecolor='blue', facecolor=fu_blue)

        fig_sessions_only.ax.bar(i-0.5*_bar_width, float(count)/sessions, width=bar_width, alpha=0.5, color='white', edgecolor='blue', facecolor=fu_blue)

        # draw box plot
        for fig in [fig_box_all, fig_box_only, fig_proto_box]:
            _bar_width = bar_width
            if not hasattr(fig, 'ax_2nd'):
                _bar_width = 0
            r = fig.ax.boxplot(throughput_mbps, positions=[i-_bar_width], notch=1, widths=bar_width, sym='rx', vert=1, patch_artist=False)
            for b in r['boxes']:
                b.set_color(fu_blue)
                b.set_fillstyle('full')
            for w in r['whiskers']:
                w.set_color(fu_blue)
            for m in r['medians']:
                m.set_color(fu_red)
            for f in r['fliers']:
                f.set_color(fu_red)

        configure_axes([fig_proto_box, fig_proto_scatter], [protocol], options['protocol'], i)
        fig_proto_box.save('throughput-%s-box' % protocol)
        fig_proto_scatter.save('throughput-%s-scatter' % protocol)

    configure_axes([fig_box_all, fig_scatter_all, fig_box_only, fig_scatter_only, fig_sessions_only], protocols, options['protocol'])
    fig_box_all.save('throughput-box')
    fig_box_only.save('throughput-box-only')
    fig_scatter_all.save('throughput-scatter')
    fig_scatter_only.save('throughput-scatter-only')
    fig_sessions_only.save('sessions-only')

def main():
    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')
    parser = argparse.ArgumentParser(description='Parse log files and store data in sqlite database')
    parser.add_argument('--db', required=True, help='name of the database where the parsed data shall be stored')
    parser.add_argument('--logs', required=True, help='root directory where the logs are stored')
    parser.add_argument('--outdir', default='', help='output directory for the plots')
    parser.add_argument('--fontsize', default=20, type=int, help='base font size of plots')
    parser.add_argument('--execute', nargs='+', default='', help='Execute only the specifiedfunction')
    parser.add_argument('--protocol', required=True, help='[tcp | udp]')
    args = parser.parse_args()

    options = {}
    options['db'] = args.db
    options['outdir'] = args.outdir
    options['log_root_dir'] = args.logs
    options['fontsize'] = args.fontsize
    options['color'] = cm.jet
    options['color2'] = cm.autumn
    options['markers'] = ['o', 'v', 'x', 'h', '<', '*', '>', 'D', '+', '|']
    if args.protocol.lower() not in ['udp', 'tcp']:
        logging.critical('invalid protocol specified: %s', args.protocol)
        sys.exit(1)
    options['protocol'] = args.protocol.lower()

    db = options["db"]
    conn = sqlite3.connect(db)
    options['db_conn'] = conn
    cursor = conn.cursor()

    if len(args.execute):
        for func in args.execute:
            try:
                globals()[func](options)
            except KeyError:
                logging.critical('function not found: %s' % func)
    else:
        logging.info('creating table')
        cursor.execute(''' \
            CREATE TABLE server_results ( \
                time         TEXT NOT NULL, \
                protocol     TEXT NOT NULL, \
                server_ip    TEXT NOT NULL, \
                client_ip    TEXT NOT NULL, \
                duration     REAL NOT NULL, \
                transferred  INTEGER NOT NULL, \
                throughput   INTEGER NOT NULL, \
                jitter       REAL, \
                lost_packets INTEGER, \
                sent_packets INTEGER, \
                loss         REAL, \
                _unknown     TEXT,
                PRIMARY KEY(protocol, server_ip, client_ip)
        )''')
        cursor.execute(''' \
            CREATE TABLE client_stats ( \
                time         TEXT NOT NULL, \
                protocol     TEXT NOT NULL, \
                server_ip    TEXT NOT NULL, \
                client_ip    TEXT NOT NULL, \
                duration     REAL NOT NULL, \
                transferred  INTEGER NOT NULL, \
                throughput   INTEGER NOT NULL, \
                PRIMARY KEY(protocol, server_ip, client_ip)
        )''')
        cursor.execute(''' \
            CREATE TABLE client_results ( \
                time         TEXT NOT NULL, \
                protocol     TEXT NOT NULL, \
                server_ip    TEXT NOT NULL, \
                client_ip    TEXT NOT NULL, \
                duration     REAL NOT NULL, \
                transferred  INTEGER NOT NULL, \
                throughput   INTEGER NOT NULL, \
                jitter       REAL, \
                lost_packets INTEGER, \
                sent_packets INTEGER, \
                loss         REAL, \
                _unknown     TEXT,
                PRIMARY KEY(protocol, server_ip, client_ip)
        )''')
        parse_files(options)
        conn.commit()
        plot(options)

if __name__ == "__main__":
  main()
