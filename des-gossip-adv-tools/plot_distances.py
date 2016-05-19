#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
from pygraph.classes.digraph import digraph
from pygraph.algorithms import minmax
import matplotlib.cm as cm
import pylab
import scipy, numpy
from helpers import MyFig
from copy import deepcopy
from scipy import stats

#def pathlen_main():
    #'''
    #Main function of the path length / distance plotter
    #'''
    #parser = argparse.ArgumentParser(description='Parse log files and store data in sqlite database')
    #parser.add_argument('--db', default='./sqlite.db', help='name of the database where the parsed data shall be stored')
    #parser.add_argument('--outdir', default='', help='output directory for the plots')
    #parser.add_argument('--fontsize', default=20, type=int, help='base font size of plots')
    #parser.add_argument('--bins', default=10, type=int, help='granularity of the 3d box plot; bin size')
    #args = parser.parse_args()

    #options = {}
    #options['log_level'] = logging.INFO
    #options['db'] = args.db
    #options['outdir'] = args.outdir
    #options['fontsize'] = args.fontsize
    #options['bins'] = args.bins

    #logging.basicConfig(level=options['log_level'], format='[%(funcName)s] %(message)s')

    #logging.info('connecting to database')
    #options['db_conn'] = sqlite3.connect(options["db"])
    #plot_pathlen(options)

def plot_pathlen(options):
    '''
    Plot the distances from each node to each other node
    '''
    cursor = options['db_conn'].cursor()
    options['prefix'] = 'all'

    steps = 1.0/options['bins']
    quantiles = numpy.arange(0, 1.0+steps*0.9, steps)

    fig_avr_len = MyFig(options, grid=True, legend=True, xlabel='Hops', ylabel='CDF of Average Distances', aspect='auto')
    fig_max_len = MyFig(options, grid=True, legend=True, xlabel='Hops', ylabel='CDF of Maximum Distances', aspect='auto')
    fig_max_avr_len_meds = MyFig(options, grid=True, xlabel='Maximum Distance', ylabel='Median of Average Distance')

    cursor.execute('''SELECT host FROM addr''')
    hosts = cursor.fetchall()
    vertices = digraph()
    for host, in hosts:
        vertices.add_node(host)

    tag_ids = cursor.execute('SELECT DISTINCT(id), helloSize FROM tag ORDER BY helloSize').fetchall()
    colors = cm.hsv(pylab.linspace(0, 0.8, len(tag_ids)))
    for i, (tag_id, hello_size) in enumerate(tag_ids):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, i+1, len(tag_ids))

        fig_max_avr_len = MyFig(options, grid=True, xlabel='Maximum Distance', ylabel='Average Distance')

        links = cursor.execute('SELECT src,host,pdr FROM eval_helloPDR WHERE tag_id=?''', (tag_id,)).fetchall()
        lens = []
        maxl = []
        max_avr_len = []
        graph = deepcopy(vertices)
        for src, host, pdr in links:
            graph.add_edge((src, host), wt=1)
        for host, in hosts:
            stp, distances = minmax.shortest_path(graph, host)
            if len(distances) > 1:
                vals = [t for t in distances.values() if t>0]
                lens.append(scipy.average(vals))
                maxl.append(max(vals))
                max_avr_len.append((maxl[-1], lens[-1]))
        avr_values = stats.mstats.mquantiles(lens, prob=quantiles)
        max_values = stats.mstats.mquantiles(maxl, prob=quantiles)
        fig_avr_len.ax.plot(avr_values, quantiles, '-', color=colors[i], label=str(hello_size)+' bytes')
        fig_max_len.ax.plot(max_values, quantiles, '-', color=colors[i], label=str(hello_size)+' bytes')

        points = []
        for max_avr_dist in sorted(set([x[0] for x in max_avr_len])):
            median = scipy.median([y[1] for y in max_avr_len if y[0]==max_avr_dist])
            fig_max_avr_len.ax.plot(max_avr_dist, median, 'o', color='black', label='')
            points.append((max_avr_dist, median))
        fig_max_avr_len_meds.ax.plot([x[0] for x in points], [y[1] for y in points], color=colors[i], label=str(hello_size)+' bytes')

        fig_max_avr_len.ax.scatter([x[0] for x in max_avr_len], [y[1] for y in max_avr_len], color=colors[i], label='')
        fig_max_avr_len.ax.axis((0, max([x[0] for x in max_avr_len])+1, 0, max([y[1] for y in max_avr_len])+1))
        fig_max_avr_len.save('dist-average_over_max_%d' % (hello_size))

    for _ax in [fig_avr_len.ax, fig_max_len.ax]:
        _ax.set_yticks(numpy.arange(0.1, 1.1, 0.1))

    fig_avr_len.save('dist-cdf_avr')
    fig_max_len.save('dist-cdf_max')
    fig_max_avr_len_meds.save('dist-median_of_averages_over_max')

def plot_distance(options):
    for j, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, j+1, len(options['src']))
        options['prefix'] = src
        fontsize = options['fontsize']
        cursor = options['db_conn'].cursor()
        cursor.execute('''SELECT host FROM addr WHERE NOT host=? ORDER BY host''', (src,))
        data = []
        max_dist = 0
        for host, in cursor.fetchall():
            dist = cursor.execute('''SELECT dist FROM eval_distance WHERE host=? and src=? ORDER BY dist''', (host, src)).fetchall()
            if len(dist) == 0:
                continue
            dist = [x[0] for x in dist]
            data.append((host, dist, 0.0))
            max_dist = max(max_dist, max(dist))

        for i, d in enumerate(data):
            hist, bin_edges = numpy.histogram(d[1], bins=max_dist, range=(0, max_dist), normed=False)
            hist = [float(x)/sum(hist) for x in hist]
            data[i] = (d[0], d[1], hist)

        if len(data) == 0:
            continue

        fig1 = MyFig(options, xlabel='Distance from %s [Hops]' % src, ylabel='Host Names', figsize=(19, 30), aspect='auto')
        fig2 = MyFig(options, xlabel='Distance from %s [Hops]' % src, ylabel='Host Names', figsize=(19, 40), aspect='auto', grid=True)

        bp = fig1.ax.boxplot([x[1] for x in data], 1, 'ro', 0)
        plt.setp(bp['fliers'], markersize=0.5)

        circ_max = 40
        spacing = 40
        for i, d in enumerate([x[2] for x in data]):
            for j, e in enumerate(d):
                if e == 0:
                    continue
                fig2.ax.plot(j, i*spacing+spacing, 'o', color='red', ms=max(float(e)*circ_max,1))
        fig1.ax.set_yticklabels([x[0] for x in data], rotation=1, size=fontsize*0.8)

        fig2.ax.set_yticks(range(0,len(data)*spacing+1,spacing))
        fig2.ax.set_yticklabels(['']+[x[0] for x in data], rotation=1 , size=fontsize*0.8)

        fig1.save('distance-boxplot')
        fig2.save('distance-circleplot')

if __name__ == "__main__":
    pathlen_main()