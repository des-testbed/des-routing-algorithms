#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import matplotlib
matplotlib.use('Agg')
import logging
import argparse
import scipy
import networkx as nx
import multiprocessing
from multiprocessing import Pool, Lock, Manager
from percolation import create_header, eval_graph, get_tuples, get_parser, get_layouts, write_header, get_options, run_process
from graph_generator import *
from export import graphToNED, graphToCSV

def main():
    parser = parser = get_parser()
    parser.add_argument('--dimensions', '-d', type=int, default=2, help='dimension of the grid')
    args = parser.parse_args()

    options = get_options(args)
    get_layouts(args.layout, options)
    options['type'] = 'grid'
    options['graphs'] = 1
    options['dimensions'] = args.dimensions

    logging.basicConfig(level=options['log_level'])

    graphs = create_grid(options)
    options['graph'] = graphs
    graphToNED(options)
    graphToCSV(options)
    if args.graph_only:
        sys.exit(0)
    write_header(options)

    pool = Pool(processes=options['processes'])
    manager = Manager()
    lock = manager.Lock()
    data = [(graphs[0], options, i, ptuple, lock) for i, ptuple in enumerate(get_tuples(options))]
    run_process(pool, data, options)

if __name__ == "__main__":
    main()
