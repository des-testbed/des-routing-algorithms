#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib
matplotlib.use('Agg')
import logging
import argparse
import numpy
from helpers import *
import scipy
import networkx as nx
from multiprocessing import Pool, Lock, Manager
from export import graphToNED

def create_header(options):
    logging.info('creating header')
    graph = options['graph']

    header = str()
    if options['process'] == 'percolation':
        header += '# Combined bond-site percolation process: [DESCRIPTION]\n\n'
    else:
        header += '# Gossip routing process: [DESCRIPTION]\n\n'
        header += '# sources        : %d\n' % options['sources']
    #header += '# seed           : %d\n' % options['seed']
    header += '# mode           : %s\n' % options['type']
    header += '# graphs         : %d\n' % options['graphs']

    if options['type'] == 'DES-Testbed':
        header += '# db             : %s\n' % options['db']
        header += '# helloSize      : %d\n' % options['size']
        header += '# mode           : %s\n' % options['mode']
    elif options['type'] ==  'grid':
        pass
    elif options['type'] == 'random geometric':
        header += '# range          : %f\n' % options['range']
        header += '# size           : %f\n' % options['size']
    elif options['type'] == 'random regular':
        pass
    elif options['type'] == 'random':
        pass
    else:
        logging.critical("unknown type")
        assert(False)

    header += '# nodes          : %s\n' % options['nodes']

    def diameter(Gs):
        diameters = list()
        info = ''
        for G in Gs:
            if G.is_directed():
                components = nx.algorithms.strongly_connected_components(G)
            else:
                components = nx.algorithms.connected_components(G)
            c = components[0]
            if len(components) > 1:
                info = ' (partitioned!)'
            d = nx.algorithms.distance_measures.diameter(nx.subgraph(G, c))
            diameters.append(d)
        return scipy.mean(d), info

    if options['type'] == 'grid':
        header += '# dimensions     : %d\n' % options['dimensions']
        header += '# diameter       : %d\n' % (options['dimensions']*(int((options['nodes'])**(1.0/options['dimensions']))-1))
    else:
        if type(graph) == list:
            d, info = diameter(graph)
        else:
            d, info = diameter([graph])
        header += '# mean diameter  : %d%s\n' % (d, info)

    if 'degree' in options and options['degree'] != None:
        header += '# degree         : %f\n' % options['degree']
    header += '# mean degree    : %f\n' % scipy.mean([scipy.mean(g.degree().values()) for g in graph])

    header += '# replications   : %d\n' % options['replications']
    header += '# steps          : %f\n' % options['steps']

    if len(options['suppression']) > 1:
        header += '# suppression    : %s, params=[%s]\n' % (options['suppression'][0], ','.join([str(s) for s in options['suppression'][1:]]))
    else:
        header += '# suppression    : %s\n\n' % options['suppression'][0]

    header += '\n# column 0: number of run (ignore or use as key)\n'
    if options['process'] == 'percolation':
        header += '# column 1: Site occupation probability\n'
        header += '# column 2: Bond occupation probability\n'
        header += '# column 3: Mean cluster size calculated over all replications\n'
        header += '# column 4: Calculated standard deviation of the cluster sizes\n'
        header += '# column 5: Calculated 95% confidence interval (0.0 if missing)\n'
        header += '# column 6: Number of data points/replications\n\n'
    else:
        header += '# column 1: Forwarding probability\n'
        header += '# column 2: Packet delivery ratio\n'
        header += '# column 3: Mean reachability calculated over all replications and sources\n'
        header += '# column 4: Calculated standard deviation of the reachability\n'
        header += '# column 5: Calculated 95% confidence interval (0.0 if missing)\n'
        header += '# column 6: Number of data points/replications*sources\n\n'
    return header

def get_parser():
    parser = argparse.ArgumentParser(description='Run percolation or gossip routing process on a graph')
    parser.add_argument('--graph_only', nargs='?', const=True, default=False, help='Create graphs but do not run simulation')
    parser.add_argument('--process', default='percolation', help='[percolation | gossip] (default = percolation)')
    parser.add_argument('--outdir', default='./', help='output directory for the results')
    parser.add_argument('--replications', '-r', type=int, default=30, help='number of replications')
    parser.add_argument('--sources', type=int, default=30, help='source for gossip routing process')
    parser.add_argument('--nodes', '-n', type=int, default=100, help='number of nodes')
    parser.add_argument('--steps', '-s', type=float, default=0.01, help='steps for the probabilities')
    parser.add_argument('--seed', type=int, default=scipy.random.randint(0, 2**32), help='seed for the RNG')
    parser.add_argument('--processes', '-p', type=int, default=2, help='number of processes')
    parser.add_argument('--suppression', nargs='+', default=['none'], help='suppression mode (default=disabled)')
    parser.add_argument('--layout', '-l',  nargs='+', default=['none'], help='Layout for the graph drawing: spring, circular, spectral, shell, graphviz, all')
    return parser

def get_layouts(args_layout, options):
    layouts = { 'spring': nx.draw_spring, 'circular': nx.draw_circular, 'spectral': nx.draw_spectral, 'shell': nx.draw_shell, 'graphviz': nx.draw_graphviz }

    selected_layouts = list()
    if not 'none' in args_layout:
        if 'all' in args_layout:
            selected_layouts = layouts.items()
        else:
            for l in args_layout:
                selected_layouts.append((l, layouts[l]))
    options['layouts'] = selected_layouts

def get_options(args):
    options = dict()
    options['log_level'] = logging.INFO
    options['outdir'] = args.outdir
    options['replications'] = args.replications
    options['steps'] = args.steps
    options['nodes'] = args.nodes
    options['processes'] = args.processes
    options['suppression'] = args.suppression
    options['seed'] = args.seed
    options['process'] = args.process
    options['sources'] = args.sources
    scipy.random.seed(args.seed)
    return options

def write_header(options):
    logging.info('writing header')
    header = create_header(options)

    if options['process'] == 'percolation':
        outfile_max = open('./%s/results-max' % (options['outdir'],), 'w')
        s1 = 'maximum cluster size'
        header1 = header.replace('[DESCRIPTION]', s1)
        outfile_max.write(header1)
        outfile_max.close()

    outfile_mean = open('./%s/results-mean' % (options['outdir'],), 'w')
    if options['process'] == 'percolation':
        s2 = 'mean cluster size'
    else:
        s2 = 'mean reachability'
    header2 = header.replace('[DESCRIPTION]', s2)
    outfile_mean.write(header2)
    outfile_mean.close()

def get_tuples(options):
    if 'mode' in options and options['mode'] in ['uu', 'du', 'dw']:
        logging.info('mode = %s' % options['mode'])
        c = [(ps, 1.0) for ps in numpy.arange(options['steps'], 1.0+options['steps'], options['steps'])]
    else:
        pss = numpy.arange(options['steps'], 1.0+options['steps'], options['steps'])
        a = pss.repeat(len(pss))
        b = list(pss)*len(pss)
        c = zip(a,b)
    return c

def eval_graph(G, origG, pb):
    components = nx.connected_components(G)
    if len(components):
        sizes = list()
        for comp in components:
            neighbors = list()
            for node in comp:
                neighbors += origG.neighbors(node)
            perimeter = set(neighbors).difference(comp)
            size = len(comp)+int(len(perimeter)*pb)
            assert(size <= len(origG) and size >= len(comp))
            sizes.append(size)
        return float(max(sizes))/len(origG.nodes()), scipy.mean(sizes)/len(origG.nodes())
        #return float(len(components[0]))/len(origG.nodes()), scipy.mean([len(c) for c in components])/len(origG.nodes())
    else:
        return 0, 0

def remove_sites(G, ps):
    sites = [site for site in G.nodes() if scipy.rand() > ps]
    G.remove_nodes_from(sites)

def remove_bonds(G, pb, origG, options):
    for bond in G.edges():
        sfactor = 1.0
        #sfactor = suppression_factor(bond, G, origG, options)
        #assert(sfactor  >= 0 and sfactor  <= 1.0)
        if scipy.rand() > pb * sfactor:
            try:
                G.remove_edge(bond[0], bond[1])
            except nx.exception.NetworkXError:
                print 'nodes: ', G.nodes()
                print 'edges: ', G.edges()
                raise

def suppression_factor(bond, G, origG, options):
    def open_sites(neighbors):
        return [n for n in neighbors if n in G.nodes()]

    def vicinity_cluster(bond):
        return set(origG.neighbors(bond[0])+origG.neighbors(bond[1]))

    if options['suppression'][0] == 'linear':
        neighbors = vicinity_cluster(bond)
        opensites = open_sites(neighbors)
        max_neighbors = float(len(neighbors))
        if len(options['suppression']) > 1:
            max_neighbors = float(options['suppression'][1])
        suppression_factor = 1.0 - min(len(opensites)/max_neighbors, 1)
    elif options['suppression'][0] == 'triangular':
        best = options['suppression'][1]
        neighbors = vicinity_cluster(bond)
        opensites = open_sites(neighbors)
        max_neighbors = float(len(neighbors))
        if len(options['suppression']) > 2:
            max_neighbors = float(options['suppression'][2])
        if opensites < best:
            suppression_factor = 1.0/best * len(opensites)
        elif opensites > best:
            suppression_factor = 1.0 - min(1.0/(max_neighbors - best) * (len(opensites) - best), 1)
        else:
            suppression_factor = 1.0
        print len(opensites), len(neighbors), suppression_factor
    elif options['suppression'][0] == 'none':
        suppression_factor = 1.0
    else:
        raise Exception('unsupported suppression mode')
    return suppression_factor

def run_process(pool, data, options):
    if options['process'] == 'gossip':
        logging.info('running gossip process')
        pool.map(gossip, data)
    elif options['process'] == 'percolation':
        logging.info('running percolation process')
        pool.map(percolation, data)
    else:
        logging.critical('invalid process: %s', options['process'])

def percolation((origG, options, cur_run, (ps, pb), lock)):
    max_cluster_size = list()
    mean_cluster_size = list()
    for n in xrange(0, options['replications']):
        G = origG.copy()
        remove_sites(G, ps)
        remove_bonds(G, pb, origG, options)
        max_cluster, mean_cluster = eval_graph(G, origG, pb)
        max_cluster_size.append(max_cluster)
        mean_cluster_size.append(mean_cluster)
    lock.acquire()
    outfile_max = open('./%s/results-max' % (options['outdir'],), 'a')
    outfile_mean = open('./%s/results-mean' % (options['outdir'],), 'a')
    outfile_max.write('RUN%d: %f, %f, %f, %f, %f, %d\n' % (cur_run, ps, pb, scipy.mean(max_cluster_size), scipy.std(max_cluster_size), confidence(max_cluster_size)[2], options['replications']))
    outfile_mean.write('RUN%d: %f, %f, %f, %f, %f, %d\n' % (cur_run, ps, pb, scipy.mean(mean_cluster_size), scipy.std(mean_cluster_size), confidence(mean_cluster_size)[2], options['replications']))
    outfile_max.close()
    outfile_mean.close()
    lock.release()

def gossip((G, options, cur_run, (ps, pb), lock)):
    mean_reachability = list()
    mean_fw = list()
    nodes = list(G.nodes())
    sources = random.sample(nodes, options['sources'])
    for src in sources:
        for r in xrange(0, options['replications']):
            have_packet = set()
            have_packet.add(src)
            reached = set()
            fw = 0
            reached.add(src)
            potential_rx = list()

            while len(have_packet) or len(potential_rx):
                # receive
                for (tx, rx) in potential_rx:
                    if 'mode' in options and options['mode'] != 'pb':
                        if options['mode'] == 'uu' or options['mode'] == 'du':
                            have_packet.add(rx)
                        elif options['mode'] == 'dw':
                            if scipy.rand() <= G.edge[tx][rx]['weight']:
                                have_packet.add(rx)
                    else: # mode == 'pb'
                        if scipy.rand() <= pb:
                            have_packet.add(rx)

                # transmit
                potential_rx = list()
                for tx in have_packet:
                    if tx == src or scipy.rand() <= ps:
                        potential_rx.extend([(tx, rx) for rx in G.neighbors(tx) if rx not in have_packet and rx not in reached])
                        fw += 1
                    reached.add(tx)
                have_packet = set()
            reachability = float(len(reached)-1) / (len(nodes)-1)
            mean_reachability.append(reachability)
            mean_fw.append(fw)
    lock.acquire()
    outfile_mean = open('./%s/results-mean' % (options['outdir'],), 'a')
    outfile_mean.write('RUN%d: %f, %f, %f, %f, %f, %d, %f, %f, %f\n' % (cur_run, ps, pb, scipy.mean(mean_reachability), scipy.std(mean_reachability), confidence(mean_reachability)[2], options['replications']*options['sources'], scipy.mean(mean_fw), scipy.std(mean_fw), confidence(mean_fw)[2]))
    outfile_mean.close()
    lock.release()

if __name__ == "__main__":
    main()
