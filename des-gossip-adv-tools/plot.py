#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
import traceback
import matplotlib
matplotlib.use('Agg')
import matplotlib.cm as cm
import pylab
import sys
from helpers import update_data, write_scenarios, eval_scenarios, open_latex_file, prepare_coordinates, MyFig, prepare_outdir, all_routers, get_options
from plot_fractions import plot_single
from plot_parallel_sources import *
from plot_nhdp_mpr import *
from plot_hello import *
import scipy
import numpy

def plot_statistics(options):
    if not options['statistics']:
        return
    fig = MyFig(options, legend=True, grid=True, xlabel=r'Building', ylabel='Distance [m]', aspect='auto', legend_pos='best')
    statistics = options['statistics']
    colors = options['color'](pylab.linspace(0, 1.0, len(statistics['sad'])+1))

    keys = ['a3', 'a6', 't9', 'all']
    bar_width = 0.2
    for i, key in enumerate(keys):
        sad = statistics['sad'][key]
        aad = statistics['aad'][key]
        mad = statistics['mad'][key]
        fig.ax.bar(i-bar_width*1.5, sad, width=bar_width, color=colors[0])

        fig.ax.bar(i-bar_width*0.5, aad[0], width=bar_width, color=colors[1])
        fig.ax.errorbar(i, aad[0], yerr=aad[1], ecolor='red')

        fig.ax.bar(i+bar_width*0.5, mad[0], width=bar_width, color=colors[2])
        fig.ax.errorbar(i+bar_width, mad[0], yerr=mad[1], ecolor='red')

    fig.ax.plot(0, -1, color=colors[0], label='Statistical Average Distance (SAD)')
    fig.ax.plot(0, -1, color=colors[1], label='Actual Average Distance (AAD)')
    fig.ax.plot(0, -1, color=colors[2], label='MST Average Distance (MAD)')
    fig.ax.plot(0, -1, color='red', label='Standard deviation $\sigma$')
    fig.ax.set_ylim(0, 120)
    fig.ax.set_xticks(range(0, len(statistics['sad'])))
    fig.ax.set_xticklabels(keys)
    options['prefix'] = 'all'
    fig.save('statistics')

def main():
    """
    The main function of the plotting module
    """
    parser = argparse.ArgumentParser(description='Plot data stored in an sqlite database')
    parser.add_argument('--db', default='./sqlite.db', help='name of the database where the parsed and evaluated data is stored')
    parser.add_argument('--src', nargs='+', default=['all'], help='source node sending packets')
    parser.add_argument('--bins', default=10, type=int, help='granularity of the 3d box plot; bin size')
    parser.add_argument('--fix', nargs='+', default=[0], type=int, help='fix the number of parallel sources when routers failed [steps]')
    parser.add_argument('--outdir', default='', help='output directory for the plots')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--username', help='username for the database access to get the nodes\' positions')
    parser.add_argument('--password', help='password for the database access to get the nodes\' positions')
    parser.add_argument('--dbhost', default='uhu.imp.fu-berlin.de', help='hostname for the database access to get the nodes\' positions')
    parser.add_argument('--execute', '-e', nargs='+', default=['all'], help='Execute the specified functions')
    parser.add_argument('--list', nargs='?', const=True, default=False, help='List all callable plotting functions')
    parser.add_argument('--update', nargs='?', const=True, default=False, help='Download or update data from the testbed database')
    parser.add_argument('--statistics', nargs='?', const=True, default=False, help='Calculate and plot distance statistics')
    parser.add_argument('--deadline', default=40, type=float, help='Maximum time for the retransmit plot')
    parser.add_argument('--restarts', default=numpy.infty, type=float, help='Maximum number of retransmsmissions to plot')
    parser.add_argument('--repetitions', default=13, type=int, help='for MPR phi plotting')
    parser.add_argument('--intervals', nargs='+', default=[], type=float, help='Selected intervals for the evaluation')
    parser.add_argument('--special', nargs='+', default=[''], help='Special options for the plots')
    parser.add_argument('--mark', nargs='+', default=[], type=int, help='Mark p_s in plots for sources')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    if args.list:
        print('Callable plot functions:')
        for func in sorted([key for key in globals().keys() if key.startswith('plot_')]):
            print('\t'+func)
        sys.exit(0)

    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')

    options = get_options()
    options['db'] = args.db
    options['src'] = args.src
    options['bins'] = args.bins
    options['outdir'] = args.outdir
    options['fontsize'] = args.fontsize
    options['username'] = args.username
    options['dbpassword'] = args.password
    options['fix'] = args.fix
    options['dbhost'] = args.dbhost
    options['show3d'] = False
    options['statistics'] = args.statistics
    options['restarts'] = args.restarts
    options['deadline'] = args.deadline
    options['intervals'] = args.intervals
    options['special'] = args.special
    options['mark'] = args.mark
    options['repetitions'] = args.repetitions
    options['grayscale'] = args.grayscale

    db = options["db"]
    conn = sqlite3.connect(db)
    options['db_conn'] = conn
    cursor = conn.cursor()

    options['neg_rssi'] = False
    logging.info('old or new rssi format?')
    #max_rssi = max(cursor.execute('''SELECT rssi FROM rx WHERE NOT rssi=0 LIMIT 1''').fetchone()[0], cursor.execute('''SELECT rssi FROM rx WHERE NOT rssi=0 LIMIT 1''').fetchone()[0])
    #if max_rssi>0:
        #options['neg_rssi'] = False

    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')
    logging.info('connecting to database')
    if args.update:
        logging.info('updating positions from %s', options['dbhost'])
        update_data(options)
        sys.exit(0)

    if 'all' in args.src:
        logging.info('all sources selected for plotting')
        try:
            cursor.execute('''
                CREATE TABLE eval_sources (
                    host TEXT NOT NULL,
                    FOREIGN KEY(host) REFERENCES addr(host)
                )
            ''')
            logging.info('extracting all source node names')
            cursor.execute('''
                INSERT INTO eval_sources
                    SELECT DISTINCT(host)
                    FROM tx
                    ORDER BY host
            ''')
            conn.commit()
        except sqlite3.OperationalError:
            pass
        sources = cursor.execute('''
                SELECT DISTINCT(host)
                FROM eval_sources
                ORDER BY host
            ''').fetchall()
        options['src'] = list(pylab.flatten(sources))

    prepare_outdir(options)
    open_latex_file(options)
    eval_scenarios(options)
    write_scenarios(options)
    #try:
    prepare_coordinates(options)
    #except ValueError:
        #logging.warning('no router positions found in DB')

    if 'all' in args.execute:
        for func in [globals()[key] for key in globals().keys() if key.startswith('plot_')]:
            func(options)
    else:
        for func in args.execute:
            try:
                globals()[func](options)
            except KeyError:
                logging.critical('function not found: %s' % func)
                raise
    cursor.close()

if __name__ == "__main__":
    main()
