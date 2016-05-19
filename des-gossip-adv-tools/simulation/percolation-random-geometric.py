#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import matplotlib
matplotlib.use('Agg')
import logging
import argparse
import numpy
from helpers import *
import multiprocessing
from multiprocessing import Pool, Lock, Manager
from percolation import create_header, eval_graph, get_tuples, write_header, get_parser, get_layouts, get_options, run_process
from graph_generator import *
from export import graphToNED, graphToCSV, graphToNEDMixim

def main():
    parser = get_parser()
    parser.add_argument('--range', type=float, default=250.0, help='radio range for nodes in unit-square')
    parser.add_argument('--size', type=float, default=1000.0, help='physical length of the unit-square')
    parser.add_argument('--dim', type=int, default=2, help='dimension of the graph')
    parser.add_argument('--graphs', '-g', type=int, default=1, help='number of random graphs to generate and use')

    args = parser.parse_args()

    options = get_options(args)
    get_layouts(args.layout, options)
    options['type'] = 'random geometric'
    options['graphs'] = args.graphs
    options['range'] = args.range
    options['dim'] = args.dim
    options['size'] = args.size

    logging.basicConfig(level=options['log_level'])

    graphs = create_random_geometric(options)
    options['graph'] = graphs
    graphToNED(options)
    graphToCSV(options)
    graphToNEDMixim(options)
    if args.graph_only:
        sys.exit(0)
    write_header(options)

    ptuples = get_tuples(options)
    ptuples = numpy.array(ptuples).repeat(len(graphs), axis=0)
    params = zip(graphs*len(ptuples), ptuples)

    pool = Pool(processes=options['processes'])
    manager = Manager()
    lock = manager.Lock()

    data = [(G, options, i, (ps, pb), lock) for i, (G, (ps, pb)) in enumerate(params)]
    run_process(pool, data, options)

if __name__ == "__main__":
    main()
