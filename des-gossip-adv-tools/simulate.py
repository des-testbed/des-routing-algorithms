#!/usr/bin/env python

import matplotlib
matplotlib.use('Agg')
from matplotlib import rc
rc('text', usetex=True)
import argparse
import sys
import logging
import networkx as nx
import sqlite3
import pylab
import random
from helpers import tags_with_hellos
from parse import create_tables, create_indexes
import scipy

# temporarily set to static values
p2 = 1.0
n = 0
m = 0
timeout = 200
T_MAX = 200
p_min = 0.5
p_max = 0.9

def main():
    """
    The main function of the plotting module
    """
    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')
    try:
        import psyco
        psyco.full()
        psyco.profile(0.0)
    except ImportError:
        print "INFO: psyco module not available"

    parser = argparse.ArgumentParser(description='Plot data stored in an sqlite database')
    parser.add_argument('--topo', required=True, help='name of the database where the network topology is available')
    parser.add_argument('--db', required=True, help='name of the database where the results shall be stored')
    parser.add_argument('--clone', default=None, help='clone the scenarios from another experiment')
    parser.add_argument('--src', nargs='+', default=['all'], help='source node sending packets')
    parser.add_argument('--gossip', nargs='+', default=['0'], help='gossip variants')
    parser.add_argument('-m', nargs='+', default=['1'], help='m')
    parser.add_argument('-k', nargs='+', default=['1'], help='flood on first k hops')
    parser.add_argument('--probability', '-p', nargs='+', default=list(pylab.linspace(0.1, 1, 10)), help='gossip variants')
    parser.add_argument('--outdir', default='', help='output directory for the plots')
    parser.add_argument('--pkg_num', default=100, type=int, help='number of packets')
    parser.add_argument('--pkg_size', nargs='+', default=[0], type=int, help='select a particular graph')
    parser.add_argument('--no_loss', nargs='?', const=True, default=False, help='List all callable plotting functions')
    args = parser.parse_args()

    options = {}
    options['topo'] = args.topo
    options['outdir'] = args.outdir
    options['loss'] = not args.no_loss
    options['db'] = args.db
    options['nodes'] = 'all'

    gossip_params = dict()
    options['gossip_params'] = gossip_params
    gossip_params['src'] = args.src
    gossip_params['gossip'] = args.gossip
    gossip_params['m'] = args.m
    gossip_params['k'] = args.k
    gossip_params['p'] = args.probability
    gossip_params['pkg_num'] = args.pkg_num
    gossip_params['pkg_size'] = args.pkg_size

    db = options['topo']
    conn = sqlite3.connect(db)
    options['topo_conn'] = conn
    cursor = conn.cursor()

    if 'all' in args.src and not args.clone:
        logging.info('all routers selected as sources')
        cursor.execute('''SELECT host FROM addr''')
        hosts = list(pylab.flatten(cursor.fetchall()))
        options['src'] = hosts

    if args.clone:
        clone_conn = sqlite3.connect(args.clone)
        clone_cursor = clone_conn.cursor()
        clone(options, clone_cursor)

    cursor = prepare_db(options)
    graphs = create_graphs(options)

    #for helloSize, G in graphs:
        #for gossip in gossip_params['gossip']:
            #for p in gossip_params['p']:
                #t = (None, '', '', p, p2, k, n, m, timeout, gossip, 0, 0, 0, 0, T_MAX, p_min, p_max, helloSize)
                #cursor.execute('INSERT INTO tag VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)', t)
                #curr_tag_key = c.lastrowid

    for node in graphs[0][1]:
        cursor.execute('INSERT INTO addr VALUES (?)', (node, ))
    options['db_conn'].commit()

    for i, pkg_size in enumerate(gossip_params['pkg_size']):
        G = None
        for helloSize, graph in graphs:
            if helloSize == pkg_size:
                G = (helloSize, graph)
                break
        if not G:
            logging.critical('no graph found for pkg_size=%d', pkg_size)
            assert(False)
        logging.info('helloSize=\"%d\" (%d/%d)', G[0], i+1, len(graphs))
        simulate(G, options)

def clone(options, cursor):
    parameters = cursor.execute('''
        PRAGMA TABLE_INFO(view_configurations)
    ''').fetchall()
    parameters = [name for _num, name, _type, _a, _b, _c in parameters]
    values = {}
    for param in parameters:
        values[param] = set()

    configurations = cursor.execute('''
        SELECT DISTINCT *
        FROM view_configurations
    ''').fetchall()

    for config in configurations:
        for i, value in enumerate(config):
            values[parameters[i]].add(value)

    for key in sorted(values.keys()):
        options['gossip_params'][key] = sorted(values[key])

    src = [s for s, in cursor.execute('''
        SELECT DISTINCT(host)
        FROM tx
    ''').fetchall()]
    options['gossip_params']['src'] = src

    options['nodes'] = [n for n, in cursor.execute('SELECT DISTINCT(host) FROM addr').fetchall()]

    num_packets = [n for n, in cursor.execute('''
        SELECT COUNT(host)
        FROM tx
        GROUP BY tag
    ''').fetchall()]
    assert(len(set(num_packets)) == 1)
    options['gossip_params']['pkg_num'] = num_packets[0]

def prepare_db(options):
    db = options['db']
    conn = sqlite3.connect(db)
    options['db_conn'] = conn
    cursor = conn.cursor()
    conn = options['db_conn']
    cursor = conn.cursor()

    create_tables(conn)
    create_indexes(conn)

    cursor.execute('''
        CREATE VIEW view_configurations
        AS SELECT
        p, p2, k, n, m, timeout, gossip, helloTTL, hello_interval, helloSize, cleanup_interval, T_MAX, p_min, p_max, pkg_size
        FROM tag
    ''')
    cursor.execute('''
        CREATE TABLE eval_rx_fracs_for_tag ( \
            src TEXT NOT NULL, \
            tag_key INTEGER NOT NULL, \
            frac REAL NOT NULL, \
            FOREIGN KEY(tag_key) REFERENCES tag(key) \
        )''')
    cursor.execute('''
        CREATE TABLE eval_fracsOfHosts (
            src TEXT NOT NULL,
            tag_key INTEGER NOT NULL,
            host TEXT NOT NULL,
            total REAL DEFAULT 0.0,
            frac REAL DEFAULT 0.0,
            PRIMARY KEY(src, tag_key, host),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    return cursor

def create_graphs(options):
    tags = tags_with_hellos(options, cur=options['topo_conn'].cursor())
    cursor = options['topo_conn'].cursor()
    cursor.execute('SELECT host FROM addr')
    hosts = pylab.flatten(cursor.fetchall())
    hosts = [h for h in hosts if h in options['nodes'] or options['nodes'] == 'all']

    digraphx = nx.DiGraph()
    digraphx.add_nodes_from(hosts)

    graphs = list()
    for i, (tag_key, tag_id, helloSize) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, i+1, len(tags))
        if helloSize not in options['gossip_params']['pkg_size'] and options['gossip_params']['pkg_size'][0] != 0:
            logging.debug('\t skipping size = %d', helloSize)
            continue
        links = cursor.execute('''
            SELECT src, host, pdr
            FROM eval_helloPDR
            WHERE tag_key=?
        ''', (tag_key,)).fetchall()

        digraph = digraphx.copy()
        for src, host, pdr in links:
            if src in hosts and host in hosts:
                digraph.add_edge(src, host, weight=pdr)
        graphs.append((helloSize, digraph))
    return graphs

def r():
    f = random.uniform(0, 1)
    return f

def transmit(node, G, loss):
    """
    Returns a set of nodes that received the packet from the
    transmitting node
    """
    out_edges = G.out_edges(node, data=True)
    forward = set()
    if loss:
        for src, nxt, data in out_edges:
            if data['weight'] > r():
                forward.add(nxt)
    else:
        forward = set([nxt for src, nxt, data in out_edges])

    return forward

def receive(nodes, src, seq, rx_pkgs_counter, rx_pkgs_frac, cursor):
    for n in nodes:
        t = ('', n, n+'-0', n+'-0', 'prev', src, 'dst', seq, 0)
        cursor.execute('INSERT INTO rx VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)', t)
        rx_pkgs_counter[n] += 1
        rx_pkgs_frac[n].add(seq)

def create_containers(G):
    rx_pkgs_counter = dict()
    rx_pkgs_frac = dict()
    for n in G:
        rx_pkgs_counter[n] = 0
        rx_pkgs_frac[n] = set()
    return rx_pkgs_counter, rx_pkgs_frac

def gossip0(src, G, cur_gossip_params, options, tag_key):
    loss = options['loss']
    cursor = options['db_conn'].cursor()
    fractions = list()
    forwarded_pkgs = list()
    rx_pkgs_counter, rx_pkgs_frac = create_containers(G)

    for seq, packet in enumerate(range(0, options['gossip_params']['pkg_num'])):
        t = ('', src, src+'-0', src+'-0', src, 'dst', seq, tag_key)
        cursor.execute('INSERT INTO tx VALUES (?, ?, ?, ?, ?, ?, ?, ?)', t)
        received = set()
        received.add(src)
        forward = transmit(src, G, loss)
        receive(forward, src, seq, rx_pkgs_counter, rx_pkgs_frac, cursor)
        received.update(forward)
        forwarded = 1

        while len(forward):
            new_forward = set()
            for nxt in forward:
                t = ('', nxt, nxt+'-0', nxt+'-0', src, 'dst', seq)
                cursor.execute('INSERT INTO fw VALUES (?, ?, ?, ?, ?, ?, ?)', t)
                # gossip0 magic
                if cur_gossip_params['p'] > r():
                   n = transmit(nxt, G, loss)
                   new_forward.update(n)
                   forwarded += 1
            receive(new_forward, src, seq, rx_pkgs_counter, rx_pkgs_frac, cursor)
            new_forward = new_forward.difference(received)
            received.update(new_forward)
            forward = new_forward
        forwarded_pkgs.append(forwarded)
        fractions.append(float(len(received))/len(G))
    options['db_conn'].commit()
    return fractions, rx_pkgs_counter, rx_pkgs_frac, forwarded_pkgs

def gossip3(src, G, cur_gossip_params, options, tag_key):
    loss = options['loss']
    cursor = options['db_conn'].cursor()
    fractions = list()
    forwarded_pkgs = list()
    rx_pkgs_counter, rx_pkgs_frac = create_containers(G)

    for seq, packet in enumerate(range(0, options['gossip_params']['pkg_num'])):
        t = ('', src, src+'-0', src+'-0', src, 'dst', seq, tag_key)
        cursor.execute('INSERT INTO tx VALUES (?, ?, ?, ?, ?, ?, ?, ?)', t)
        received = set()
        stored = dict()

        received.add(src)
        forward = transmit(src, G, loss)
        receive(forward, src, seq, rx_pkgs_counter, rx_pkgs_frac, cursor)
        received.update(forward)
        forwarded = 1

        while len(forward) or len(stored):
            while len(forward):
                new_forward = set()
                for nxt in forward:
                    t = ('', nxt, nxt+'-0', nxt+'-0', src, 'dst', seq)
                    cursor.execute('INSERT INTO fw VALUES (?, ?, ?, ?, ?, ?, ?)', t)
                    # gossip3 magic
                    if cur_gossip_params['p'] > r():
                        n = transmit(nxt, G, loss)
                        forwarded += 1
                        new_forward.update(n)
                    else:
                        if nxt in stored:
                            stored[nxt] += 1
                        else:
                            stored[nxt] = 0
                receive(new_forward, src, seq, rx_pkgs_counter, rx_pkgs_frac, cursor)
                new_forward = new_forward.difference(received)
                received.update(new_forward)
                forward = new_forward
            # simulate timeout
            for host in stored:
                num = stored[host]
                if num < cur_gossip_params['m']:
                    forward.add(host)
            stored = dict()
        forwarded_pkgs.append(forwarded)
        fractions.append(float(len(received))/len(G))
    options['db_conn'].commit()
    return fractions, rx_pkgs_counter, rx_pkgs_frac, forwarded_pkgs

def simulate(graph, options):
    cursor = options['db_conn'].cursor()
    gossip_params = options['gossip_params']
    pkgs = gossip_params['pkg_num']
    for gossip_num, gossip in enumerate(gossip_params['gossip']):
        logging.info('\tgossip=%s (%d/%d)', gossip, gossip_num+1, len(gossip_params['gossip']))
        for src_num, src in enumerate(gossip_params['src']):
            logging.info('\t\tsrc=%s (%d/%d)', src, src_num+1, len(gossip_params['src']))
            # start here with parameters
            cur_gossip_params = dict()
            for p in gossip_params['p']:
                for m in gossip_params['m']:
                    for k in gossip_params['k']:
                        cur_gossip_params['p'] = float(p)
                        cur_gossip_params['m'] = int(m)
                        cur_gossip_params['k'] = int(m)
                        try:
                            t = (None, '', '', p, p2, k, n, m, timeout, gossip, 0, 0, 0, 0, T_MAX, p_min, p_max, graph[0])
                            cursor.execute('INSERT INTO tag VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)', t)
                            tag_key = cursor.lastrowid
                            fractions, rx_pkgs_counter, rx_pkgs_frac, forwarded_pkgs = globals()['gossip%s' % gossip](src, graph[1], cur_gossip_params, options, tag_key)
                            for fraction in fractions:
                                t = (src, tag_key, fraction)
                                cursor.execute('INSERT INTO eval_rx_fracs_for_tag VALUES (?, ?, ?)', t)
                            for rx in rx_pkgs_counter.keys():
                                t = (src, tag_key, rx, float(rx_pkgs_counter[rx])/pkgs, float(len(rx_pkgs_frac[rx]))/pkgs)
                                cursor.execute('INSERT INTO eval_fracsOfHosts VALUES (?,?,?,?,?)', t)
                        except KeyError:
                            logging.critical('gossip variant not found: %d' % gossip)
            options['db_conn'].commit()

if __name__ == "__main__":
    main()