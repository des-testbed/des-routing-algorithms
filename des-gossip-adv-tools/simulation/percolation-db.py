#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import matplotlib
matplotlib.use('Agg')
import sqlite3
import logging
import argparse
import numpy
from helpers import *
import scipy
import networkx as nx
from multiprocessing import Pool, Lock, Manager

from percolation import create_header, eval_graph, remove_bonds, remove_sites, get_tuples, write_header, get_parser, get_layouts, get_options, run_process
from graph_generator import *
from export import graphToNED, graphToCSV

def main():
    parser = get_parser()
    parser.add_argument('--db', help='topology database')
    parser.add_argument('--size', '-b', type=int, default=100, help='packet size; determines the graph in the db')
    parser.add_argument('--mode', default='pb', help='[pb, uu, du, dw] (default=pb)')

    args = parser.parse_args()

    options = get_options(args)
    get_layouts(args.layout, options)
    options['type'] = 'DES-Testbed'
    options['db'] = args.db
    options['size'] = args.size
    options['mode'] = args.mode

    logging.basicConfig(level=options['log_level'])

    logging.info('connecting to database')
    db = options["db"]
    if not os.path.exists(db):
        logging.critical('Database file %s does not exist' % (db))
        sys.exit(2)
    options['db_conn'] = sqlite3.connect(db)
    graphs = create_graph_db(options)
    options['graph'] = graphs
    options['graphs'] = 1
    graphToNED(options)
    graphToCSV(options)
    if args.graph_only:
        sys.exit(0)
    write_header(options)

    pool = Pool(processes=options['processes'])
    manager = Manager()
    lock = manager.Lock()
    data = [(graphs[0], options, i, ptuple, lock) for i, ptuple in enumerate(get_tuples(options))]
    logging.info('Starting percolation simulation')
    run_process(pool, data, options)

if __name__ == "__main__":
    main()
