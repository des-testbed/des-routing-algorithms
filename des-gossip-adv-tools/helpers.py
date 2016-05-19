#!/usr/bin/env python
# -*- coding: utf-8 -*-

import psycopg2
import getpass
import logging
import os, sys
import scipy
import sqlite3
import matplotlib
import matplotlib.cm as cm
from matplotlib.patches import Polygon, Rectangle
from matplotlib.collections import PatchCollection
from mpl_toolkits.mplot3d import Axes3D
import math
import scipy.special as sp
import scipy.signal
from scipy.stats import chi2
from scipy.stats.kde import gaussian_kde
import networkx as nx
import random
import copy
import matplotlib.pyplot as plt
import numpy
import pylab

matplotlib.rc('text', usetex=True)

mathsymbols = '\n'.join([line for line in open('/home/blywis/svn/cst-phd/blywis/Text/text/mathsymbols.tex')])
matplotlib.rcParams['text.latex.preamble'] = r'''
    \usepackage{euler}
    \usepackage{amsmath}
    \usepackage{amsfonts}
    \usepackage{units}

    %s
''' % mathsymbols
matplotlib.rcParams['lines.linewidth'] = 2
matplotlib.rcParams['grid.color'] = 'lightslategray'
matplotlib.rcParams['grid.linestyle'] = 'dotted'
matplotlib.rcParams['grid.linewidth'] = 0.25
matplotlib.rcParams['font.family'] = 'serif'
matplotlib.rcParams['font.serif'] = 'Computer Modern Roman'
matplotlib.rcParams['font.size'] = '24'

def data2dict(data):
    results = dict()
    for sources, frac in data:
        try:
            d1 = results[int(sources)]
        except KeyError:
            d1 = list()
            results[int(sources)] = d1
        d1.append(float(frac))
    return results

def add_legend(options, fig, labels, alpha=0.5):
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(labels)))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(labels)))
        proxies = []
        for i, label in enumerate(labels):
            r = Rectangle((0, 0), 1, 1, facecolor=colors[i], edgecolor='black', alpha=alpha)
            proxies.append(r)
        fig.ax.legend(proxies, labels, loc='best', labelspacing=0.1)
        fig.legend = True

def mcds_setCover(_G, num=1):
    '''
    _G weighted graph
    '''

    def weight(s, covered, w=1):
        return len(s.difference(covered))

    def greedy_set_cover(universe, elements):
        sc = set()
        covered = set()
        while len(covered) != len(universe):
            max_weight = 0
            for n, s, w in elements:
                if weight(s, covered, w) > max_weight:
                    max_weight = len(s)
                    covered.update(s)
                    sc.add(n)
                    break
            elements.remove((n,s,w))
        return sc

    def steiner(ds, G):
        return ds

    cds = list()
    for i in range(0, num):
        G = _G.copy()
        universe = G.node
        elements = list()
        for n in universe:
            s = set(G.neighbors(n))
            s.add(n)
            elements.append((n,s, 1))
            ecopy = copy.deepcopy(elements)
            random.shuffle(ecopy)
        ds = greedy_set_cover(universe, ecopy)
        cds.append(steiner(ds, G))
    return cds

def mcds_berman(_G, num=1):
    def whites(G):
        return [n for n in G.node if G.node[n]['color'] == 'white']

    def greys(G):
        return [n for n in G.node if G.node[n]['color'] == 'grey']

    def blacks(G):
        return [n for n in G.node if G.node[n]['color'] == 'black']

    def pieces(G):
        black_components = len(set([G.node[n]['id'] for n in G.node if G.node[n]['color'] == 'black']))
        return black_components + len(whites(G))

    def randomly_connect(G):
        bs = blacks(G)
        paths = nx.shortest_path(G)
        #print sorted(bs)

        for src in paths.keys():
            if src not in bs:
                del paths[src]
                continue
            for dst in paths[src].keys():
                p = paths[src][dst]
                if dst not in bs:
                    del paths[src][dst]
                    continue
            if len(paths[src]) == 0:
                del paths[src]
                continue

        for src in paths.keys():
            for dst in paths[src].keys():
                p = paths[src][dst]
                if G.node[src]['id'] == G.node[p[-1]]['id']:
                    del paths[src][dst]
                    continue
                if src == p[-1]:
                    del paths[src][dst]
                    continue
            if len(paths[src]) == 0:
                del paths[src]
                continue

        for src in paths.keys():
            for dst in paths[src].keys():
                p = paths[src][dst]
                if len(p) > 4:
                    #print p
                    del paths[src][dst]
                    continue
            if len(paths[src]) == 0:
                del paths[src]
                continue

        if not len(paths):
            logging.warning('no paths left')
            return False

        min_length = 100000
        for src in paths.keys():
            for dst in paths[src].keys():
                min_length = min(len(paths[src][dst]), min_length)

        for src in paths.keys():
            for dst in paths[src].keys():
                if len(paths[src][dst]) >  min_length:
                    del paths[src][dst]
            if len(paths[src]) == 0:
                del paths[src]

        src_name =  random.choice(paths.keys())
        src = paths[src_name]
        path = src[random.choice(src.keys())]
        assert(len(path) <= 4)
        assert(src_name in bs)
        assert(path[-1] in bs)
        assert(G.node[src_name]['id'] != G.node[path[-1]]['id'])

        for n in path[0:-1]:
            color_black(n, G)
        return True

    def color_black(n, G):
        G.node[n]['color'] = 'black'
        black_neighbor_ids = set()
        for neighbor in G.neighbors(n):
            if G.node[neighbor]['color'] == 'white':
                G.node[neighbor]['color'] = 'grey'
            elif G.node[neighbor]['color'] == 'black':
                black_neighbor_ids.add(G.node[neighbor]['id'])
        black_to_update = [m for m in G.node if G.node[m]['id'] in black_neighbor_ids]
        for update in black_to_update:
            G.node[update]['id'] = G.node[n]['id']

    def reduction(n, G):
        red = 1
        for neighbor in G.neighbors(n):
            if G.node[neighbor]['color'] == 'white':
                red += 1
        return red

    def phase1(G):
        logging.debug('')
        while len(whites(G)):
            best_node = None
            best_reduction = 0
            nodes = whites(G) + greys(G)
            random.shuffle(nodes)
            for n in nodes:
                red = reduction(n, G)
                if red > best_reduction:
                    best_reduction = red
                    best_node = n
            color_black(best_node, G)
        assert(len(whites(G)) == 0)

    def phase2(G):
        logging.debug('')
        c = pieces(G)
        g = greys(G)
        while c > 1 and len(g):
            #print 'pieces=%d, greys=%s' % (c, len(g))
            r = randomly_connect(G)
            #print 'pieces:', c
            if not r:
                logging.warning('randomly_connect failed')
                return False
            c_new = pieces(G)
            assert(c_new < c)
            c = c_new
            g = greys(G)
        return True

    def validate(G):
        if len(whites(G)) > 0:
            logging.error('')
            return False
        for n in greys(G):
            black_neigbors = [neighbor for neighbor in G.neighbors(n) if G.node[neighbor]['color'] == 'black']
            if not len(black_neigbors):
                logging.error('')
                return False
        return True

    cds = list()
    for i in range(0, num):
        G = _G.copy()
        for i, n in enumerate(G.node):
            G.node[n]['color'] = 'white'
            G.node[n]['id'] = i
        phase1(G)
        assert(validate(G))
        r = phase2(G)
        if not r:
            logging.warning('phase2 failed')
            return list()
        cds.append(blacks(G))
        assert(validate(G))
    return cds

def normRSSI(RSSI):
    d = abs(min(RSSI) - max(RSSI))
    m = min(RSSI)+255
    return [(r, min(max((r+255-m)/d, 0.0), 1.0)) for r in RSSI]

def coeffs2polystr(polycoeffs):
    poly = str()
    for i, c in enumerate(polycoeffs):
        degree = len(polycoeffs) - i - 1
        poly += '%.2fx^{%d}+' % (c, degree)
    return poly[0:-1]

def convex_hull(points):
    '''Calculate subset of points that make a convex hull around points

    Recursively eliminates points that lie inside two neighbouring points until only convex hull is remaining.

    Modified version of: http://www.scipy.org/Cookbook/Finding_Convex_Hull

    :Parameters:
        points : ndarray (2 x m)
            array of points for which to find hull

    :Returns:
        hull_points : ndarray (2 x n)
            convex hull surrounding points
    '''
    def _angle_to_point(point, centre):
        '''calculate angle in 2-D between points and x axis'''
        delta = point - centre
        res = numpy.arctan(delta[1] / delta[0])
        if delta[0] < 0:
            res += numpy.pi
        return res

    def area_of_triangle(p1, p2, p3):
        '''calculate area of any triangle given co-ordinates of the corners'''
        return numpy.linalg.norm(numpy.cross((p2 - p1), (p3 - p1)))/2.

    n_pts = points.shape[1]
    assert(n_pts > 5)
    centre = points.mean(1)
    angles = numpy.apply_along_axis(_angle_to_point, 0, points, centre)
    pts_ord = points[:,angles.argsort()]
    pts = [x[0] for x in zip(pts_ord.transpose())]
    prev_pts = len(pts) + 1
    k = 0
    while prev_pts > n_pts:
        prev_pts = n_pts
        n_pts = len(pts)
        i = -2
        while i < (n_pts - 2):
            Aij = area_of_triangle(centre, pts[i],     pts[(i + 1) % n_pts])
            Ajk = area_of_triangle(centre, pts[(i + 1) % n_pts], \
                                   pts[(i + 2) % n_pts])
            Aik = area_of_triangle(centre, pts[i],     pts[(i + 2) % n_pts])
            if Aij + Ajk < Aik:
                del pts[i+1]
            i += 1
            n_pts = len(pts)
        k += 1
    return numpy.asarray(pts)

def chi2_2sample(s1, s2, bins=20, range=(0,1)):
    """
    Chi square two sample test
    see: http://www.itl.nist.gov/div898/software/dataplot/refman1/auxillar/chi2samp.htm

    @return chi-square-statistic, degrees of freedom, p-value
    """

    while True:
        R, _e = numpy.histogram(s1, bins=bins, range=range)
        S, _e = numpy.histogram(s2, bins=bins, range=range)
        if min(R) >= 5 and min(S) >= 5:
            break
        bins -= 1
        logging.debug('decreased bin count to %d for X2 test: less than 5 samples in a bin' % bins)

    def _k1(s, r):
        return math.sqrt(float(numpy.sum(s))/numpy.sum(r))
    def _k2(s, r):
        return math.sqrt(float(numpy.sum(r))/numpy.sum(s))

    x2 = 0
    K1 = _k1(R, S)
    K2 = _k2(R, S)

    for i, r in enumerate(R):
        u = (K2 * R[i] - K2 * S[i])**2
        d = R[i] + S[i]
        x2 += u/d

    df = bins - 1
    pvalue = chi2.pdf(x2, df)
    return x2, df, pvalue

def chi2_2sample_crit(alpha, df):
    """
    @param alpha confidence level
    @param df degrees of freedom
    """
    crit = chi2.ppf(1.0-alpha, df)
    return crit

def data2hist(data, bins=20, range=None):
    """
    Returns x,y coordinates of the data as a histogram.
    The histogram is normalized.
    """
    if not range:
        range = (min(data), max(data))
    Y, X = numpy.histogram(data, bins=bins, range=range)
    Y = Y/float(len(data))
    Y = numpy.hstack((0, Y.repeat(2), 0))
    X = X.repeat(2)
    return X, Y

def data2pdf(data, X=None):
    """
    Determine the probability density with a Gaussian
    kernel estimator
    """
    if X == None:
        X = pylab.linspace(min(data),max(data), 200)
    k = gaussian_kde(data)
    Y = k.evaluate(X)
    return X,Y


def cdf_skew_norm(X=pylab.linspace(0,1,1000), loc=0, scale=1, a=0):
    def dens_skew_norm(X, loc, scale, a):
        Y = 2.0/scale*numpy.exp(-X**2/2)/numpy.sqrt(2*numpy.pi)
        Y *= sp.ndtr(a*X)
        return Y

    X = (X-loc)/scale
    Y = dens_skew_norm(X, loc, scale, a)
    Y = numpy.cumsum(Y)*(X[1]-X[0])*scale
    return Y

def ks_crit_val(len_data1, len_data2, alpha):
    c_a = {
        0.10: 1.22,
        0.05: 1.36,
        0.025:1.48,
        0.01: 1.63,
        0.005:1.73,
        0.001:1.95
    }
    assert(alpha in c_a)
    D_c = c_a[alpha] * math.sqrt(float(len_data1+len_data2)/float(len_data1*len_data2))
    return D_c

def confidence(data, alpha=0.95):
    #crit = (1-alpha)/2.0
    if alpha == 0.95:
        z = 1.96
    elif alpha == 0.90:
        z = 1.645
    else:
        logging.critical('Unsupported alpha = %.4f', alpha)
        assert(1==0)
    mean = scipy.mean(data)
    std = scipy.std(data)
    intv = z * std/scipy.sqrt(len(data))
    left = mean + intv
    right = mean - intv
    return (left, right, intv)

def commits(func):
    """
    Decorator for functions that modify the database and should
    always commit before returning
    """
    def _commits(*args, **kwargs):
        conn = None
        for arg in args:
            if isinstance(arg, dict):
                try:
                    conn = arg['db_conn']
                    if isinstance(conn, sqlite3.Connection):
                        break
                    conn = None
                except KeyError:
                    pass
        if not conn:
            raise Exception, 'No options dictionary with sqlite3.Connection in argument list'
        res = func(*args, **kwargs)
        conn.commit()
        return res
    return _commits

def requires_hellos(func):
    """
    Decorator for plotting functions that require HELLOs
    """
    def _hello(*args, **kwargs):
        for arg in args:
            if isinstance(arg, dict):
                arg['prefix'] = 'all'
                tags = tags_with_hellos(*args)
                if not tags or len(tags) == 0:
                    return
                kwargs['tags'] = tags
                kwargs['cursor'] = arg['db_conn'].cursor()
                return func(*args, **kwargs)
        raise Exception, 'No options dictionary'
    return _hello

def requires_nhdp_hellos(func):
    def _hello(*args, **kwargs):
        for arg in args:
            if isinstance(arg, dict):
                arg['cur_src'] = 'all'
                tags = tags_with_nhdp_hellos(*args)
                if not tags or len(tags) == 0:
                    return
                kwargs['tags'] = tags
                kwargs['cursor'] = arg['db_conn'].cursor()
                return func(*args, **kwargs)
        raise Exception, 'No options dictionary'
    return _hello

def requires_parallel_sources(func):
    """
    Decorator for plotting functions that require HELLOs
    """
    def _test_sources(*args, **kwargs):
        for arg in args:
            if isinstance(arg, dict):
                arg['prefix'] = 'all'
                if not has_tags_with_parallel_sources(*args):
                    return
                kwargs['cursor'] = arg['db_conn'].cursor()
                return func(*args, **kwargs)
        raise Exception, 'No options dictionary'
    return _test_sources

def requires_single_sources(func):
    """
    Decorator for plotting functions that do not allow parallel sources
    """
    def _test_sources(*args, **kwargs):
        for arg in args:
            if isinstance(arg, dict):
                arg['prefix'] = 'all'
                if has_tags_with_parallel_sources(*args):
                    return
                kwargs['cursor'] = arg['db_conn'].cursor()
                return func(*args, **kwargs)
        raise Exception, 'No options dictionary'
    return _test_sources


def hostname_to_addr(hostname, options, cur=None):
    """
    Get MAC address of TAP interface of a node
    """
    if cur:
        cursor = cur
    else:
        cursor = options['db_conn'].cursor()
    src_addr, = cursor.execute('''
        SELECT DISTINCT(src)
        FROM tx
        WHERE host=?
    ''', (hostname,)).fetchone()
    return src_addr

def tags_with_hellos(options, cur=None):
    if cur:
        cursor = cur
    else:
        cursor = options['db_conn'].cursor()
    tags = cursor.execute('''
        SELECT key, id, helloSize, hello_interval
        FROM tag
        WHERE gossip IN (2,5,8,9,11)
        ORDER BY helloSize, hello_interval
    ''').fetchall()
    if len(tags) == 0:
        logging.warning('No tags found for gossip variants that use HELLOs')
        return list()
    return tags

def tags_with_nhdp_hellos(options, cur=None):
    if cur:
        cursor = cur
    else:
        cursor = options['db_conn'].cursor()
    tags = cursor.execute('''
        SELECT key, id, nhdp_hi, nhdp_ht, mpr_minpdr
        FROM tag
        WHERE gossip IN (11)
        ORDER BY helloSize, hello_interval
    ''').fetchall()
    if len(tags) == 0:
        logging.warning('No tags found for gossip variants that use NHDP HELLOs')
        return list()
    return tags

def sources_per_tag(options, cur=None):
    if cur:
        cursor = cur
    else:
        cursor = options['db_conn'].cursor()
    l = cursor.execute('''
        SELECT tag_key, sources
        FROM eval_sources_per_tag
        ORDER BY tag_key
    ''').fetchall()
    return l

def has_tags_with_parallel_sources(options, cur=None):
    data = sources_per_tag(options)
    srcs = set(s for t, s in data)
    if len(srcs) > 1 or max(srcs) > 1:
        return True
    logging.warning('No parallel sources')
    return False

def _get_fig_label():
    counter = getattr(_get_fig_label,"counter", -1)
    if counter < 0:
        _get_fig_label.counter = -1
    _get_fig_label.counter += 1
    return 'figure%d' % (_get_fig_label.counter)

def update_data(options):
    if options['username'] == None:
        logging.warn('No username supplied. Trying to get username from environment.')
        options['username'] = getpass.getuser()

    if options['dbpassword'] == None:
        options['dbpassword'] = getpass.getpass('Password for database access of user \"%s\":' % (options['username']))

    conn_uhu = psycopg2.connect(database='DES-DB', user=options['username'], host=options['dbhost'], password=options['dbpassword'], port=5432)
    cur_uhu = conn_uhu.cursor()
    cur_uhu.execute('''
        SELECT "Node"."name", "Node".xloc, "Node".yloc, "Node".zloc
        FROM public."Node";
    ''')
    nodes = cur_uhu.fetchall()

    if len(nodes) == 0:
        logging.critical('Could not get the nodes\' positions from the database on uhu. Exiting now.')
        sys.exit(61)

    conn_sqlite = options['db_conn']
    cursor_sqlite = conn_sqlite.cursor()
    cursor_sqlite.execute('''DROP TABLE IF EXISTS node''')
    conn_sqlite.commit()
    cursor_sqlite.execute('''
        CREATE TABLE node (
            host TEXT NOT NULL,
            x REAL NOT NULL,
            y REAL NOT NULL,
            z REAL NOT NULL,
            PRIMARY KEY(host)
        )''')
    conn_sqlite.commit()

    for name, x, y, z in nodes:
        hosts = all_routers(options)
        if name in hosts:
            cursor_sqlite.execute('''\
                INSERT INTO node VALUES (
                    ?, ?, ?, ?
                )
            ''', (name, x, y, z))
    conn_sqlite.commit()

def num_hosts(options):
    cursor = options['db_conn'].cursor()
    nhosts = float(cursor.execute('''SELECT COUNT(host) FROM addr''').fetchone()[0])
    return nhosts

def get_hosts(options):
    cursor = options['db_conn'].cursor()
    hosts = list(pylab.flatten(cursor.execute('''SELECT host FROM addr''').fetchall()))
    return hosts

def all_routers(options):
    cursor = options['db_conn'].cursor()
    cursor.execute('''SELECT host FROM addr''')
    return list(pylab.flatten(cursor.fetchall()))

def fu_colormap():
    fu_green = '#99CC00'
    fu_blue = '#003366'
    fu_red = '#CC0000'
    fu_orange = '#FF9900'

    c=[fu_blue, fu_green, fu_orange, fu_red]
    mycm=matplotlib.colors.LinearSegmentedColormap.from_list('mycm',c)
    return mycm

def gray_colormap():
    black = '#202020'
    gray = '#D0D0D0'
    mycm=matplotlib.colors.LinearSegmentedColormap.from_list('mycm', [black, gray])
    return mycm

def calc_average_distance(locs):
    def _distance(t1, t2):
        return math.sqrt((t1[0]-t2[0])**2 + (t1[1]-t2[1])**2 + (t1[2]-t2[2])**2)

    dists_all = list()
    for host1 in locs:
        host1_dists = list()
        t1 = locs[host1]
        for host2 in locs:
            if host1 == host2:
                continue
            t2 = locs[host2]
            host1_dists.append((host1, host2, _distance(t1, t2)))
        dists_all.extend(host1_dists)
    dists_t9 = [(h1, h2, d) for h1, h2, d in dists_all if h1.startswith('t9') and h2.startswith('t9')]
    dists_a3 = [(h1, h2, d) for h1, h2, d in dists_all if h1.startswith('a3') and h2.startswith('a3')]
    dists_a6 = [(h1, h2, d) for h1, h2, d in dists_all if h1.startswith('a6') and h2.startswith('a6')]
    dists_a7 = [(h1, h2, d) for h1, h2, d in dists_all if h1.startswith('a6') and h2.startswith('a6')]

    t9 = [d for h1, h2, d in dists_t9]
    a3 = [d for h1, h2, d in dists_a3]
    a6 = [d for h1, h2, d in dists_a6]
    a7 = [d for h1, h2, d in dists_a6]
    all_buildings = [d for h1, h2, d in dists_all]

    aad = dict()
    aad['all'] = (scipy.mean(all_buildings), scipy.std(all_buildings))
    aad['t9'] = (scipy.mean(t9), scipy.std(t9))
    aad['a3'] = (scipy.mean(a3), scipy.std(a3))
    aad['a6'] = (scipy.mean(a6), scipy.std(a6))
    aad['a7'] = (scipy.mean(a7), scipy.std(a7))

    dists = dict()
    dists['all'] = dists_all
    dists['t9'] = dists_t9
    dists['a3'] = dists_a3
    dists['a6'] = dists_a6
    dists['a7'] = dists_a7

    logging.info('t9, AAD = %.3f, std = %.3f', aad['t9'][0], aad['t9'][1])
    logging.info('a3, AAD = %.3f, std = %.3f', aad['a3'][0], aad['a3'][1])
    logging.info('a6, AAD = %.3f, std = %.3f', aad['a6'][0], aad['a6'][1])
    logging.info('a7, AAD = %.3f, std = %.3f', aad['a7'][0], aad['a7'][1])
    logging.info('all, AAD = %.3f, std = %.3f', aad['all'][0], aad['all'][1])

    return aad, dists

def calc_statistical_average_distance(nodes):
    t9 = list()
    a6 = list()
    a3 = list()
    a7 = list()
    for name,x,y,z in nodes:
        if name.startswith('t9'):
            t9.append((x,y,z))
        if name.startswith('a3'):
            a3.append((x,y,z))
        if name.startswith('a6'):
            a6.append((x,y,z))
        if name.startswith('a7'):
            a7.append((x,y,z))
    all_buildings = t9+a6+a3+a7

    # calc router per cubic meters
    t9_m3 = ((max([x for x,y,z in t9])-min([x for x,y,z in t9]))*(max([y for x,y,z in t9])-min([y for x,y,z in t9]))*(max([z for x,y,z in t9])-min([z for x,y,z in t9])))/len(t9)
    a3_m3 = ((max([x for x,y,z in a3])-min([x for x,y,z in a3]))*(max([y for x,y,z in a3])-min([y for x,y,z in a3]))*(max([z for x,y,z in a3])-min([z for x,y,z in a3])))/len(a3)
    a6_m3 = ((max([x for x,y,z in a6])-min([x for x,y,z in a6]))*(max([y for x,y,z in a6])-min([y for x,y,z in a6]))*(max([z for x,y,z in a6])-min([z for x,y,z in a6])))/len(a6)
    a7_m3 = ((max([x for x,y,z in a7])-min([x for x,y,z in a7]))*(max([y for x,y,z in a7])-min([y for x,y,z in a7]))*(max([z for x,y,z in a7])-min([z for x,y,z in a7])))/len(a7)
    total_m3 = ((max([x for x,y,z in all_buildings])-min([x for x,y,z in all_buildings]))*(max([y for x,y,z in all_buildings])-min([y for x,y,z in all_buildings]))*(max([z for x,y,z in all_buildings])-min([z for x,y,z in all_buildings])))/len(all_buildings)

    # calc router distance based on router per cubic meters
    sad = dict()
    sad['all'] = total_m3**(1/3.0)
    sad['t9'] = t9_m3**(1/3.0)
    sad['a3'] = a3_m3**(1/3.0)
    sad['a6'] = a6_m3**(1/3.0)
    sad['a7'] = a7_m3**(1/3.0)

    logging.info('all, SAD = %.3f', sad['all'])
    logging.info('t9, SAD = %.3f', sad['t9'])
    logging.info('a3, SAD = %.3f', sad['a3'])
    logging.info('a6, SAD = %.3f', sad['a6'])
    logging.info('a7, SAD = %.3f', sad['a7'])

    return sad

def minimum_spanning_distance(options):
    dists = options['dists']
    mad = dict()
    for key in dists.keys():
        d = dists[key]
        G = nx.Graph()
        G.add_nodes_from([h1 for h1, h2, dist in d])
        for h1, h2, d in d:
            G.add_edge(h1, h2, weight=d)
        mst = nx.Graph(nx.minimum_spanning_tree(G))
        msts = [d['weight'] for h1, h2, d in mst.edges(data=True)]
        mad_mean = scipy.mean(msts)
        mad_std = scipy.std(msts)
        mad[key] = (mad_mean, mad_std)
        logging.info('%s, MAD = %.3f, std = %.3f', key, mad_mean, mad_std)
    return mad

def prepare_coordinates(options):
    cursor = options['db_conn'].cursor()
    locs = {}
    nodes = cursor.execute('''
      SELECT host, x, y, z
      FROM node
    ''').fetchall()
    for name,x,y,z in nodes:
        locs[str(name)] = (x, y, z)
    options['locs'] = locs

    if options['statistics']:
        statistics = dict()
        statistics['sad'] = calc_statistical_average_distance(nodes)
        aad, dists = calc_average_distance(locs)
        options['dists'] = dists
        statistics['aad'] = aad
        statistics['mad'] = minimum_spanning_distance(options)
        options['statistics'] = statistics

    dims = {}
    for bid in ['t9', 'a3', 'a6', 'a7']:
        cursor.execute('''
          SELECT MAX(x), MIN(x), MAX(y), MIN(y) \
          FROM node  \
          WHERE host LIKE ? \
        ''', (bid+'%',))
        xmax, xmin, ymax, ymin = cursor.fetchone()

        cursor.execute('''
          SELECT AVG(z) \
          FROM node  \
          WHERE host LIKE ? \
        ''', (bid+'-k'+'%',))
        z_sub, = cursor.fetchone()
        #print bid, z_sub

        cursor.execute('''
          SELECT AVG(z) \
          FROM node  \
          WHERE host LIKE ? \
        ''', (bid+'-0'+'%',))
        z_1st, = cursor.fetchone()
        #print bid, z_1st

        cursor.execute('''
          SELECT AVG(z) \
          FROM node  \
          WHERE host LIKE ? \
        ''', (bid+'-1'+'%',))
        z_2nd, = cursor.fetchone()
        #print bid, z_2nd

        cursor.execute('''
          SELECT AVG(z) \
          FROM node  \
          WHERE host LIKE ? \
        ''', (bid+'-2'+'%',))
        z_3rd, = cursor.fetchone()
        #print bid, z_3rd

        z_4th = None
        if z_3rd:
            z_4th = z_3rd + (z_3rd - z_2nd)
        elif z_2nd:
            z_3rd = z_2nd + (z_2nd - z_1st)

        zs = [z for z in [z_sub, z_1st, z_2nd, z_3rd, z_4th] if z]
        if not len(zs):
            continue

        lines = []
        for z in zs:
            lines.append([[xmin, xmax, xmax, xmin, xmin], [ymin, ymin, ymax, ymax, ymin], [z]*5])
        dims[bid] = lines
        dims[bid].extend([
          [[xmin, xmin], [ymin, ymin], [min(zs), max(zs)]],
          [[xmax, xmax], [ymin, ymin], [min(zs), max(zs)]],
          [[xmin, xmin], [ymax, ymax], [min(zs), max(zs)]],
          [[xmax, xmax], [ymax, ymax], [min(zs), max(zs)]]
        ])
    logging.info('determined building/deployment dimensions')
    options['dims'] = dims

def _cname_to_latex(s):
    if s == 'helloInterval':
        return '$hello_{interval}$'
    elif s == 'helloTTL':
        return r'Hello$_{\text{TTL}}$'
    elif s == 'helloSize':
        return r'Hello$_{\text{size}}$'
    elif s == 'pkg_interval':
        return r'Packet$_{\text{interval}}$'
    elif s == 'cleanup_interval':
        return '$cleanup_{interval}$'
    elif s == 'p_min':
        return '$p_{min}$'
    elif s == 'p_max':
        return '$p_{max}$'
    elif s == 'T_MAX':
      return '$T_{max}$'
    elif s == 'pkg_size':
      return r'Packet$_{\text{size}}$'
    elif s == 'key':
        return 'Scenario'
    elif s == 'gossip':
        return 'Gossip Variant(-s)'
    elif s == 'channel':
        return 'Channel(-s)'
    return '$%s$' % s

def write_scenarios(options):
    newline = '\\\\\n'
    cursor = options['db_conn'].cursor()
    skip = [1,2]
    lf = options['latex_file']
    parameters = cursor.execute('''
        PRAGMA TABLE_INFO(tag)
    ''').fetchall()
    lf.write('\\section{Experiment Scenarios}\n')
    #lf.write('\\begin{landscape}\n')
    #parameters = [name for _num, name, _type, _a, _b, _c in parameters]
    #lf.write('\t\\begin{longtable}{')
    #first = True
    #for i, _p in enumerate(parameters):
        #if i in skip:
            #continue
        #if not first:
            #lf.write(' | ')
        #lf.write('l')
        #first = False
    #lf.write('}\n\t\\toprule\n')
    #lf.write('\t')
    #first = True
    #for i, p in enumerate(parameters):
        #if i in skip:
            #continue
        #if not first:
            #lf.write(' & ')
        #lf.write(_cname_to_latex(p))
        #first = False
    #lf.write(newline)
    #lf.write('\t\\midrule\\midrule\n\t\\endhead\n')

    #lines = cursor.execute(''' \
        #SELECT DISTINCT * \
        #FROM tag \
    #''').fetchall()

    #for line in lines:
        #lf.write('\t')
        #first = True
        #for i, v in enumerate(line):
            #if i in skip:
                #continue
            #if not first:
                #lf.write(' & ')
            #lf.write(str(v))
            #first = False
        #lf.write(newline)
        #lf.write('\t\\midrule\n')

    #lf.write('\\caption{Parameters of the Experiment}\n')
    #lf.write('\\end{longtable}\n')
    #lf.write('\\end{landscape}\n')

    values = options['configurations']
    lf.write('\\begin{table}\n\\centering\n')
    lf.write('    \\begin{tabular}{l|l}\n    \\toprule\n')
    lf.write('    Parameter & Values %s    \\midrule\\midrule\n' % (newline))

    parallel_sources = set([x[0] for x in cursor.execute('''
        SELECT DISTINCT(A)
        FROM (
            SELECT COUNT(DISTINCT(host)) AS A
            FROM tx GROUP BY tag ORDER BY A
        )
        ''').fetchall()])

    tx_pkgs_max, tx_pkgs_min = cursor.execute('''
        SELECT MAX(seq), MIN(seq)
        FROM (
            SELECT COUNT(seq) AS seq
            FROM tx
            GROUP BY tag, src
        )
    ''').fetchone()

    pkg_sizes = set([x[0] for x in cursor.execute('''
        SELECT distinct(pkg_size)
        FROM tag
        ORDER BY pkg_size
    ''').fetchall()])

    gossip = set([x[0] for x in cursor.execute('''
        SELECT distinct(gossip)
        FROM tag
        ORDER BY gossip
    ''').fetchall()])

    channels = set([x[0] for x in cursor.execute('''
        SELECT distinct(channel)
        FROM tag
        ORDER BY channel
    ''').fetchall()])

    ### channel
    lf.write('    %s & %s %s    \midrule\n' % (_cname_to_latex('gossip'), ', '.join(str(s) for s in sorted(gossip)), newline))
    ### number of sources or name of sources
    if len(parallel_sources) == 1:
        lf.write('    Sources & %s %s    \midrule\n' % (', '.join(s for s in options['src']), newline))
    else:
        lf.write('    Simultaneous Sources & %s %s    \midrule\n' % (', '.join(str(s) for s in sorted(parallel_sources)), newline))
    if not tx_pkgs_max:
        tx_pkgs_max = 0
    tx_pkgs = '%d' % (tx_pkgs_max)
    ### number of packets
    lf.write('    Packets & %s %s    \midrule\n' % (tx_pkgs, newline))
    ### packet size
    lf.write('    %s & %s %s    \midrule\n' % (_cname_to_latex('pkg_size'), ', '.join(str(s) for s in sorted(pkg_sizes)), newline))
    ### channel
    lf.write('    %s & %s %s    \midrule\n' % (_cname_to_latex('channel'), ', '.join(str(s) for s in sorted(channels)), newline))
    ### all other parameters
    for key in sorted(values.keys()):
        if len(values[key]) > 1:
            lf.write('    %s & %s %s    \midrule\n' % (_cname_to_latex(key), ', '.join(str(s) for s in sorted(values[key])), newline))
    lf.write('    \\end{tabular}\n')
    lf.write('\\caption{Parameters of the Experiment}\n')
    lf.write('\\end{table}\n')

    lf.write('\\section{Evaluation}\n')

def open_latex_file(options):
    if not len(options['outdir']):
        logging.debug('outdir not set using ./%s_results' % (options['db']))
        outdir = options['db']+'_results'
    else:
        outdir = options['outdir']

    try:
        os.mkdir('%s' % (outdir))
    except OSError:
        pass

    outfile_name = '%s/report.tex' %(outdir)
    outfile = None
    try:
        outfile = open(outfile_name, 'w')
    except IOError, msg:
        logging.critical('ERROR: Unable to open output file %s for writing' % (outfile_name))
        sys.exit(1)
    options['latex_file'] = outfile
    logging.info('opened report file: %s', outfile_name)

def legend(*args, **kwargs):
    """
    Overwrites the pylab legend function.

    It adds another location identifier 'outer right'
    which locates the legend on the right side of the plot

    The args and kwargs are forwarded to the pylab legend function
    """
    if kwargs.has_key('loc'):
        loc = kwargs['loc']
        if (loc == 'outer'):
            global new
            kwargs.pop('loc')
            leg = plt.legend(loc=(0,0), *args, **kwargs)
            frame = leg.get_frame()
            currentAxes = plt.gca()
            barray = currentAxes.get_position().get_points()
            currentAxesPos = [barray[0][0], barray[0][1], barray[1][0], barray[1][1]]
            currentAxes.set_position([currentAxesPos[0]-0.02, currentAxesPos[1], currentAxesPos[2] - 0.2, currentAxesPos[3]-currentAxesPos[1]])
            version = mpl.__version__.split(".")
            #if map(int, version) < [0, 98]:
            #   leg._loc = (1 + leg.axespad, 0.0)
            #else:
            leg._loc = (1.03, -0.05) # + leg.borderaxespad, 0.0)
            plt.draw_if_interactive()
            return leg
    return plt.legend(*args, **kwargs)

def drawBuildingContours(ax, options):
    units2meter = options['units2meter']
    for dim_key in options['dims'].keys():
        dim = options['dims'][dim_key]
        for line in dim:
            ax.plot(
                numpy.array(line[0])*units2meter,
                numpy.array(line[1])*units2meter,
                numpy.array(line[2])*units2meter,
                linestyle='-', color='gray', alpha=0.5, linewidth=0.75)

def prepare_outdir(options):
    if not options['outdir']:
            logging.info('outdir not set using ./%s_results' % (options['db']))
            options['outdir'] = options['db']+'_results'
    try:
        os.mkdir('%s' % (options['outdir']))
    except OSError:
        pass

def data2fig(data, X, options, legend_title, xlabel, ylabel=r'Reachability~$\reachability$'):
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(data.keys())))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(data.keys())))
    fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=ylabel, grid=False, aspect='auto', legend=True)
    for j, nhdp_ht in enumerate(sorted(data.keys())):
        d = data[nhdp_ht]
        try:
            mean_y = [scipy.mean(d[n]) for n in X]
        except KeyError:
            logging.warning('key \"%s\" not found, continuing...', nhdp_ht)
            continue
        confs_y = [confidence(d[n])[2] for n in X]
        poly = [conf2poly(X, list(numpy.array(mean_y)+numpy.array(confs_y)), list(numpy.array(mean_y)-numpy.array(confs_y)), color=colors[j])]
        patch_collection = PatchCollection(poly, match_original=True)
        patch_collection.set_alpha(0.3)
        patch_collection.set_linestyle('dashed')
        fig.ax.add_collection(patch_collection)
        fig.ax.plot(X, mean_y, label='$%d$' % nhdp_ht, color=colors[j])
    fig.ax.set_xticks(X)
    fig.ax.set_xticklabels(['$%s$' % i for i in X])
    fig.ax.set_ylim(0,1)
    fig.legend_title = legend_title
    return fig

def conf2poly(xs, ucl, lcl, color='red'):
    xs = list(xs) + list(xs)[::-1]
    poly = zip(xs, (ucl + lcl[::-1]))
    return Polygon(poly, edgecolor='black', facecolor=color, closed=True)

class MyFig():
    def __init__(self, options, figsize=(10,10), xlabel='', ylabel='', zlabel='', legend=False, grid=False, aspect='equal', twinx=False, rect=[.1,.1,.8,.8], ThreeD=False, legend_pos='best', legend_title=None):
        self.fig    = pylab.figure(figsize=figsize)
        if ThreeD:
            self.ax = Axes3D(self.fig, rect=rect, aspect=aspect)
            self.ax.set_zlabel(zlabel, fontsize=options['fontsize'])
        else:
            self.ax = self.fig.add_axes(rect, aspect=aspect)
        self.ThreeD = ThreeD
        self.options= options
        self.legend = legend
        if not ThreeD:
            self.ax.set_xlabel(xlabel, fontsize=options['fontsize'])
            self.ax.set_ylabel(ylabel, fontsize=options['fontsize'])
        else:
            self.ax.set_xlabel(xlabel, fontsize=options['fontsize']*0.8)
            self.ax.set_ylabel(ylabel, fontsize=options['fontsize']*0.8)
            self.ax.set_zlabel(zlabel, fontsize=options['fontsize']*0.8)
        if grid:
            self.ax.grid()
        for ticklabel in self.ax.get_xticklabels():
            ticklabel.set_fontsize(options['fontsize']*0.8)
        for ticklabel in self.ax.get_yticklabels():
            ticklabel.set_fontsize(options['fontsize']*0.8)
        if ThreeD:
            for wax in [self.ax.w_xaxis, self.ax.w_yaxis, self.ax.w_zaxis]:
                for ticklabel in wax.get_ticklabels():
                    ticklabel.set_fontsize(options['fontsize']*0.7)
        if twinx:
            self.ax_2nd = self.ax.twinx()
            for ticklabel in self.ax_2nd.get_yticklabels():
                ticklabel.set_fontsize(options['fontsize']*0.8)
        self.legend_title = legend_title
        self.legend_pos = legend_pos

    def save(self, fileName, close=True, fileformat='pdf', legend_cols=1, grid='y'):
        if self.legend:
            legend = self.ax.get_legend()
            if not legend:
                self.ax.legend(loc=self.legend_pos, fancybox=False, shadow=True, labelspacing=0, ncol=legend_cols)
                legend = self.ax.get_legend()
            if legend:
                if self.legend_title:
                    legend.set_title(self.legend_title)
                    legend.get_title().set_fontsize(self.options['fontsize']*0.8)
                for t in legend.get_texts():
                    t.set_fontsize(self.options['fontsize']*0.8)
        self.ax.set_title(self.ax.get_title(), fontsize=self.options['fontsize'])

        if grid == 'y':
            self.ax.xaxis.grid(False)
        fileName = '%s-%s' % (self.options['prefix'], fileName)
        fileName = fileName.replace(' ', '_')
        fileName = fileName.replace('=', '_')
        outdir = self.options['outdir']
        try:
            outfile = open(('%s/%s.%s' %(outdir, fileName, fileformat)), 'w')
        except IOError, msg:
            logging.critical('ERROR: Unable to open output file %s/%s.pdf for writing' % (outdir, fileName))
            sys.exit(1)
        self.fig.savefig(outfile, format=fileformat, bbox_inches='tight')
        if close:
            pylab.close(self.fig)

        return fileName+'.pdf', _get_fig_label()

    def colorbar(self, ax):
        self.cbar = self.fig.colorbar(ax, shrink=0.8, aspect=20)
        for t in self.cbar.ax.get_yticklabels():
            t.set_fontsize(0.8*self.options['fontsize'])
        for t in self.cbar.ax.get_xticklabels():
            t.set_fontsize(0.8*self.options['fontsize'])
        return self.cbar

def eval_scenarios(options):
    """
    Returns information in a dictionary that is important for some plotting functions.
    All tags in the tag table are evaluated.
    """
    cursor = options['db_conn'].cursor()
    parameters = cursor.execute('''
        PRAGMA TABLE_INFO(view_configurations)
    ''').fetchall()
    parameters = [name for _num, name, _type, _a, _b, _c in parameters]
    values = {}
    for param in parameters:
        values[param] = set()

    configurations = cursor.execute('''
        SELECT DISTINCT * \
        FROM view_configurations
    ''').fetchall()

    for config in configurations:
        for i, value in enumerate(config):
            #print (i, value)
            values[parameters[i]].add(value)

    options['configurations'] = values

    logging.info('%d different configurations found in tags table', len(configurations))
    logging.info('Values of the parameters:')
    for key in sorted(values.keys()):
        if len(values[key]) > 1:
            try:
                if key in ['p', 'pkg_interval']:
                    logging.info('\t%s:\t\t[%s]', key, ', '.join('%.2f' % v for v in sorted(values[key])))
                else:
                    logging.info('\t%s:\t\t%s', key, sorted([v for v in values[key]]))
            except TypeError:
                print key
                continue

def get_options():
    options = {}
    options['units2meter'] = 1
    options['color'] = fu_colormap() # cm.RdYlBu #
    options['color2'] = cm.autumn
    options['graycm'] = gray_colormap()
    options['markers'] = ['o', 'v', 'x', 'h', '<', '*', '>', 'D', '+', '|']*3
    options['linestyles'] = ['-', '--', '-.', ':']*30
    options['fontsize'] = matplotlib.rcParams['font.size']
    return options

def gauss_kern(size, sizey=None):
    """ Returns a normalized 2D gauss kernel array for convolutions """
    size = int(size)
    if not sizey:
        sizey = size
    else:
        sizey = int(sizey)
    x, y = numpy.mgrid[-size:size+1, -sizey:sizey+1]
    g = scipy.exp(-(x**2/float(size)+y**2/float(sizey)))
    return g / g.sum()

def blur_image(im, n, ny=None) :
    """ blurs the image by convolving with a gaussian kernel of typical
        size n. The optional keyword argument ny allows for a different
        size in the y direction.
    """
    g = gauss_kern(n, sizey=ny)
    improc = scipy.signal.convolve(im,g, mode='valid')
    return(improc)