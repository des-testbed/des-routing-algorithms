#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib
matplotlib.use('Agg')
import sqlite3
import logging
import argparse
import sys
import re
from helpers import hostname_to_addr, tags_with_hellos, commits, sources_per_tag, mcds_berman
import numpy, pylab
import networkx as nx
import scipy
import time
import unicodedata
import copy

max_tag_length = 20

@commits
def update_database(options):
    logging.info('')
    re_jitter = re.compile(r"^.*jitter=(\d+).*$")
    re_pkg_size = re.compile(r"^.*pkg_size=(\d+).*$")
    re_pkg_interval = re.compile(r"^.*pkg_interval=(\d+(?:\.\d+)?).*$")
    re_channel = re.compile(r"^.*channel=(\d+).*$")
    re_nhdp_ht = re.compile(r"^.*nhdp_ht=(\d+).*$")
    re_nhdp_hi = re.compile(r"^.*nhdp_hi=(\d+).*$")
    re_nhdp_minpdr = re.compile(r"^.*,mpr_minpdr=(\d.\d+).*$")

    #re_mcds = re.compile(r"^.*mcds=(\d+).*$")
    #re_pkg_size = re.compile(r"^.*-(\d+)$")

    conn = options['db_conn']
    c = conn.cursor()
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'pkg_size' INTEGER
        ''')
    except sqlite3.OperationalError:
        pass
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'pkg_interval' REAL
        ''')
    except sqlite3.OperationalError:
        pass
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'channel' INTEGER
        ''')
    except sqlite3.OperationalError:
        pass
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'jitter' INTEGER
        ''')
    except sqlite3.OperationalError:
        pass
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'nhdp_ht' INTEGER
        ''')
        logging.info('added tag.nhdp_ht')
    except sqlite3.OperationalError:
        pass
    try:
        c.execute('''
            ALTER TABLE tag
            ADD COLUMN
            'nhdp_hi' INTEGER
        ''')
        logging.info('added tag.nhdp_hi')
    except sqlite3.OperationalError:
        pass
    #try:
        #c.execute('''
            #ALTER TABLE tag
            #ADD COLUMN
            #'mcds' INTEGER
        #''')
    #except sqlite3.OperationalError:
        #pass

    ids = c.execute('''
        SELECT id
        FROM tag
    ''').fetchall()
    for _id, in ids:
        re_match_size = re_pkg_size.match(_id)
        if re_match_size:
            size = int(re_match_size.group(1))
            logging.debug('matched size, tag=%s, size=%d', _id[0:max_tag_length], size)
            c.execute('''
                UPDATE tag
                SET pkg_size=?
                WHERE id=?
            ''', (size, _id))
        re_match_interval = re_pkg_interval.match(_id)
        if re_match_interval:
            interval = float(re_match_interval.group(1))
            logging.debug('matched interval, tag=%s, interval=%d', _id[0:max_tag_length], interval)
            c.execute('''
                UPDATE tag
                SET pkg_interval=?
                WHERE id=?
            ''', (interval, _id))
        re_match_channel = re_channel.match(_id)
        if re_match_channel:
            channel = int(re_match_channel.group(1))
            logging.debug('matched channel, tag=%s, channel=%d', _id[0:max_tag_length], channel)
            c.execute('''
                UPDATE tag
                SET channel=?
                WHERE id=?
            ''', (channel, _id))
        re_match_jitter = re_jitter.match(_id)
        if re_match_jitter:
            jitter = int(re_match_jitter.group(1))
            logging.debug('matched jitter, tag=%s, jitter=%d', _id[0:max_tag_length], jitter)
            c.execute('''
                UPDATE tag
                SET jitter=?
                WHERE id=?
            ''', (jitter, _id))
        re_match_nhdp_hi = re_nhdp_hi.match(_id)
        if re_match_nhdp_hi:
            nhdp_hi = int(re_match_nhdp_hi.group(1))
            logging.debug('matched nhdp_hi, tag=%s, nhdp_hi=%d', _id[0:max_tag_length], nhdp_hi)
            c.execute('''
                UPDATE tag
                SET nhdp_hi=?
                WHERE id=?
            ''', (nhdp_hi, _id))
        re_match_nhdp_ht = re_nhdp_ht.match(_id)
        if re_match_nhdp_hi:
            nhdp_ht = int(re_match_nhdp_ht.group(1))
            logging.debug('matched nhdp_ht, tag=%s, nhdp_ht=%d', _id[0:max_tag_length], nhdp_ht)
            c.execute('''
                UPDATE tag
                SET nhdp_ht=?
                WHERE id=?
            ''', (nhdp_ht, _id))
        re_match_nhdp_minpdr = re_nhdp_minpdr.match(_id)
        if re_match_nhdp_minpdr:
            nhdp_minpdr = float(re_match_nhdp_ht.group(1))
            logging.debug('matched nhdp_minpdr, tag=%s, nhdp_minpdr=%d', _id[0:max_tag_length], nhdp_minpdr)
            c.execute('''
                UPDATE tag
                SET mpr_minpdr=?
                WHERE id=?
            ''', (nhdp_minpdr, _id))


    c.execute('''
        DROP VIEW IF EXISTS view_configurations
    ''')
    logging.info("creating view")
    c.execute('''
        CREATE VIEW view_configurations
        AS SELECT
        p, p2, k, n, m, timeout, gossip, helloTTL, hello_interval, helloSize, cleanup_interval, T_MAX, p_min, p_max, pkg_size, pkg_interval, jitter, channel, nhdp_ht, nhdp_hi, mpr_minpdr
        FROM tag
    ''')

@commits
def eval_mpr_topology_changes(options):
    # calculates changes to the mpr set over time for the whole network
    # WARNING: interval needs to be set to the mpr selector advertisment interval
    logging.info('')
    interval = 2
    conn = options['db_conn']
    c = conn.cursor()
    if options['sqlsync'] == 'OFF':
        c.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        c.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    c.execute('''DROP TABLE IF EXISTS eval_mpr_topology_changes''')
    conn.commit()
    c.execute('''
              CREATE TABLE eval_mpr_topology_changes (
              tag_key INTEGER NOT NULL,
              host TEXT NOT NULL,
              sample INTEGER NOT NULL,
              abs_changes INTEGER NOT NULL,
              num_links INTEGER NOT NULL,
              num_n1 INTEGER NOT NULL,
              FOREIGN KEY(tag_key) REFERENCES tag(key)
              )''')
    conn.commit()
    hosts = list(pylab.flatten(c.execute('''SELECT host FROM addr''').fetchall()))
    tags = c.execute('''SELECT key, id, nhdp_hi, nhdp_ht, mpr_minpdr FROM tag''').fetchall()
    samples = []
    host_txif_set = {}
    for i, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        logging.info("[%d/%d] tag_id: %s nhdp_hi: %d nhdp_ht: %d mpr_minpdr: %f", i+1, len(tags), tag_id, nhdp_hi, nhdp_ht, mpr_minpdr)
        for j, (host) in enumerate(hosts):
            start_time, = c.execute('''SELECT min(time) FROM nhdp_mpr_selectors WHERE host=? AND tag_key=?''',(host, tag_key)).fetchone()
            end_time, = c.execute('''SELECT max(time) FROM nhdp_mpr_selectors WHERE host=? AND tag_key=?''', (host, tag_key,)).fetchone()
            min_he_time, = c.execute('''SELECT min(time) FROM nhdp_he WHERE host=? AND tag=?''', (host, tag_key)).fetchone()
            mpr_linkset = set()
            old_linkset = set()
            if start_time == None:
                logging.info("  [%d/%d] host: %s has no valid entries (has never been selected as mpr).", j+1, len(hosts), host)
                continue
            logging.info("  [%d/%d] host: %s", j+1, len(hosts), host)
            sample = 0
            cur_time = start_time
            while cur_time < end_time:
                mprselectors_host = c.execute('''SELECT mprselector FROM nhdp_mpr_selectors WHERE time BETWEEN ? AND ? AND host=?''', (cur_time - 0.3, cur_time + 0.3, host)).fetchall()
                num_n1 = None
                span = 0.5
                while num_n1 == None:
                    if(cur_time > min_he_time):
                        num_n1 = c.execute('''SELECT num_n1 FROM nhdp_he WHERE time BETWEEN ? AND ? AND host=?''', (cur_time - span, cur_time, host)).fetchone()
                    else:
                        num_n1 = c.execute('''SELECT num_n1 FROM nhdp_he WHERE time BETWEEN ? AND ? AND host=?''', (cur_time, cur_time + span, host)).fetchone()
                    span += 0.5
                num_n1 = num_n1[0]
                for selector, in mprselectors_host:
                    try:
                        selector_host = host_txif_set[selector]
                    except KeyError:
                        try:
                            selector_host, = c.execute('''SELECT host FROM he WHERE tx_if = ?''', (selector,)).fetchone()
                            host_txif_set[selector] = selector_host
                        except TypeError:
                            continue
                    mpr_linkset.add((host, selector_host))
                if len(old_linkset) == 0:
                    abs_change = 0
                else:
                    abs_change = abs(len(mpr_linkset.symmetric_difference(old_linkset)))
                c.execute('''INSERT INTO eval_mpr_topology_changes VALUES (?,?,?,?,?,?)''', (tag_key, host, sample, abs_change, len(mprselectors_host), num_n1))
                old_linkset = copy.deepcopy(mpr_linkset)
                mpr_linkset = set()
                cur_time = cur_time + interval
                sample = sample + 1
    conn.commit()






@commits
def eval_mpr_n1_fracs(options):
    # time window each measurement is long in seconds
    # number hellos is: 60 / helloInterval * interval
    logging.info('')
    interval = 600
    conn = options['db_conn']
    c = conn.cursor()
    if options['sqlsync'] == 'OFF':
        c.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        c.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    c.execute('''DROP TABLE IF EXISTS eval_mpr_n1_fracs''')
    conn.commit()
    c.execute('''
              CREATE TABLE eval_mpr_n1_fracs (
              sample INTEGER NOT NULL,
              num_n1 INTEGER NOT NULL,
              mean_mprs REAL NOT NULL,
              num_values INTEGER NOT NULL,
              std REAL NOT NULL
              )''')
    conn.commit()
    min_time = c.execute('''
                         SELECT min(time)
                         FROM nhdp_he
                         ''').fetchone()
    try:
        min_time = min_time[0] + 30
        max_time = min_time + interval
    except TypeError:
        logging.error("NHDP_HE table is empty, but needed for eval_mpr_n1_fracs!")
        return
    sample = 0
    while True:
        data = {}
        sample = sample + 1
        #####################################################################
        # we filter out all hellos where we have no neighbors and only pick #
        # those in the wanted time interval                                 #
        #####################################################################
        hellos_n1 = c.execute('''
                              SELECT num_n1, num_mpr
                              FROM nhdp_he
                              WHERE time BETWEEN ? and ? and num_n1 > 0
                              ''', (min_time, max_time)).fetchall()
        if len(hellos_n1) == 0:
            break
        min_time = min_time + interval
        max_time = max_time + interval
        logging.info("Evaluating %d hellos. Start: %s End: %s" %
                     (len(hellos_n1),
                      time.strftime("%d/%m/%y %H:%M", time.localtime(min_time)),
                      time.strftime("%d/%m/%y %H:%M",
                                    time.localtime(max_time)))
                    )
        #################################################################
        # fill the data array with values from the selected nhdp hellos #
        #################################################################
        for num_n1, num_mprs in hellos_n1:
            try:
                data[num_n1].append(num_mprs)
            except KeyError:
                data[num_n1] = list()
                data[num_n1].append(num_mprs)

        for num_n1, num_mprs_list in data.iteritems():
            mean_mprs = scipy.mean(num_mprs_list)
            std = scipy.std(num_mprs_list)
            c.execute('''
                      INSERT INTO eval_mpr_n1_fracs
                      VALUES (?, ?, ?, ?, ?)
                      ''', (sample, num_n1, mean_mprs, len(num_mprs_list), std))
        data = {}

@commits
def eval_mpr_n2_fracs(options):
    # time window each measurement is long in seconds
    # number hellos is: 60 / helloInterval * interval
    logging.info('')
    interval = 600
    conn = options['db_conn']
    c = conn.cursor()
    if options['sqlsync'] == 'OFF':
        c.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        c.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    c.execute('''DROP TABLE IF EXISTS eval_mpr_n2_fracs''')
    conn.commit()
    c.execute('''
              CREATE TABLE eval_mpr_n2_fracs (
              sample INTEGER NOT NULL,
              num_n2 INTEGER NOT NULL,
              mean_mprs REAL NOT NULL,
              num_values INTEGER NOT NULL,
              std REAL_NOT NULL
             )''')
    conn.commit()
    min_time = c.execute('''
                         SELECT min(time)
                         FROM nhdp_he
                         ''').fetchone()

    try:
        min_time = min_time[0] + 30
        max_time = min_time + interval
    except TypeError:
        logging.error("NHDP_HE table is empty, but needed for eval_mpr_n1_fracs!")
        return
    sample = 0
    while True:
        data = {}
        sample = sample + 1
        #####################################################################
        # we filter out all hellos where we have no neighbors and only pick #
        # those in the wanted time interval                                 #
        #####################################################################
        hellos_n2 = c.execute('''
                              SELECT num_n2, num_mpr
                              FROM nhdp_he
                              WHERE time BETWEEN ? and ? and num_n2 > 0
                              ''', (min_time, max_time)).fetchall()
        if len(hellos_n2) == 0:
            break
        min_time = min_time + interval
        max_time = max_time + interval
        logging.info("Evaluating %d hellos. Start: %s End: %s" %
                     (len(hellos_n2),
                      time.strftime("%d/%m/%y %H:%M", time.localtime(min_time)),
                      time.strftime("%d/%m/%y %H:%M",
                                    time.localtime(max_time)))
                    )
        #################################################################
        # fill the data array with values from the selected nhdp hellos #
        #################################################################
        data = {}
        for num_n2, num_mprs in hellos_n2:
            try:
                data[num_n2].append(num_mprs)
            except KeyError:
                data[num_n2] = list()
                data[num_n2].append(num_mprs)

        for num_n2, num_mprs_list in data.iteritems():
            mean_mprs = scipy.mean(num_mprs_list)
            std = scipy.std(num_mprs_list)
            c.execute('''
                      INSERT INTO eval_mpr_n2_fracs
                      VALUES (?, ?, ?, ?, ?)
                      ''', (sample, num_n2, mean_mprs, len(num_mprs_list), std))

@commits
def eval_time(options):
    '''
    Evaluates the minimum, average and maximum time a packet needed to travel
    from source to destination.
    '''
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    cursor.execute('''DROP TABLE IF EXISTS eval_time''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_time (
            tag_key INTEGER NOT NULL,
            tx_host TEXT NOT NULL,
            seq INTEGER NOT NULL,
            min_time REAL NOT NULL,
            avr_time REAL NOT NULL,
            max_time REAL NOT NULL,
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()

    tags = cursor.execute('''SELECT key, id FROM tag''').fetchall()
    for i, (tag_key, tag_id) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id[0:max_tag_length], i+1, len(tags))
        cursor.execute('''
            INSERT INTO eval_time
                SELECT tx.tag, tx.host, tx.seq, MIN(rx.time-tx.time), AVG(rx.time-tx.time), MAX(rx.time-tx.time)
                FROM tx JOIN rx
                ON tx.src=rx.src AND tx.seq=rx.seq
                WHERE tx.tag=? AND ABS(tx.time - rx.time) < 300
                GROUP BY tx.src, tx.time, tx.seq
                ORDER BY tx.src, tx.time, tx.seq
        ''', (tag_key,))

@commits
def eval_mcds(options):
    tags = tags_with_hellos(options)
    if not tags or len(tags) == 0:
        logging.warning('no tag found with gossip=(2,5,8,9,11)')
        return

    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    if options['mcds'] < 0:
        logging.info('deleting old data')
        cursor.execute('''DROP TABLE IF EXISTS eval_mcds''')
        conn.commit()
        cursor.execute('''
            CREATE TABLE eval_mcds (
                id INTEGER PRIMARY KEY,
                tag_key INTEGER NOT NULL,
                algorithm TEXT NOT NULL,
                min_pdr REAL NOT NULL,
                bidirectional INTEGER NOT NULL,
                max_reach REAL NOT NULL,
                size INTEGER NOT NULL,
                mcds TEXT NOT NULL,
                diameter INTEGER,
                avr_dist REAL,
                FOREIGN KEY(tag_key) REFERENCES tag(key)
            )''')
        cursor.execute('''CREATE INDEX id_tag_eval_mcds on eval_mcds(tag_key)''')

        cursor.execute('''DROP TABLE IF EXISTS eval_mcds_vectors''')
        conn.commit()
        cursor.execute('''
            CREATE TABLE eval_mcds_vectors (
                id INTEGER NOT NULL,
                eccentricity INTEGER NOT NULL,
                hops INTEGER NOT NULL,
                mean_reachability_gain REAL NOT NULL,
                mean_reachability_gain2 REAL NOT NULL,
                FOREIGN KEY(id) REFERENCES eval_mcds(id)
            )''')
        cursor.execute('''CREATE INDEX id_id_eval_mcds_vectors on eval_mcds_vectors(id)''')
        conn.commit()

        cursor.execute('''DROP TABLE IF EXISTS eval_mcds_eccentricities''')
        conn.commit()
        cursor.execute('''
            CREATE TABLE eval_mcds_eccentricities (
                id INTEGER NOT NULL,
                eccentricity INTEGER NOT NULL,
                fraction REAL NOT NULL,
                FOREIGN KEY(id) REFERENCES eval_mcds(id)
            )''')
        cursor.execute('''CREATE INDEX id_id_eval_mcds_eccentricities on eval_mcds_eccentricities(id)''')
        conn.commit()
    hosts = list(pylab.flatten(cursor.execute('''SELECT host FROM addr''').fetchall()))
    graphx = nx.Graph()
    graphx.add_nodes_from(hosts)

    min_pdrs = pylab.linspace(0.0, 1.0, 11)
    #min_pdrs = pylab.linspace(0.0, 1.0, 101)

    for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id[0:max_tag_length], i+1, len(tags))
        for min_pdr in min_pdrs:
            graph_tmp = graphx.copy()
            bigraph_tmp = graphx.copy()
            links = cursor.execute('''
                SELECT src, host, pdr
                FROM eval_helloPDR
                WHERE tag_key=?
            ''', (tag_key,)).fetchall()

            bilinks = cursor.execute('''
                SELECT A.src, A.host, A.pdr, B.pdr
                FROM eval_helloPDR as A JOIN eval_helloPDR as B
                ON A.host=B.src AND B.host=A.src
                WHERE A.tag_key=? AND B.tag_key=?
            ''', (tag_key, tag_key)).fetchall()

            for src, host, pdr in links:
                if pdr >= min_pdr:
                    graph_tmp.add_edge(src, host, weight=pdr)

            for a, b, pdr1, pdr2 in bilinks:
                if pdr1 >= min_pdr and pdr2 >= min_pdr:
                    bigraph_tmp.add_edge(a, b, weight=(pdr1+pdr2)*0.5)


            def eval_mcds(G, mcds):
                G_MCDS = G.subgraph(mcds) # induced subgraph
                assert(len(nx.connected_components(G_MCDS)) == 1)
                for node in [n for n in G.nodes() if n not in mcds]: # add edges between MCDS and non-MCDS nodes
                    G_MCDS.add_node(node)
                    edges = [(node, node2) for node2 in G.nodes() if node2 in mcds and (node, node2) in G.edges() or (node2, node) in G.edges()]
                    assert(len(edges))
                    G_MCDS.add_edges_from(edges)
                assert(len(G) == len(G_MCDS))
                diameter = nx.diameter(G_MCDS)
                avr_dist = nx.average_shortest_path_length(G_MCDS)
                return diameter, avr_dist, G_MCDS

            def avr_gain_per_hop(G, cursor, key):
                all_paths = nx.all_pairs_shortest_path(G_MCDS)
                diameter = nx.diameter(G)
                gains_per_hop = dict()

                for i,src in enumerate(all_paths.keys()):
                    gains = dict()
                    eccentricity = nx.eccentricity(G, src)
                    assert(eccentricity > 0)
                    for hops in range(0, eccentricity+1):
                        gains[hops] = set()
                    for dst in [v for v in all_paths[src].keys() if v != src]:
                        dist = len(all_paths[src][dst][1:])
                        gains[dist].add(dst)
                    nodes_per_dist = numpy.array([len(gains[dist]) for dist in sorted(gains.keys())])
                    assert(sum(nodes_per_dist) == len(G.nodes())-1)
                    try:
                        gains_per_hop[eccentricity] = numpy.vstack((gains_per_hop[eccentricity], nodes_per_dist))
                    except KeyError:
                        gains_per_hop[eccentricity] = nodes_per_dist.reshape(1, len(nodes_per_dist))

                for i in range(0, max(gains_per_hop.keys())+1):
                    if i not in gains_per_hop:
                        gains_per_hop[i] = numpy.array([])
                entries = [len(gains_per_hop[v]) for v in sorted(gains_per_hop.keys())]
                for i, e in enumerate(entries):
                    if e == 0:
                        continue
                    cursor.execute('''
                        INSERT INTO eval_mcds_eccentricities
                        VALUES (?, ?, ?)
                    ''', (key, i, e/float(sum(entries))))

                assert(sum(entries) == len(G.nodes()))
                for eccentricity in sorted(gains_per_hop.keys()):
                    summed = numpy.array([sum(v) for v in gains_per_hop[eccentricity].transpose()])
                    summed1 = summed/float(sum(summed))
                    summed2 = summed/float(sum(summed)) * len(G)/104.0
                    #print eccentricity, sum(summed), len(G)
                    #print summed1, sum(summed1)
                    #print summed2, sum(summed2)
                    assert((sum(summed1) - 1.0) <= sys.float_info.epsilon)
                    for hops, mean_reachability_gain in enumerate(summed1):
                        assert(hops <= eccentricity)
                        cursor.execute('''
                            INSERT INTO eval_mcds_vectors
                            VALUES (?, ?, ?, ?, ?)
                        ''', (key, eccentricity, hops, mean_reachability_gain, summed2[hops]))

            ###### determine MCDS ######
            #if not nx.is_connected(graph_tmp):
                #graph_tmp = nx.subgraph(graph_tmp, nx.connected_components(graph_tmp)[0])
                #logging.warning('\tGraph is not connected, using largest component, nodes=%d' % len(graph_tmp))
            #if len(graph_tmp) < len(hosts)*0.7:
                #logging.warning('\tless than 70\% of nodes left in graph -> skipping')
            #else:
                #cds = mcds_berman(graph_tmp, abs(options['mcds']))
                #cds_length = [len(c) for c in cds]
                #logging.info('\tmin_pdr = %.2f, MCDS size = %d' % (min_pdr, min(cds_length)))
                #mcds_set = [set(c) for c in cds if len(c) == min(cds_length)]
                #mcds_unique = list()
                #for mcds in mcds_set:
                    #if mcds not in mcds_unique:
                        #mcds_unique.append(mcds)
                        #diameter, avr_dist = None, None # eval_mcds(graph_tmp, mcds)
                        #avr_gph = None
                        #cursor.execute('''
                            #INSERT INTO eval_mcds
                            #VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        #''', (tag_key, 'berman', min_pdr, 0, len(graph_tmp.node)/float(len(hosts)), min(cds_length), ','.join(sorted(mcds)), diameter, avr_dist, avr_gph))

            ###### determine SCDS ######
            if not nx.is_connected(bigraph_tmp):
                bigraph_tmp = nx.subgraph(bigraph_tmp, nx.connected_components(bigraph_tmp)[0])
                #logging.warning('\tGraph is not connected, using largest component, nodes=%d' % len(bigraph_tmp))
            #if len(bigraph_tmp) < len(hosts)*0.7:
            if len(bigraph_tmp) == 1:
                logging.warning('\tless than 70\% of nodes left in graph -> skipping')
            else:
                cds = mcds_berman(bigraph_tmp, abs(options['mcds']))
                cds_length = [len(c) for c in cds]
                logging.info('\tmin_pdr = %.2f, min MCDS size = %d' % (min_pdr, min(cds_length)))
                mcds_set = [set(c) for c in cds if len(c) == min(cds_length)]
                mcds_unique = list()
                for mcds in mcds_set:
                    if mcds not in mcds_unique:
                        mcds_unique.append(mcds)
                        diameter, avr_dist, G_MCDS = eval_mcds(bigraph_tmp, mcds)
                        cursor.execute('''
                            INSERT INTO eval_mcds
                            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ''', (None, tag_key, 'berman', min_pdr, 1, len(bigraph_tmp.node)/float(len(hosts)), min(cds_length), ','.join(sorted(mcds)), diameter, avr_dist))
                        conn.commit()
                        avr_gain_per_hop(G_MCDS, cursor, cursor.lastrowid)

@commits
def eval_distance(options):
    """
    Determines the distance of each router from each source based on the hop count field
    in the DES-SERT message.
    """
    conn = options['db_conn']
    c = conn.cursor()
    ##########################################################################################
    # Stores the length of the path each packet took from the source to
    # a particular host
    # Note: There are duplicate rows, i.e., there is a row for each packet sent by the source
    ##########################################################################################
    c.execute('''DROP TABLE IF EXISTS eval_distance''')
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    conn.commit()
    c.execute('''
        CREATE TABLE eval_distance (
            tag_key INTEGER NOT NULL,
            src TEXT NOT NULL,
            host TEXT NOT NULL,
            dist INTEGER NOT NULL,
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    evaluated = False
    ################################################################################
    # Evaluate for all sources
    ################################################################################
    for i, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, i+1, len(options['src']))
        src_addr = hostname_to_addr(src, options)
        ################################################################################
        # Get all tags for which the source sent packets with gossip0(1.0) == flooding
        ################################################################################
        tags = c.execute('''
            SELECT key, id
            FROM tag
            WHERE gossip=? AND p=?
        ''', (0, 1.0)).fetchall()
        for j, (tag_key, tag_id) in enumerate(tags):
            logging.info('\ttag=%s (%d/%d)', tag_id[0:max_tag_length], j+1, len(tags))
            evaluated = True
            send_pkts = c.execute('''
                SELECT time, seq
                FROM tx
                WHERE host=? AND tag=?
                ORDER BY time, seq
            ''', (src, tag_key)).fetchall()
            logging.debug('host %s sent %d packets for tag=%s', src, len(send_pkts), tag_id[0:max_tag_length])
            if len(send_pkts) == 0:
                logging.warning('source node %s did not sent any packets for tag %s', src, tag_id[0:max_tag_length])
                continue
            for time, tx_seq in send_pkts:
                c.execute('''
                    SELECT host, hops
                    FROM rx
                    WHERE src=? AND seq=? AND NOT host=? AND ABS(time - rx.time) < 300
                    ORDER BY host
                ''', (src_addr, tx_seq, src))
                receivers = c.fetchall()
                for host, hops in receivers:
                    c.execute('''
                        INSERT INTO eval_distance
                        VALUES(?,?,?,?)
                    ''', (tag_key, src, host, hops))
        conn.commit()
    if not evaluated:
        logging.warning('found no tag with gossip=0 and p=1.0')

@commits
def eval_retransmission(options):
    """
    Calculates for each src and sent packet, the different values of
    reachability for m retransmissions
    """
    conn = options['db_conn']
    c = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    c.execute('''DROP TABLE IF EXISTS eval_reachability''')
    conn.commit()
    c.execute('''
        CREATE TABLE eval_reachability (
            tag_key INTEGER NOT NULL,
            src TEXT NOT NULL,
            iteration INTEGER NOT NULL,
            reachability REAL NOT NULL,
            gain REAL NOT NULL,
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    ################################################################################
    # Get number of receivers
    ################################################################################
    all_hosts = set(c.execute('''
        SELECT host
        FROM addr''').fetchall())
    logging.info('%d potential destination nodes found in DB', len(all_hosts))
    ################################################################################

    ############################
    # Evaluate for all sources #
    ############################
    for src_num, src in enumerate(options['src']):
        logging.info("src: (%d/%d)" % (src_num + 1, len(options['src'])))
        src_addr = hostname_to_addr(src, options)
        #########################
        # Evaluate for each tag #
        #########################
        tags = c.execute('''SELECT key, id FROM tag''').fetchall()
        for j, (tag_key, tag_id) in enumerate(tags):
            ########################
            # Get sequence numbers #
            ########################
            sent_pkts = c.execute('''
                SELECT time, seq
                FROM tx
                WHERE host=? AND tag=?
                ORDER BY time, seq
                ''', (src, tag_key)).fetchall()
            if len(sent_pkts) == 0:
                logging.debug("src: (%d/%d) tag: (%d/%d) SKIPPED (no packets sent)" %  (src_num + 1, len(options['src']), j + 1, len(tags)))
                continue
            # calculate the number of possible sequence number samples
            retransmissions = options['retransmissions']
            if retransmissions == 'max':
                retransmissions = len(sent_pkts)-1
            #num_samples = len(sent_pkts) - retransmissions
            num_samples = 10
            for sample_nr in range(0, num_samples):
                # get the first and last sequence number we will look at now
                seq_hosts = {}
                seq_list_str = ','.join([str(k) for t,k in sent_pkts[sample_nr:sample_nr + retransmissions]])
                query = '''
                        SELECT DISTINCT(seq), host
                        FROM rx
                        WHERE NOT host=? AND src=? AND seq IN (%s) AND abs(%f - rx.time) < 300
                    ''' % (seq_list_str, sent_pkts[0][0])
                rcvd_by_list = c.execute(query, (src, src_addr)).fetchall()
                #for time, seq in sent_pkts:
                #    seq_hosts[seq] = list()
                # insert a host if it received sequence number seq
                for seq, host in rcvd_by_list:
                    try:
                        seq_hosts[seq].append(host)
                    except KeyError:
                        seq_hosts[seq] = list()
                        seq_hosts[seq].append(host)
                # calculate reachability
                rx_total = set()
                #assert(len(sent_pkts) == len(seq_hosts))
                for i, (k, v) in enumerate(seq_hosts.iteritems()):
                    rx_total_prev = rx_total
                    rx_total = rx_total.union(set(v))
                    reachability = float(len(rx_total)) / float(len(all_hosts)-1)
                    gain = reachability - float(len(rx_total_prev)) / float(len(all_hosts)-1)
                    c.execute('''
                        INSERT INTO eval_reachability
                        VALUES (?, ?, ?, ?, ?)
                    ''', (tag_key, src, i, reachability, gain))

@commits
def eval_hello_pdr(options):
    """
    Evaluates the link quality based on HELLO packets sent by one
    node (src) and received by anoter (host).
    """
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    ##########################################################################################
    # Stores the link qualities measured with HELLO messages
    ##########################################################################################
    cursor.execute('''DROP TABLE IF EXISTS eval_helloPDR''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_helloPDR (
            tag_key INTEGER NOT NULL,
            tx_if TEXT NOT NULL,
            src TEXT NOT NULL,
            rx_if TEXT NOT NULL,
            host TEXT NOT NULL,
            pdr REAL NOT NULL,
            rssi REAL DEFAULT 0.0,
            PRIMARY KEY(tag_key, tx_if, src, rx_if, host),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()

    tags = tags_with_hellos(options)
    if not tags or len(tags) == 0:
        logging.warning('no tag found with gossip=(2,5,8,9)')
        return
    for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=%s, size=%d, interval=%d (%d/%d)', tag_id[0:max_tag_length], helloSize, helloInterval, i, len(tags))
        cursor.execute('''
            INSERT INTO eval_helloPDR
            SELECT L.key, L.src_if, L.src, L.dst_if, L.dst, CAST(L.rcvd AS REAL) / R.total AS frac, L.RSSI
            FROM (
                SELECT tag.key, he.host AS src, he.tx_if AS src_if, rh.host AS dst, rh.prev AS dst_if, COUNT(rh.seq) as rcvd, AVG(rh.rssi) AS RSSI
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag
                AND he.seq = rh.seq
                AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND ABS(he.time - rh.time) < 300
                GROUP BY tag.key, he.host, rh.host
                ORDER BY tag.key, he.host, rh.host
            ) AS L
            JOIN (
                SELECT tag.key, he.host, COUNT(he.seq) as total
                FROM tag JOIN he
                ON tag.key = he.tag
                GROUP BY tag.key, he.host
            ) AS R
            ON L.key = R.key AND L.src = R.host
            ORDER BY L.src, frac;
        ''', (tag_key, ))
        conn.commit()

@commits
def eval_nhdp_hello_pdr(options):
    """
    Evaluates the link quality based on HELLO packets sent by one
    node (src) and received by anoter (host).
    """
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    ##########################################################################################
    # Stores the link qualities measured with HELLO messages
    ##########################################################################################
    cursor.execute('''DROP TABLE IF EXISTS eval_helloPDR''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_helloPDR (
            tag_key INTEGER NOT NULL,
            tx_if TEXT NOT NULL,
            src TEXT NOT NULL,
            rx_if TEXT NOT NULL,
            host TEXT NOT NULL,
            pdr REAL NOT NULL,
            rssi REAL DEFAULT 0.0,
            PRIMARY KEY(tag_key, tx_if, src, rx_if, host),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()

    tags = tags_with_hellos(options)
    if not tags or len(tags) == 0:
        logging.warning('no tag found with gossip=(2,5,8,9)')
        return
    for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=%s, size=%d, interval=%d (%d/%d)', tag_id[0:max_tag_length], helloSize, helloInterval, i, len(tags))
        cursor.execute('''
            INSERT INTO eval_helloPDR
            SELECT L.key, L.src_if, L.src, L.dst_if, L.dst, CAST(L.rcvd AS REAL) / R.total AS frac, L.RSSI
            FROM (
                SELECT tag.key, he.host AS src, he.tx_if AS src_if, rh.host AS dst, rh.prev AS dst_if, COUNT(rh.seq) as rcvd, AVG(rh.rssi) AS RSSI
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag
                AND he.seq = rh.seq
                AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND ABS(he.time - rh.time) < 300
                GROUP BY tag.key, he.host, rh.host
                ORDER BY tag.key, he.host, rh.host
            ) AS L
            JOIN (
                SELECT tag.key, he.host, COUNT(he.seq) as total
                FROM tag JOIN he
                ON tag.key = he.tag
                GROUP BY tag.key, he.host
            ) AS R
            ON L.key = R.key AND L.src = R.host
            ORDER BY L.src, frac;
        ''', (tag_key, ))
        conn.commit()

@commits
def eval_rx_fracs_for_tag(options):
    """
    Evaluate which fraction of nodes received the packets sent by each source.
    """
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    cursor.execute('''DROP TABLE IF EXISTS eval_fracOverP''') # legacy
    cursor.execute('''DROP TABLE IF EXISTS eval_rx_fracs_for_tag''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_rx_fracs_for_tag (
            src TEXT NOT NULL,
            tag_key INTEGER NOT NULL,
            frac REAL NOT NULL,
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    ################################################################################
    # Get number of receivers
    ################################################################################
    dest_nodes = cursor.execute('''
        SELECT COUNT(host)
        FROM addr''').fetchone()[0] - 1
    logging.debug('%d potential destination nodes found in DB', dest_nodes)
    ################################################################################
    # Get all tags
    ################################################################################
    tags = cursor.execute('''
        SELECT key, id
        FROM tag
    ''').fetchall()
    ################################################################################
    # Evaluate all tags for all sources
    ################################################################################
    for i, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, i+1, len(options['src']))
        src_addr = hostname_to_addr(src, options)
        fraction_of_nodes_list = []
        sample_size = numpy.infty
        truncate = False
        for j, (tag_key, tag_id) in enumerate(tags):
            # number of packets the source sent for the tag
            pkts, = cursor.execute('''
                SELECT COUNT(DISTINCT(seq))
                FROM tx
                WHERE host=? AND tag=?
            ''', (src, tag_key)).fetchone()
            if pkts ==0:
                continue
            logging.info('\ttag=\"%s\" (%d/%d), src=%s sent %d packets', tag_id[0:max_tag_length], j+1, len(tags), src, pkts)
            result = cursor.execute(''' \
                SELECT seq, COUNT(DISTINCT(host))
                FROM rx
                WHERE src=? AND NOT host=? AND seq IN (
                    SELECT seq FROM tx
                    WHERE host=? AND tag=?
                )
                GROUP BY seq
            ''', (src_addr, src, src, tag_key)).fetchall()
            fraction_of_nodes = [float(num_hosts)/dest_nodes for seq, num_hosts in result]
            fraction_of_nodes.extend([0.0 for x in range(0, pkts-len(fraction_of_nodes))])
            assert(pkts == len(fraction_of_nodes))
            if not options['notruncate'] and sample_size != numpy.infty and sample_size != pkts:
                logging.warning('data sets have different sizes; data will be truncated: %d != %d', sample_size, pkts)
                truncate = True
            sample_size = min(pkts, sample_size)
            fraction_of_nodes_list.append((tag_key, fraction_of_nodes))
        ################################################################################
        # Truncate if data sets have different lengths
        ################################################################################
        if truncate:
            fraction_of_nodes_list = [(key, fraction_of_nodes[0:sample_size]) for key, fraction_of_nodes in fraction_of_nodes_list]
        ################################################################################
        # Insert fractions in table
        ################################################################################
        for key, fracs in fraction_of_nodes_list:
            for frac in fracs:
                cursor.execute('''
                    INSERT INTO eval_rx_fracs_for_tag
                    VALUES (?, ?, ?)
                ''', (src, key, frac))
        conn.commit()

@commits
def eval_rx_fracs_of_hosts(options):
    """
    Evaluate how many packets sent by each source were received by each router (host).
    The total number of packets is determined, including duplicates, and the fraction
    of packets, i.e., distinct packets received by the router / packets sent by the source.
    """
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    ##########################################################################################
    # Stores the absolute number of received packets for each host for each tag and source
    ##########################################################################################
    cursor.execute('''DROP TABLE IF EXISTS eval_fracsOfHosts''')
    conn.commit()
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
    ##########################################################################################
    # Stores the how many packages each node received from the others as previous hop
    ##########################################################################################
    cursor.execute('''DROP TABLE IF EXISTS eval_prevHopFraction''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_prevHopFraction (
            src TEXT NOT NULL,
            tag_key INTEGER NOT NULL,
            cur TEXT NOT NULL,
            prev TEXT NOT NULL,
            frac REAL NOT NULL,
            PRIMARY KEY(src, tag_key, cur, prev),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    ################################################################################
    # Evaluate for all sources
    ################################################################################
    for i, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, i+1, len(options['src']))
        src_addr = hostname_to_addr(src, options)
        ################################################################################
        # Evaluate received packets for each tag for each node
        ################################################################################
        tags = cursor.execute('''
            SELECT key, id
            FROM tag
        ''').fetchall()
        for k, (tag_key, tag_id) in enumerate(tags):
            ################################################################################
            # Get sequence numbers of the sent data packets (HELLOs are in table he!)
            ################################################################################
            cursor.execute('''
                SELECT DISTINCT(seq)
                FROM tx WHERE host=? AND tag=?
                ORDER BY seq
            ''', (src, tag_key))
            pkts = len(list(pylab.flatten(cursor.fetchall())))
            if pkts == 0:
                logging.debug('source node %s did not sent any packets for tag %s', src, tag_id[0:max_tag_length])
                continue
            logging.info('\t%d packets sent for tag=\"%s\" (%d/%d)', pkts, tag_id[0:max_tag_length], k+1, len(tags))
            ################################################################################
            # Evaluate how many times the nodes received the sent packets
            ################################################################################
            cursor.execute('''
                INSERT INTO eval_fracsOfHosts
                    SELECT ? AS FOO, ? AS BAR, addr.host AS dst, ifnull(R.total, 0.0)/? as rx_total, ifnull(R.dist, 0.0)/? AS rx_fraction
                    FROM addr LEFT JOIN (
                        SELECT rx.host AS host, COUNT(rx.seq) AS total, COUNT(DISTINCT(rx.seq)) AS dist
                        FROM rx
                        WHERE rx.src=? AND NOT rx.host=? AND rx.seq IN (
                            SELECT seq FROM tx
                            WHERE tx.host=? AND tx.tag=?
                        )
                        GROUP BY rx.host) AS R
                    ON addr.host = R.host
                    WHERE NOT addr.host = ?
            ''', (src, tag_key, float(pkts), float(pkts), src_addr, src, src, tag_key, src))
            conn.commit()
            ################################################################################
            # Evaluate how many times each node received packets from its neighbors
            ################################################################################
            #prev_hop_data = cursor.execute('''
                #SELECT host, prev, COUNT(prev)
                #FROM rx
                #WHERE src=? AND NOT host=? AND seq IN (
                    #SELECT seq FROM tx
                    #WHERE host=? AND tag=?
                #)
                #GROUP BY host, prev
            #''', (src_addr, src, src, tag_key)).fetchall()
            #for dst, prev_addr, prev_count in prev_hop_data:
                #prev_count = float(prev_count)/rx_total
                #cursor.execute('''
                    #INSERT INTO eval_prevHopFraction
                    #VALUES (?,?,?,?,?)
                #''', (src, tag_key, dst, prev_addr, prev_count))
            #conn.commit()

@commits
def eval_traveled_hops(options):
    logging.info('Evaluating traveled hop distance')
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    cursor.execute('''DROP TABLE IF EXISTS eval_traveled_hops''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_traveled_hops (
            tag_key INTEGER NOT NULL,
            hops REAL NOT NULL,
            PRIMARY KEY(tag_key),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()

    tags = cursor.execute('''SELECT key from tag''').fetchall()
    for i, (tag,) in enumerate(tags):
        logging.info('(%d/%d)', i+1, len(tags))
        cursor.execute('''
            INSERT INTO eval_traveled_hops
                SELECT tag AS T, AVG(hops) as A
                FROM rx JOIN tx
                ON tx.src=rx.src AND tx.seq=rx.seq
                WHERE T=? AND ABS(tx.time - rx.time) < 300
                GROUP BY T
                ORDER BY T;
        ''', (tag,))
        conn.commit()

@commits
def eval_sources_per_tag(options):
    logging.info('Evaluating number of sources per tag')
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    cursor.execute('''DROP TABLE IF EXISTS eval_sources_per_tag''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_sources_per_tag (
            tag_key INTEGER NOT NULL,
            sources INTEGER NOT NULL,
            PRIMARY KEY(tag_key),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    cursor.execute('''
        INSERT INTO eval_sources_per_tag
        SELECT tag, COUNT(DISTINCT(host))
        FROM tx
        GROUP BY tag
        ORDER BY tag
    ''')

@commits
def eval_fw_packets_per_tag(options):
    logging.info('Evaluating number of forwarded packets per tag')
    conn = options['db_conn']
    cursor = conn.cursor()
    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    cursor.execute('''DROP TABLE IF EXISTS eval_fw_packets_per_tag''')
    conn.commit()
    cursor.execute('''
        CREATE TABLE eval_fw_packets_per_tag (
            tag_key INTEGER NOT NULL,
            forwarded INTEGER NOT NULL,
            PRIMARY KEY(tag_key),
            FOREIGN KEY(tag_key) REFERENCES tag(key)
        )''')
    conn.commit()
    cursor.execute('''
        INSERT INTO eval_fw_packets_per_tag
            SELECT tag, count(*)
            FROM fw AS F JOIN (
                SELECT tag, src as TSRC, seq AS TSEQ, time AS TXTIME
                FROM tx
            )
            ON F.src = TSRC AND F.seq = TSEQ
            WHERE ABS(TXTIME - F.time) < 300
            GROUP BY tag
            ORDER BY tag
    ''')

def main():
    parser = argparse.ArgumentParser(description='Evaluate data stored in an sqlite database')
    parser.add_argument('--db', default='./sqlite.db', help='name of the database where the parsed data is stored')
    parser.add_argument('--src', nargs='+', default=['all'], help='source node sending packets')
    parser.add_argument('--execute', '-e', nargs='+', default=['all'], help='Execute the specified function(-s)')
    parser.add_argument('--list', nargs='?', const=True, default=False, help='List all callable plotting functions')
    parser.add_argument('--notruncate', nargs='?', const=True, default=False, help='Do not truncate data so that the reachability is always determined over the same number of packets (default=True)')
    parser.add_argument('--retransmissions', '-r', default=10, help='Number of retransmissions for \"eval_reachability\"')
    parser.add_argument('--mcds', '-m', type=int, default=100, help='Number of approximated MCDS to calculate; negative numbers will delete old entries"')
    parser.add_argument('--sqlsync', default='FULL', help='Value of the PRAGMA synchronous, handed to the sql db.')
    args = parser.parse_args()

    if args.list:
        print 'Callable functions:'
        for f in sorted([key for key in globals().keys() if key.startswith('eval_')]):
            print '\t'+f
        sys.exit(0)

    options = {}
    options['log_level'] = logging.INFO
    options['src'] = args.src
    options['notruncate'] = args.notruncate
    options['sqlsync'] = args.sqlsync
    #print options['notruncate']
    if args.retransmissions == 'max':
        options['retransmissions'] = args.retransmissions
    else:
        options['retransmissions'] = int(args.retransmissions)
    options['mcds'] = int(args.mcds)

    logging.basicConfig(level=options['log_level'], format='%(levelname)s [%(funcName)s] %(message)s')

    logging.info('connecting to database')
    conn = sqlite3.connect(args.db)
    options['db_conn'] = conn
    cursor = conn.cursor()
    logging.warning('Setting sqlite to mode: synchronous=%s' % options['sqlsync'])

    if options['sqlsync'] == 'OFF':
        cursor.execute('''
            PRAGMA synchronous=OFF
            ''')
    elif options['sqlsync'] == 'NORMAL':
        cursor.execute('''
            PRAGMA synchronous=NORMAL
            ''')
    ##########################################################################################
    # Resolve source = "all" to hostnames of nodes that have sent packets
    ##########################################################################################
    if 'all' in args.src:
        logging.info('all sources selected for evaluation')
        sources = cursor.execute('''
            SELECT DISTINCT(host)
            FROM tx
            ORDER BY host
        ''').fetchall()
        options['src'] = list(pylab.flatten(sources))

    logging.info('starting evaluation')
    update_database(options)
    if 'all' in args.execute:
        #for f in sorted([globals()[key] for key in globals().keys() if key.startswith('eval_')]):
        #    f(options)
        eval_time(options)
        eval_distance(options)
        eval_hello_pdr(options)
        eval_retransmission(options)
        eval_rx_fracs_for_tag(options)
        eval_mcds(options)
        eval_rx_fracs_of_hosts(options)
        eval_mpr_n1_fracs(options)
        eval_mpr_n2_fracs(options)
        eval_traveled_hops(options)
        eval_sources_per_tag(options)
        eval_fw_packets_per_tag(options)
        eval_mpr_topology_changes(options)

    else:
        for f in args.execute:
            try:
                globals()[f](options)
            except KeyError:
                logging.critical('function not found: %s' % f)
    logging.info('evaluation finished')
    cursor.close()

if __name__ == "__main__":
    main()
