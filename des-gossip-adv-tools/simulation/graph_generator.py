
import os
import networkx as nx
import scipy
import logging
import pylab
import random
import argparse
from helpers import confidence
from multiprocessing import Pool, Lock, Manager
from helpers import MyFig

def create_random_regular(options):
    nodes = options['nodes']
    degree = options['degree']
    graphs = options['graphs']
    Gs = [nx.generators.random_graphs.random_regular_graph(degree, nodes, create_using=None, seed=None) for i in range(0, graphs)]
    return Gs

def create_grid(options):
    d = options['dimensions']
    n = int(options['nodes']**(1.0/d))
    if n**d != options['nodes']:
        logging.warning('%d * %d != %d' % (n, n, options['nodes']))
    return [nx.generators.classic.grid_graph([n for i in xrange(d)])]
    #return [nx.generators.classic.grid_2d_graph(n, n)]

def create_random(options):
    nodes = float(options['nodes'])
    degree = float(options['degree'])
    graphs = options['graphs']
    # m is the expected number of edges: m = p*n*(n-1)/2.
    # => p = m / (n*(n-1)/2)
    # => degree = m / nodes => degree * nodes = m
    # => p = (degree * nodes) / (n*(n-1)/2) # why /2 ???
    m = degree * nodes
    p = m / (nodes*(nodes-1))
    Gs = list()
    for i in range(0, options['graphs']):
        G = nx.generators.random_graphs.erdos_renyi_graph(int(nodes), p)
        j = 0
        while not nx.algorithms.components.connected.is_connected(G):
            G = nx.generators.erdos_renyi_graph(int(nodes), p)
            j += 1
            if(j > 100):
                print 'got unconnected graph in 100 tries; tune parameters: aborting'
                assert(False)
        Gs.append(G)
    return Gs

def create_random_geometric(options):
    graphs = list()
    size = options['size']
    radius = options['range']/float(options['size'])
    for i in range(0, options['graphs']):
        G = nx.generators.geometric.random_geometric_graph(options['nodes'], dim=options['dim'], radius=radius)
        j = 0
        while not nx.algorithms.components.connected.is_connected(G):
            G = nx.generators.geometric.random_geometric_graph(options['nodes'], dim=options['dim'], radius=radius)
            j += 1
            if(j > 100):
                assert(False)
        graphs.append(G)
    return graphs

def create_graph_db(options):
    logging.info('creating graph')
    mode = options['mode']
    cursor = options['db_conn'].cursor()
    hosts = list(pylab.flatten(cursor.execute('SELECT DISTINCT(host) FROM addr ORDER BY host').fetchall()))

    if mode in ['du', 'dw']:
        protoGraph = nx.DiGraph()
    else:
        protoGraph = nx.Graph()
    protoGraph.add_nodes_from(hosts)
    tag_ids = cursor.execute('SELECT key, helloSize FROM tag where helloSize = ? ORDER BY helloSize', (options['size'],)).fetchall()
    if len(tag_ids) <= 0:
        sizes = cursor.execute('SELECT DISTINCT(helloSize) FROM tag ORDER BY helloSize').fetchall()
        raise Exception('Packet size not found in db; available: [%s]' % ', '.join([str(s[0]) for s in sizes]))
    graphs = list()
    for key, helloSize in tag_ids:
        G = protoGraph.copy()
        cursor.execute('''SELECT src,host,pdr FROM eval_helloPDR WHERE tag_key=?''', (key,))
        for src, host, pdr in cursor.fetchall():
            G.add_edge(host, src, weight=pdr)
        graphs.append(G)
    if len(graphs) == 0:
        logging.critical('no graph created')
    return graphs

def scale_graph(graph, factor, mode='add'):
    assert(nx.algorithms.components.connected.is_connected(graph))
    assert(factor >= 2)
    G = nx.Graph()

    # create copies of graph as layers
    for n in graph.nodes():
        G.add_node('1-' + str(n))
        for i in range(2, factor+1):
            G.add_node('%d-%s' % (i, str(n)))
    for (n1, n2) in graph.edges():
        G.add_edge('1-' + str(n1), '1-' + str(n2))
        for i in range(2, factor+1):
            G.add_edge('%d-%s' % (i, str(n1)), '%d-%s' % (i, str(n2)))

    # connect layers
    for n in graph.nodes():
        n = str(n)
        for i in range(2, factor+1):
            skip = 0
            lower = '%d-%s' % (i-1, n)
            upper = '%d-%s' % (i, n)
            if mode == 'swap' or mode == 'swap-add' or mode == 'swap-random':
                neighbors = [neigh for neigh in G.neighbors(lower) if neigh.startswith('%d-' % (i-1))]
                if len(neighbors):
                    m = random.choice(neighbors)
                    if mode == 'swap-add':
                        if random.random() < 0.33:
                            G.remove_edge(lower, m)
                    else:
                        G.remove_edge(lower, m)
                else:
                    skip += 1

                neighbors = [neigh for neigh in G.neighbors(upper) if neigh.startswith('%d-' % (i))]
                if len(neighbors):
                    m = random.choice(neighbors)
                    if mode == 'swap-add':
                        if random.random() < 0.33:
                            G.remove_edge(upper, m)
                    else:
                        G.remove_edge(upper, m)
                else:
                    skip += 1
            elif mode == 'add-random':
                pass
            elif mode == 'add':
                pass
            else:
                assert(False)

            if mode == 'swap-add':
                if random.random() < 0.5:
                    G.add_edge(lower, upper)
            elif mode == 'add-random' or mode == 'swap-random':
                r = random.choice(G.neighbors(upper))
                G.add_edge(lower, r)
            else:
                G.add_edge(lower, upper)

            if skip:
                print '%d: %d skips' % (i, skip)
    return G

def smp_scale_eval((G, options, factors, lock)):
    scaled_graphs = [scale_graph(G, i, options['scale_mode']) for i in factors]
    scaled_graphs.insert(0, G)
    dia = [nx.algorithms.distance_measures.diameter(g) for g in scaled_graphs]
    deg = [scipy.mean(g.degree().values()) for g in scaled_graphs]
    lock.acquire()
    out_file_diameter = open(options['outdir']+'/scaled_graph_diameter.data', 'a')
    out_file_degree = open(options['outdir']+'/scaled_graph_degree.data', 'a')
    for j, x in enumerate(dia):
        y = deg[j]
        out_file_diameter.write('%d; %f\n'% (j+1, x))
        out_file_degree.write('%d; %f\n'% (j+1, y))
    lock.release()

def test_scaler():
    def parse(in_file):
        rlist = [0]
        for line in in_file:
            if line.startswith('#') or line.isspace():
                continue
            tokens = line.split(';')
            try:
                rlist[int(tokens[0].strip())].append(float(tokens[1].strip()))
            except IndexError:
                rlist.append([float(tokens[1].strip())])
        return rlist[1:]

    parser = argparse.ArgumentParser(description='Run graph scaler example')
    parser.add_argument('--outdir', default='./', help='output directory for the results')
    parser.add_argument('--nodes', '-n', type=int, default=100, help='number of nodes')
    parser.add_argument('--processes', '-p', type=int, default=1, help='number of processes')
    parser.add_argument('--fontsize', '-f', type=int, default=24, help='fontsize')
    parser.add_argument('--degree', '-d', type=int, default=12, help='target avr. node degree')
    parser.add_argument('--graphs', '-g', type=int, default=20, help='number of graphs')
    parser.add_argument('--max_scale', '-s', type=int, default=20, help='maximum scale factor')
    parser.add_argument('--mode', '-m', default='swap-add', help='mode to connected layers')
    args = parser.parse_args()

    options = dict()
    options['nodes'] = args.nodes
    options['degree'] = args.degree
    options['graphs'] = args.graphs
    options['scale_mode'] = args.mode
    options['processes'] = args.processes
    options['fontsize'] = args.fontsize
    options['outdir'] = args.outdir
    options['max_scale'] = args.max_scale

    try:
        os.remove(options['outdir']+'/scaled_graph_diameter.data')
    except OSError:
        pass
    out_file_diameter = open(options['outdir']+'/scaled_graph_diameter.data', 'w')
    out_file_diameter.write('# scale_factor; diameter\n')
    out_file_diameter.close()
    try:
        os.remove(options['outdir']+'/scaled_graph_degree.data')
    except OSError:
        pass
    out_file_degree = open(options['outdir']+'/scaled_graph_degree.data', 'w')
    out_file_degree.write('# scale_factor; avr. degree\n')
    out_file_degree.close()

    factors = range(2, options['max_scale']+1)
    graphs = create_random(options)
    pool = Pool(processes=options['processes'])
    manager = Manager()
    lock = manager.Lock()
    data = [(G, options, factors, lock) for i, G in enumerate(graphs)]
    pool.map(smp_scale_eval, data)

    in_file_diameter = open(options['outdir']+'/scaled_graph_diameter.data', 'r')
    diameters = parse(in_file_diameter, diameters)

    in_file_degree = open(options['outdir']+'/scaled_graph_degree.data', 'r')
    degrees = parse(in_file_degree, degrees)

    options['prefix'] = 'scale'
    fig = MyFig(options, figsize=(10,8), xlabel = 'Scale factor', ylabel='Metric', legend=True, grid=True, aspect='auto')
    factors.insert(0, 1)

    diameters_means = [scipy.mean(d) for d in diameters]
    diameters_yerr = [confidence(d)[2] for d in diameters]
    fig.ax.plot(factors, diameters_means, color='red', label='Diameter')
    fig.ax.errorbar(factors, diameters_means, yerr=diameters_yerr, color='red')

    degrees_means = [scipy.mean(d) for d in degrees]
    degrees_yerr = [confidence(d)[2] for d in degrees]
    fig.ax.plot(factors, degrees_means, color='blue', label='Mean degree')
    fig.ax.errorbar(factors, degrees_means, yerr=degrees_yerr, color='blue')

    fig.ax.set_ylim(0, max(diameters_means)+1)
    fig.save('test')

if __name__ == '__main__':
    test_scaler()
