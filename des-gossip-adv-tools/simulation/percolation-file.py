#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import matplotlib
matplotlib.use('Agg')
import logging
import argparse
import scipy, numpy
import networkx as nx
import multiprocessing
from multiprocessing import Pool, Lock, Manager
from percolation import create_header, eval_graph, get_tuples, get_parser, get_layouts, write_header, get_options, run_process
from export import graphToNED, graphToCSV

def parse_files(options):
    graphs = list()
    for f in options['files']:
        infile = open(f, 'r')
        G = nx.Graph()
        for line in infile:
            if line.isspace():
                continue

            # no regex :-(
            if line.startswith('#'):
                try:
                    p, v = line[1:].split(':')
                    p = p.strip()
                    v = v.strip()
                except ValueError:
                    continue
                if p == 'mode':
                    if options['type'] != 'files' and options['type'] != v:
                        logging.critical('files with different graph types [%s, %s]: aborting', options['type'], v)
                        sys.exit(1)
                    options['type'] = v
                elif p == 'nodes':
                    if options['nodes'] and options['nodes'] != int(v):
                        logging.critical('files with different number of nodes: aborting')
                        sys.exit(1)
                    options['nodes'] = int(v)
                elif p == 'degree':
                    if options['degree'] and options['degree'] != float(v):
                        logging.critical('files with different degrees: aborting')
                        sys.exit(1)
                    options['degree'] = float(v)
                elif p == 'db':
                    options['db'] = v
                elif p == 'helloSize':
                    if options['size'] and options['size'] != int(v):
                        logging.critical('files with  different helloSizes: aborting')
                        sys.exit(1)
                    options['size'] = int(v)
            else:
                ntuple = line.split(';')
                assert(len(ntuple) == 2)
                t = sorted([ntuple[0].strip(), ntuple[1].strip()])
                G.add_edge(t[0], t[1])
        graphs.append(G)
    return graphs

def main():
    parser = parser = get_parser()
    parser.add_argument('--files', '-f', nargs='+', default=[], help='provide graphs as file')
    args = parser.parse_args()

    options = get_options(args)
    get_layouts(args.layout, options)
    options['type'] = 'files'
    options['nodes'] = None
    options['degree'] = None
    options['db'] = None
    options['size'] = None
    options['files'] = args.files
    logging.basicConfig(level=options['log_level'])

    if len(options['files']) == 0:
        logging.critical('no files specified: aborting')
        sys.exit(1)

    graphs = parse_files(options)
    options['graph'] = graphs
    options['graphs'] = len(graphs)
    graphToNED(options)
    graphToCSV(options)
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
