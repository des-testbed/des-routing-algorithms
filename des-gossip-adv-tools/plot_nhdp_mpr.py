#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
import matplotlib.cm as cm
import numpy, math
import pylab
from helpers import *
import scipy
from scipy import stats, polyfit, polyval
import networkx as nx
import re
import unicodedata
import scipy.stats as stats
from scipy.stats import kde
from matplotlib.patches import Ellipse
from matplotlib.collections import PatchCollection
import numpy as np
from mpl_toolkits.axes_grid1.inset_locator import zoomed_inset_axes
from mpl_toolkits.axes_grid1.inset_locator import mark_inset

@requires_nhdp_hellos
def plot_mpr_pdr(options, tags=None, cursor=None):
    """ This function plots the average pdr for all links that have been made between
    mprs and their selectors in comparison to the average mpr achieved by all links."""

    options['prefix'] = "mpr"
    if options['grayscale']:
        colors = options['graycm'](range(1, 10, 1))
    else:
        colors = options['color'](range(1, 10, 1))
    fig_1 = MyFig(options, xlabel='MPR vs NOMPR',
                   ylabel='Fraction of MPRs', legend=True, aspect='auto',
                   grid=False)

    for i, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        logging.info("[%d/%d] nhdp_hi: %d nhdp_ht: %d minpdr: %f" % (i+1, len(tags), nhdp_hi, nhdp_ht, mpr_minpdr))
        # only links from a mpr selector (SRC in pdr table) to a mpr (HOST in pdr table) are interesting!!!
        results = []
        pdr_values = cursor.execute('''
            SELECT distinct(pdr)
            FROM eval_helloPDR AS pdr
            JOIN nhdp_mpr_selectors AS mpr
            ON pdr.tag_key = mpr.tag_key AND pdr.host = mpr.host AND pdr.tx_if = mpr.mprselector
            WHERE pdr.tag_key = ?
            ''', (tag_key, )).fetchall()
        pdr_list = list(pylab.flatten(pdr_values))
        results.append(pdr_list)
        total_pdr = cursor.execute('''
                SELECT pdr
                FROM eval_helloPDR
                WHERE tag_key = ?
                ''', (tag_key, )).fetchall()
        results.append(total_pdr)

        fig_hist = MyFig(options, figsize=(10, 8), xlabel=r'pdr dist hi:%d ht: %d minpdr: %.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr), ylabel=r'Fraction', legend=True, grid=True, aspect='auto')
        X, Y = data2hist(pdr_list, bins=5)
        fig_hist.ax.plot(X, Y, color=colors[i])
        fig_hist.save('pdr_hist_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))
        fig = MyFig(options, figsize=(10, 8), xlabel=r'hi:%d ht: %d minpdr: %.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr), ylabel=r'PDR', legend=True, grid=True, aspect='auto')
        fig.ax.boxplot(results, notch=1)
        fig.ax.set_ylim(0,1)
        fig.ax.set_xticklabels(['MPR links','ALL links'])
        fig.ax.set_title('PDR of links between MPR and SELECTOR')
        fig.save('pdr_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))

@requires_nhdp_hellos
def plot_mpr_topology_changes(options, tags=None, cursor=None):
    """
    Plots the relative amount the mpr set changes in a certain interval of time
    for a certain value of nhdp_ht (H_HOLD_TIME/REFRESH_INTERVAL).
    """

    def get_samples_per_repetition():
        max_samples = list()
        logging.info("Calculating max. number of samples possible for %d replications..." % (repetitions))
        for i, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
            logging.info('\t%d' % i)
            max_sample, = cursor.execute('''
                SELECT max(sample)
                FROM eval_mpr_topology_changes
                WHERE tag_key = ?
            ''', (tag_key,)).fetchone()
            max_samples.append(max_sample)
        num_samples = min(max_samples)
        samples_per_repetition = num_samples / repetitions
        logging.info("Samples per replication: %d" % samples_per_repetition)
        return samples_per_repetition

    def get_data():
        logging.info('\t getting data...')
        data = dict()
        cur_sample = 0
        for repetition in range(0, repetitions):
            samples = dict()
            for sample in range(0, samples_per_repetition):
                result, = cursor.execute('''
                    SELECT SUM(abs_changes)
                    FROM eval_mpr_topology_changes
                    WHERE tag_key = ? AND sample = ?
                ''', (tag_key, cur_sample)).fetchone()
                samples[sample] = result
                cur_sample += 1
            data[repetition] = samples.copy()
        return data

    def filter_tags(tags):
        hi_list = [1000, 2000]
        ht_list = [2000, 4000, 6000, 8000]
        pdr_list = [0, 0.2, 0.4, 0.6, 0.8]
        tags_filtered = [(tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) for tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr in tags if nhdp_hi in hi_list and nhdp_ht in ht_list and mpr_minpdr in pdr_list]
        return tags_filtered

    def _plot_hi(nhdp_hi, pdr=0):
        fig_single = MyFig(options, figsize=(10, 8), xlabel='Time [s]', ylabel='Link Changes $\phi$', legend=True, aspect='auto', grid=False, legend_title='$t_h [ms]$')
        _graph_data = [(hi, ht, minpdr, mean_y, confs) for hi, ht, minpdr, mean_y, confs in graph_data if hi==nhdp_hi and minpdr==pdr]
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(_graph_data)))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(_graph_data)))
        for i, (hi, ht, minpdr, mean_y, confs) in enumerate(_graph_data):
            fig_single.ax.plot(range(0, samples_per_repetition + 1), mean_y, label='%d' % ht, color=colors[i], marker='')
            poly = [conf2poly(np.arange(0, samples_per_repetition + 1, 1), list(numpy.array(mean_y)+numpy.array(confs)), list(numpy.array(mean_y)-numpy.array(confs)), color=colors[i])]
            patch_collection = PatchCollection(poly, match_original=True)
            patch_collection.set_alpha(0.3)
            patch_collection.set_linestyle('dashed')
            fig_single.ax.add_collection(patch_collection)
        fig_single.ax.set_xticks(range(0, 61, 10))
        fig_single.ax.set_xticklabels([i*2 for i in range(0, 61, 10)])
        fig_single.ax.set_xlim(0, samples_per_repetition + 1)
        fig_single.ax.set_xlim(0, 23)
        fig_single.ax.set_ylim(0, max_y)
        fig_single.save('topology_changes_hi_%d_minmpr_%.2f' % (nhdp_hi, minpdr))

    def _plot_pdr(nhdp_hi, nhdp_ht):
        fig_single = MyFig(options, figsize=(10, 8), xlabel='Time [s]', ylabel='Link Changes $\phi$', legend=True, aspect='auto', grid=False, legend_title='$t$')
        _graph_data = [(hi, ht, minpdr, mean_y, confs) for hi, ht, minpdr, mean_y, confs in graph_data if hi==nhdp_hi and ht==nhdp_ht]
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(_graph_data)))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(_graph_data)))
        for i, (hi, ht, minpdr, mean_y, confs) in enumerate(_graph_data):
            fig_single.ax.plot(range(0, samples_per_repetition + 1), mean_y, label='%.2f' % minpdr, color=colors[i], marker='')
            poly = [conf2poly(np.arange(0, samples_per_repetition + 1, 1), list(numpy.array(mean_y)+numpy.array(confs)), list(numpy.array(mean_y)-numpy.array(confs)), color=colors[i])]
            patch_collection = PatchCollection(poly, match_original=True)
            patch_collection.set_alpha(0.3)
            patch_collection.set_linestyle('dashed')
            fig_single.ax.add_collection(patch_collection)
        fig_single.ax.set_xticks(range(0, 61, 10))
        fig_single.ax.set_xticklabels([i*2 for i in range(0, 61, 10)])
        fig_single.ax.set_xlim(0, samples_per_repetition + 1)
        fig_single.ax.set_xlim(0, 23)
        fig_single.ax.set_ylim(0, max_y)
        fig_single.save('topology_changes_hi_%d_ht_%d' % (nhdp_hi, nhdp_ht))

    options['prefix'] = "mpr"
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, 10))
    else:
        colors = options['color'](pylab.linspace(0, 1, 10))
    num_hosts = len(get_hosts(options))

    repetitions = options['repetitions']
    max_y = 2
    samples_per_repetition = get_samples_per_repetition()
    tags = filter_tags(tags)

    graph_data = list()
    for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        logging.info('tag_id=\"%s\" nhdp_hi=%d nhdp_ht=%d  (%d/%d)', tag_id, nhdp_hi, nhdp_ht, q+1, len(tags))
        data = get_data()
        mean_y = [0]
        confs = [0]
        for sample in range(0, samples_per_repetition):
            value_list_per_host = list()
            for repetition in range(0, repetitions):
                value_list_per_host.append(float(data[repetition][sample]) / float(num_hosts))
            mean_y.append(scipy.mean(value_list_per_host))
            confs.append(confidence(value_list_per_host)[2])
        graph_data.append((nhdp_hi, nhdp_ht, mpr_minpdr, mean_y, confs))

    for nhdp_hi in set([hi for hi, ht, minpdr, mean_y, confs in graph_data]):
        for pdr in set([minpdr for hi, ht, minpdr, mean_y, confs in graph_data]):
            _plot_hi(nhdp_hi, pdr)

    for nhdp_hi in set([hi for hi, ht, minpdr, mean_y, confs in graph_data]):
        for nhdp_ht in set([ht for hi, ht, minpdr, mean_y, confs in graph_data]):
            _plot_pdr(nhdp_hi, nhdp_ht)

@requires_nhdp_hellos
def plot_mpr_fracs(options, tags=None, cursor=None):
    """
    Plot the fraction of nodes selected as mprs per neighbor.
    """
    options['prefix'] = "mpr"
    max_n1, = cursor.execute('''
                            SELECT max(num_n1)
                            FROM eval_mpr_n1_fracs
                            ''').fetchone()
    max_n2, = cursor.execute('''
                             SELECT max(num_n2)
                             FROM eval_mpr_n2_fracs
                             ''').fetchone()
    samples = cursor.execute('''
                            SELECT DISTINCT(sample)
                            FROM eval_mpr_n1_fracs
                            ''').fetchall()
    n1_x = range(1, max_n1 + 1, 1)
    n2_x = range(1, max_n2 + 1, 1)
    if options['grayscale']:
        colors = options['graycm'](range(1, max_n2 + 1, 1))
    else:
        colors = options['color'](range(1, max_n2 + 1, 1))
    markers = options['markers']
    linestyles = options['linestyles']
    values = []
    fig_n1 = MyFig(options, xlabel='Number of neighbors',
                   ylabel='Fraction of MPRs', legend=True, aspect='auto',
                   grid=False)
    fig_n2 = MyFig(options, xlabel='Number of 2-hop neighbors',
                   ylabel='Fraction of MPRs', legend=True, aspect='auto',
                   grid=False)

    logging.info("samples: %d 1-hop neighbors: %d 2-hop neighbors: %d" % (len(samples), len(n1_x), len(n2_x)))
    for (sample,) in samples:
        for i, num_n1 in enumerate(n1_x):
            result = cursor.execute('''
                                    SELECT mean_mprs, num_values, std
                                    FROM eval_mpr_n1_fracs
                                    WHERE sample = ? AND num_n1 = ?
                                    ''', (sample, num_n1)).fetchone()
            if result == None:
                continue
            c = 1.96 * result[2] / scipy.sqrt(result[1])
            frac = result[0] / float(num_n1)
            if c <= frac:
                fig_n1.ax.errorbar(float(num_n1), frac, yerr=c, color=colors[i])
            fig_n1.ax.bar(num_n1, frac, align='center')

        for i, num_n2 in enumerate(n2_x):
            result = cursor.execute('''
                                    SELECT mean_mprs, num_values, std
                                    FROM eval_mpr_n2_fracs
                                    WHERE sample = ? AND num_n2 = ?
                                    ''', (sample, num_n2)).fetchone()
            if result == None:
                continue
            c = 1.96 * result[2] / scipy.sqrt(result[1])
            frac = result[0] / float(num_n2)
            if c <= frac:
                fig_n2.ax.errorbar(float(num_n2), frac, yerr=c, color=colors[i])
            fig_n2.ax.bar(num_n2, frac, align='center')
        fig_n1.save('n1_fracs_%d' % sample)
        fig_n2.save('n2_fracs_%d' % sample)

    logging.info("Plotting graphs for whole experiment...")
    fig_n1_overall = MyFig(options, xlabel='Number of neighbors',
                   ylabel='Fraction of MPRs', legend=True, aspect='auto',
                   grid=False)
    for i, num_n1 in enumerate(n1_x):
        result = cursor.execute('''
                SELECT AVG(mean_mprs), SUM(num_values), AVG(std)
                FROM eval_mpr_n1_fracs
                WHERE num_n1 = ?
                ''', (num_n1,)).fetchone()
        if None in result:
            continue
        c = 1.96 * result[2] / scipy.sqrt(result[1])
        frac = result[0] / float(num_n1)
        if c <= frac:
            fig_n1_overall.ax.errorbar(float(num_n1), frac, yerr=c, color=colors[i])
        fig_n1_overall.ax.bar(num_n1, frac, align='center')
    fig_n1_overall.save('n1_fracs_overall')

    fig_n2_overall = MyFig(options, xlabel='Number of neighbors',
                   ylabel='Fraction of MPRs', legend=True, aspect='auto',
                   grid=False)
    for i, num_n2 in enumerate(n2_x):
        result = cursor.execute('''
                SELECT AVG(mean_mprs), SUM(num_values), AVG(std)
                FROM eval_mpr_n2_fracs
                WHERE num_n2 = ?
                ''', (num_n2,)).fetchone()
        if None in result:
            continue
        c = 1.96 * result[2] / scipy.sqrt(result[1])
        frac = result[0] / float(num_n2)
        if c <= frac:
            fig_n2_overall.ax.errorbar(float(num_n2), frac, yerr=c, color=colors[i])
        fig_n2_overall.ax.bar(num_n2, frac, align='center')
    fig_n1_overall.save('n1_fracs_overall')
    fig_n2_overall.save('n2_fracs_overall')

#@requires_nhdp_hellos
#def plot_mpr_topology_changes(options, tags=None, cursor=None):
    #"""
    #Plots the relative amount the mpr set changes in a certain interval of time
    #for a certain value of nhdp_ht (H_HOLD_TIME/REFRESH_INTERVAL).
    #"""
    #options['prefix'] = "mpr"
    #colors = ['#111111','#333333','#555555','#777777','#999999','#bbbbbb','#dddddd']
    #markers = options['markers']
    #repetitions = 1
    #hosts = get_hosts(options)
    #fig = MyFig(options, xlabel='Time in seconds',
                   #ylabel='Number of absolute toplogy changes', legend=True, aspect='auto',
                   #grid=False)

    #max_samples = []
    #for i, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        #max_sample, = cursor.execute('''SELECT max(sample) FROM eval_mpr_topology_changes WHERE tag_key = ?''', (tag_key,)).fetchone()
        #max_samples.append(max_sample)
    #num_samples = min(max_samples)
    #samples_per_repetition = num_samples / repetitions
    #print samples_per_repetition

    ## prepare data
    #for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        #logging.info('tag_id=\"%s\" nhdp_hi=%d nhdp_ht=%d  (%d/%d)', tag_id, nhdp_hi, nhdp_ht, q+1, len(tags))
        #labels = []
        #data = {}
        #cur_sample = 0
        #for repetition in range(0, repetitions):
            #samples = {}
            #for sample in range(0, samples_per_repetition):
                #result, = cursor.execute('''SELECT SUM(abs_changes) FROM eval_mpr_topology_changes WHERE tag_key = ? AND sample = ?''', (tag_key, cur_sample)).fetchone()
                #samples[sample] = result
                #cur_sample += 1
            #data[repetition] = samples.copy()
        #value_list = []
        #confs = []
        #mean_y = []
        #for sample in range(0, samples_per_repetition):
            #for repetition in range(0, repetitions):
                #value_list.append(data[repetition][sample])
            #mean = scipy.mean(value_list)
            #mean_y.append(mean)
            ##c = 1.96 * scipy.std(value_list) / scipy.sqrt(len(value_list))
            ##fig.ax.errorbar(float(sample), mean, yerr=c, color=colors[q])
            ##conf = confidence(value_list)[2]
            ##confs.append(conf)
            ##poly = [conf2poly(np.arange(0, samples_per_repetition, 1), list(numpy.array(mean_y)+numpy.array(confs)), list(numpy.array(mean_y)-numpy.array(confs)), color=colors[q])]
            ##patch_collection = PatchCollection(poly, match_original=True)
            ##patch_collection.set_alpha(0.3)
            ##patch_collection.set_linestyle('dashed')
        ##fig.ax.add_collection(patch_collection)
        #fig.ax.set_xlim(0, samples_per_repetition)
        #fig.ax.plot(range(0, samples_per_repetition), mean_y, label='$hi:%s ht:%s$' % (nhdp_hi,nhdp_ht), color=colors[q], marker=markers[q])

    #fig.save('topology_changes')

@requires_nhdp_hellos
def plot_mpr_topology_changes_per_host(options, tags=None, cursor=None):
    """
    Plots the relative amount the mpr set changes in a certain interval of time
    for a certain value of nhdp_ht (H_HOLD_TIME/REFRESH_INTERVAL).
    """
    options['prefix'] = "mpr"
    max_sample = 20
    markers = options['markers']
    hosts = get_hosts(options)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(hosts)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(hosts)))
    measure_interval = 20
    re_nhdp_ht = re.compile(r".*nhdp_ht=(\w+).*")
    for q, (tag_key, tag_id, tag_helloSize, helloInterval) in enumerate(tags):
        fig = MyFig(options, xlabel='Hosts',
                   ylabel='Number of absolute toplogy changes', legend=True, aspect='auto',
                   grid=False)
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        total_changes = []
        for host in hosts:
            logging.info('host=\"%s\"', host)
            nhdp_ht = re_nhdp_ht.match(tag_id).group(1)
            result, = cursor.execute('''SELECT SUM(abs_changes) FROM eval_mpr_topology_changes WHERE tag_key = ? AND host = ?''', (tag_key, host)).fetchone()
            if result == None:
                result = 0
            total_changes.append(result)
        print total_changes
        fig.ax.bar(range(0, len(hosts)), total_changes, facecolor='#777777', align='center', ecolor='black', width=0.2)
        fig.ax.set_xlim(0,len(hosts))
        fig.save('topology_changes_%s_%s' % (host, nhdp_ht))

@requires_nhdp_hellos
def plot_mpr_topology_changes_per_host(options, tags=None, cursor=None):
    """
    Plots the relative amount the mpr set changes in a certain interval of time
    for a certain value of nhdp_ht (H_HOLD_TIME/REFRESH_INTERVAL).
    On the x-axis each number represents one router!!!
    """
    options['prefix'] = "mpr"
    max_sample = 20
    markers = options['markers']
    hosts = get_hosts(options)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(hosts)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(hosts)))
    measure_interval = 20
    for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        fig = MyFig(options, xlabel='Hosts',
                   ylabel='Number of absolute toplogy changes', legend=True, aspect='auto',
                   grid=False)
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        total_changes = []
        for i, (host) in enumerate(hosts):
            logging.info('host=\"%s\"', host)
            result, = cursor.execute('''SELECT SUM(abs_changes) FROM eval_mpr_topology_changes WHERE tag_key = ? AND host = ?''', (tag_key, host)).fetchone()
            if result == None:
                result = 0
            total_changes.append(result)
        total_changes = sorted(total_changes)
        fig.ax.bar(range(0, len(hosts)), total_changes, facecolor='#777777', align='center', ecolor='black', width=0.2)
        fig.ax.set_xlim(0,len(hosts))
        fig.save('topology_changes_per_host_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))

@requires_nhdp_hellos
def plot_nhdp_hello_topology(options, tags=None, cursor=None):
    """
    A 2d and 3d representation of the network graph is plotted.
    This function only plots the nodes that participated in the experiment.
    No links are plotted.
    """
    options['cur_src'] = 'topo'
    options['prefix'] = "nhdp"
    ################################################################################
    locs = options['locs']
    colors = options['color2'](pylab.linspace(0, 1, 101))
    ################################################################################
    circ_max = 5
    line_max = 10
    floor_factor = 2
    floor_skew = -0.25
    line_min = 1

    hosts = get_hosts(options)

    for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        fig2d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate')
        fig3d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate', ThreeD=True)
        if not q:
            fig3d_onlynodes = MyFig(options, xlabel='x Coordinate [m]', ylabel='y Coordinate [$m$]', ThreeD=True)

        min_x = min_y = min_z = numpy.infty
        max_x = max_y = max_z = 0

        # draw the nodes
        for host in hosts:
            try:
                xpos, ypos, zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue

            max_x = max(xpos, max_x)
            max_y = max(ypos, max_y)
            min_x = min(xpos, min_x)
            min_y = min(ypos, min_y)
            max_z = max(zpos, max_z)
            min_z = max(zpos, min_z)

            fig2d.ax.plot(
              xpos+zpos*floor_skew*floor_factor,
              ypos+zpos*floor_factor,
              'o', color='black', ms=circ_max)
            fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='black', ms=circ_max)
            if not q:
                color = 'black'
                if host.startswith('a6'):
                    color = 'red'
                elif host.startswith('a3'):
                    color = 'blue'
                elif host.startswith('a7'):
                    color = 'orange'
                fig3d_onlynodes.ax.plot([xpos], [ypos], [zpos], 'o', color=color, ms=circ_max)
        fig2d.colorbar = fig2d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        fig3d.colorbar = fig3d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        drawBuildingContours(fig3d.ax, options)
        drawBuildingContours(fig3d_onlynodes.ax, options)

        alinspace = numpy.linspace(0, 1, 100)
        alinspace = numpy.vstack((alinspace, alinspace))
        for tax in [fig2d.colorbar, fig3d.colorbar]:
            tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
            tax.set_xticks(pylab.linspace(0, 100, 5))
            tax.set_xticklabels(['$%.2f$' % l for l in pylab.linspace(0, 1, 5)], fontsize=0.8*options['fontsize'])
            tax.set_yticks([])
            tax.set_title('$PDR$', size=options['fontsize'])
        fig2d.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
        fig2d.save('2d_%s' % (tag_id))
        fig3d.save('3d_%s' % (tag_id))
        if not q:
            fig3d_onlynodes.save('3d')

@requires_nhdp_hellos
def plot_mpr_topology_per_node(options, tags=None, cursor=None):
    """
    A 2d and 3d representation of the network graph is plotted for each node that
    was chosen as mpr. WARNING: Amount of plots == number of mprs (~number of nodes)
    """
    interval = 600
    options['cur_src'] = 'topo'
    options['prefix'] = "mpr"
    ################################################################################
    locs = options['locs']
    colors = options['color2'](pylab.linspace(0, 1, 101))
    colors_mprs = ['green', 'red', 'cyan', 'magenta', 'yellow', 'grey']
    #colors_mprs = ['#222222', '#444444', '#666666', '#888888', '#aaaaaa', '#cccccc']
    ################################################################################
    circ_max = 5
    line_max = 10
    floor_factor = 2
    floor_skew = -0.25
    line_min = 1
    wait_time = 30
    measure_time = wait_time + 20
    draw_non_mpr_neighbors = False

    hosts = get_hosts(options)

    min_x = min_y = min_z = numpy.infty
    max_x = max_y = max_z = 0

    for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        # first draw the edges...
        for host in hosts:
            logging.info("Plotting tag_id: %s host: %s" % (tag_id, host))
            tx_if, = cursor.execute('''
                            SELECT tx_if
                            FROM he
                            WHERE host=?
                            ''',(host,)).fetchone()
            fig2d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate')
            fig3d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate', ThreeD=True)
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue
            ################################################################################
            # src is the receiving router, i.e. in our case the MPR
            # host  is the sending router, i.e. the MPR selector
            # We only want to draw an edge if it connects a host with its MPR.
            #
            ################################################################################

            neighbors = cursor.execute('''
                            SELECT DISTINCT(host)
                            FROM rh
                            WHERE prev = ?
                            ''', (tx_if,)).fetchall()
            min_time, = cursor.execute('''
                        SELECT min(time)
                        FROM nhdp_he
                        WHERE tag = ?
                        ''',(tag_key,)).fetchone()
            mprs = cursor.execute('''
                SELECT pdr.host, AVG(pdr)
                FROM eval_helloPDR AS pdr JOIN nhdp_mpr_selectors AS mpr
                ON pdr.host = mpr.host AND pdr.tx_if = mpr.mprselector
                WHERE pdr.tx_if = ? AND pdr.tag_key = ? AND mpr.time BETWEEN ? AND ?
                GROUP BY pdr.host
            ''', (tx_if, tag_key, min_time + wait_time, min_time + measure_time)).fetchall()
            logging.info("Host is %s..." % host)
            mpr_list = []
            for mpr, pdr in mprs:
                mpr_list.append(mpr)
                try:
                    src_xpos, src_ypos, src_zpos = locs[mpr]
                except KeyError:
                    logging.warning('no position found for node %s', mpr)
                    continue

                fig2d.ax.plot(
                    [host_xpos+host_zpos*floor_skew*floor_factor, src_xpos+src_zpos*floor_skew*floor_factor],
                    [host_ypos+host_zpos*floor_factor, src_ypos+src_zpos*floor_factor],
                    linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3, label=tag_id)

                fig3d.ax.plot(
                    [host_xpos, src_xpos],
                    [host_ypos, src_ypos],
                    [host_zpos, src_zpos],
                    linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3, label=tag_id)
            if draw_non_mpr_neighbors == True:
                for _host, in neighbors:
                    try:
                        src_xpos, src_ypos, src_zpos = locs[_host]
                    except KeyError:
                        logging.warning('no position found for node %s', _host)
                        continue

                    fig2d.ax.plot(
                        [host_xpos+host_zpos*floor_skew*floor_factor, src_xpos+src_zpos*floor_skew*floor_factor],
                        [host_ypos+host_zpos*floor_factor, src_ypos+src_zpos*floor_factor],
                        linestyle='-', color='black', linewidth=line_min, alpha=0.3)

                    fig3d.ax.plot(
                        [host_xpos, src_xpos],
                        [host_ypos, src_ypos],
                        [host_zpos, src_zpos],
                        linestyle='-', color='black', linewidth=line_min, alpha=0.3)
            # draw nodes
            color_idx = 0;
            skip_list = []
            n2_list = []
            for _host in hosts:
                if _host in skip_list:
                    continue
                try:
                    xpos, ypos, zpos = locs[_host]
                except KeyError:
                    logging.warning('no position found for node %s', _host)
                    continue

                max_x = max(xpos, max_x)
                max_y = max(ypos, max_y)
                min_x = min(xpos, min_x)
                min_y = min(ypos, min_y)
                max_z = max(zpos, max_z)
                min_z = max(zpos, min_z)
                if _host == host:
                    fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='blue', ms=circ_max*2)
                    fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='blue', ms=circ_max*2)
                elif _host in mpr_list:
                    fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color=colors_mprs[color_idx], ms=circ_max)
                    fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color=colors_mprs[color_idx], ms=circ_max)
                    mpr_tx_if, = cursor.execute('''
                            SELECT tx_if
                            FROM he
                            WHERE host=? AND time BETWEEN ? AND ?
                            ''',(_host, min_time + wait_time, min_time + measure_time)).fetchone()
                    n2_ifs = list(pylab.flatten(cursor.execute('''
                        SELECT DISTINCT(n2)
                        FROM nhdp_mpr_n2
                        WHERE host = ? AND time BETWEEN ? AND ?
                        ''', (_host,  min_time + wait_time, min_time + measure_time)).fetchall()))

                    n2s = cursor.execute('''
                        SELECT DISTINCT(host)
                        FROM he
                        WHERE tx_if IN (%s)
                        ''' % ','.join('?'*len(n2_ifs)), n2_ifs).fetchall()
                    #n2 = cursor.execute('''
                            #SELECT DISTINCT(host)
                            #FROM rh
                            #WHERE prev = ? AND time BETWEEN ? AND ?
                            #''', (mpr_tx_if, min_time + wait_time, min_time + measure_time)).fetchall()
                    mpr_xpos, mpr_ypos, mpr_zpos = locs[_host]
                    for __host, in n2s:
                        __host = unicodedata.normalize('NFKD', __host).encode('ascii','ignore')
                        if __host in n2_list:
                            continue
                        if __host in mpr_list or __host == host:
                            continue
                        n2_list.append(__host)
                        skip_list.append(__host)
                        try:
                            _xpos, _ypos, _zpos = locs[__host]
                        except KeyError:
                            logging.warning('no position found for node %s', __host)
                            continue
                        max_x = max(_xpos, max_x)
                        max_y = max(_ypos, max_y)
                        min_x = min(_xpos, min_x)
                        min_y = min(_ypos, min_y)
                        max_z = max(_zpos, max_z)
                        min_z = max(_zpos, min_z)
                        fig3d.ax.plot([_xpos], [_ypos], [_zpos], 'o', color=colors_mprs[color_idx], ms=circ_max)
                        fig2d.ax.plot(_xpos+_zpos*floor_skew*floor_factor,_ypos+_zpos*floor_factor,'o', color=colors_mprs[color_idx], ms=circ_max)

                        n2_xpos, n2_ypos, n2_zpos = locs[__host]

                        fig2d.ax.plot(
                            [n2_xpos+n2_zpos*floor_skew*floor_factor, mpr_xpos+mpr_zpos*floor_skew*floor_factor],
                            [n2_ypos+n2_zpos*floor_factor, mpr_ypos+mpr_zpos*floor_factor],
                            linestyle='-', color=colors_mprs[color_idx], linewidth=line_min, alpha=0.3)

                        fig3d.ax.plot(
                            [n2_xpos, mpr_xpos],
                            [n2_ypos, mpr_ypos],
                            [n2_zpos, mpr_zpos],
                            linestyle='-', color=colors_mprs[color_idx], linewidth=line_min, alpha=0.3)

                    color_idx = color_idx + 1;
                    if color_idx > 5:
                        color_idx = 0
                else:
                    fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='black', ms=circ_max)
                    fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='black', ms=circ_max)


            fig2d.colorbar = fig2d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
            fig3d.colorbar = fig3d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
            drawBuildingContours(fig3d.ax, options)
            alinspace = numpy.linspace(0, 1, 100)
            alinspace = numpy.vstack((alinspace, alinspace))
            for tax in [fig2d.colorbar, fig3d.colorbar]:
                tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
                tax.set_xticks(pylab.linspace(0, 100, 5))
                tax.set_xticklabels(['$%.2f$' % l for l in pylab.linspace(0, 1, 5)], fontsize=0.8*options['fontsize'])
                tax.set_yticks([])
                tax.set_title('$PDR$', size=options['fontsize'])
            fig2d.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
            logging.info("saving %s" %(host))
            fig2d.save('2d_mpr_topology_%s' % (host))
            fig3d.save('3d_mpr_topology_%s' % (host))

@requires_nhdp_hellos
def plot_mpr_topology_per_mpr(options, tags=None, cursor=None):
    """
    A 2d and 3d representation of the network graph is plotted for each node that
    was chosen as mpr. WARNING: Amount of plots == number of mprs (~number of nodes)
    """
    interval = 600
    options['cur_src'] = 'topo'
    options['prefix'] = "mpr"
    ################################################################################
    locs = options['locs']
    colors = options['color2'](pylab.linspace(0, 1, 101))
    ################################################################################
    circ_max = 5
    line_max = 10
    floor_factor = 2
    floor_skew = -0.25
    line_min = 1
    cur_time = cursor.execute('''
                        SELECT min(time)
                        FROM nhdp_mpr_selectors
                        ''').fetchone()
    mprs = cursor.execute('''
                    SELECT DISTINCT(host)
                    FROM nhdp_mpr_selectors
                    ''').fetchall()
    hosts = get_hosts(options)

    min_x = min_y = min_z = numpy.infty
    max_x = max_y = max_z = 0

    # first draw the edges...
    for i, (mpr,) in enumerate(mprs):
        logging.info('[%d/%d] plotting host=%s', i+1, len(mprs), mpr)
        fig2d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate')
        fig3d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate', ThreeD=True)
        try:
            host_xpos, host_ypos, host_zpos = locs[mpr]
        except KeyError:
            logging.warning('no position found for node %s', mpr)
            continue
        ################################################################################
        # host is the receiving router, i.e. in our case the MPR
        # src  is the sending router, i.e. the MPR selector
        # We only want to draw an edge if it connects an MPR with its SELECTOR.
        #
        ################################################################################
        cursor.execute('''
            SELECT src, AVG(pdr)
            FROM eval_helloPDR AS pdr JOIN nhdp_mpr_selectors AS mpr
            ON pdr.host = mpr.host AND pdr.tx_if = mpr.mprselector
            WHERE pdr.host=?
            GROUP BY src
        ''', (mpr, ))

        for src, pdr in cursor.fetchall():
            try:
                src_xpos, src_ypos, src_zpos = locs[src]
            except KeyError:
                logging.warning('no position found for node %s', src)
                continue

            fig2d.ax.plot(
                [host_xpos+host_zpos*floor_skew*floor_factor, src_xpos+src_zpos*floor_skew*floor_factor],
                [host_ypos+host_zpos*floor_factor, src_ypos+src_zpos*floor_factor],
                linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3)

            fig3d.ax.plot(
                [host_xpos, src_xpos],
                [host_ypos, src_ypos],
                [host_zpos, src_zpos],
                linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3)

        # draw all other nodes
        for host in hosts:
            try:
                xpos, ypos, zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue

            max_x = max(xpos, max_x)
            max_y = max(ypos, max_y)
            min_x = min(xpos, min_x)
            min_y = min(ypos, min_y)
            max_z = max(zpos, max_z)
            min_z = max(zpos, min_z)
            if host == mpr:
                fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='blue', ms=circ_max)
                fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='blue', ms=circ_max)
            else:
                fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='black', ms=circ_max)
                fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='black', ms=circ_max)
        fig2d.colorbar = fig2d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        fig3d.colorbar = fig3d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        drawBuildingContours(fig3d.ax, options)
        alinspace = numpy.linspace(0, 1, 100)
        alinspace = numpy.vstack((alinspace, alinspace))
        for tax in [fig2d.colorbar, fig3d.colorbar]:
            tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
            tax.set_xticks(pylab.linspace(0, 100, 5))
            tax.set_xticklabels(['$%.2f$' % l for l in pylab.linspace(0, 1, 5)], fontsize=0.8*options['fontsize'])
            tax.set_yticks([])
            tax.set_title('$PDR$', size=options['fontsize'])
        fig2d.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
        fig2d.save('2d_topology_per_mpr_%s' % (mpr))
        fig3d.save('3d_topology_per_mpr_%s' % (mpr))

@requires_nhdp_hellos
def plot_mpr_topology(options, tags=None, cursor=None):
    """
    A 2d and 3d representation of the network graph is plotted, nodes frequently selected
    as mprs are colored with blue. Only links between mprs and their selectors are plotted.
    The pdr of each link determines its color and thickness.
    """
    options['cur_src'] = 'topo'
    options['prefix'] = "mpr"
    ################################################################################
    locs = options['locs']
    colors = options['color2'](pylab.linspace(0, 1, 101))
    ################################################################################
    circ_max = 5
    line_max = 10
    floor_factor = 2
    floor_skew = -0.25
    line_min = 1

    hosts = get_hosts(options)
    mprs = cursor.execute('''
                    SELECT DISTINCT(host)
                    FROM nhdp_mpr_selectors
                    ''').fetchall()

    for q, (tag_key, tag_id, nhdp_hi, nhdp_ht, mpr_minpdr) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        min_max_time = cursor.execute('''
                            SELECT min(time), max(time)
                            FROM nhdp_he
                            WHERE tag=?
                            ''',(tag_key,)).fetchone()
        fig2d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate')
        fig3d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate', ThreeD=True)
        if not q:
            fig3d_onlynodes = MyFig(options, xlabel='x Coordinate [m]', ylabel='y Coordinate [$m$]', ThreeD=True)

        min_x = min_y = min_z = numpy.infty
        max_x = max_y = max_z = 0

        # first draw the edges...
        for nr, (host) in enumerate(hosts):
            logging.info('  [%d/%d] drawing edges for host=%s', nr+1, len(hosts), host)
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue
            ################################################################################
            # host is the receiving router, i.e. in our case the MPR
            # src  is the sending router, i.e. the MPR selector
            # We only want to draw an edge if it connects an MPR with its SELECTOR.
            #
            ################################################################################
            cursor.execute('''
                SELECT DISTINCT(src), pdr
                FROM eval_helloPDR AS pdr JOIN nhdp_mpr_selectors AS mpr
                ON pdr.host = mpr.host AND pdr.tx_if = mpr.mprselector
                WHERE pdr.tag_key=? AND pdr.host=? AND mpr.time BETWEEN ? AND ?
            ''', (tag_key, host, min_max_time[0], min_max_time[1]))
            for src, pdr in cursor.fetchall():
                try:
                    src_xpos, src_ypos, src_zpos = locs[src]
                except KeyError:
                    logging.warning('no position found for node %s', src)
                    continue

                fig2d.ax.plot(
                    [host_xpos+host_zpos*floor_skew*floor_factor, src_xpos+src_zpos*floor_skew*floor_factor],
                    [host_ypos+host_zpos*floor_factor, src_ypos+src_zpos*floor_factor],
                    linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3)

                fig3d.ax.plot(
                    [host_xpos, src_xpos],
                    [host_ypos, src_ypos],
                    [host_zpos, src_zpos],
                    linestyle='-', color=colors[pdr*100], linewidth=max(line_max*pdr, line_min), alpha=0.3)

        # draw the nodes
        for host in hosts:
            logging.info('  [%d/%d] drawing node %s', nr+1, len(hosts), host)
            try:
                xpos, ypos, zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue

            max_x = max(xpos, max_x)
            max_y = max(ypos, max_y)
            min_x = min(xpos, min_x)
            min_y = min(ypos, min_y)
            max_z = max(zpos, max_z)
            min_z = max(zpos, min_z)
            if (host,) in mprs:
                fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='blue', ms=circ_max)
                fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='blue', ms=circ_max)
            else:
                fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='black', ms=circ_max)
                fig2d.ax.plot(xpos+zpos*floor_skew*floor_factor,ypos+zpos*floor_factor,'o', color='black', ms=circ_max)
            if not q:
                color = 'black'
                if host.startswith('a6'):
                    color = 'red'
                elif host.startswith('a3'):
                    color = 'blue'
                elif host.startswith('a7'):
                    color = 'orange'
                fig3d_onlynodes.ax.plot([xpos], [ypos], [zpos], 'o', color=color, ms=circ_max)
        fig2d.colorbar = fig2d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        fig3d.colorbar = fig3d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
        drawBuildingContours(fig3d.ax, options)
        drawBuildingContours(fig3d_onlynodes.ax, options)

        alinspace = numpy.linspace(0, 1, 100)
        alinspace = numpy.vstack((alinspace, alinspace))
        for tax in [fig2d.colorbar, fig3d.colorbar]:
            tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
            tax.set_xticks(pylab.linspace(0, 100, 5))
            tax.set_xticklabels(['$%.2f$' % l for l in pylab.linspace(0, 1, 5)], fontsize=0.8*options['fontsize'])
            tax.set_yticks([])
            tax.set_title('$PDR$', size=options['fontsize'])
        fig2d.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
        fig2d.save('2d_topology_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))
        fig3d.save('3d_topology_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))
        if not q:
            fig3d_onlynodes.save('3d_topology_only_nodes_hi_%d_ht_%d_minpdr_%.2f' % (nhdp_hi, nhdp_ht, mpr_minpdr))

@requires_parallel_sources
def plot_mpr_retransmission(options, cursor = None):
    ''' This function allows to plot the reachability for an experiment only
    using PARALLEL sources.'''
    logging.info('')
    options['prefix'] = "nhdp"
    markers = options['markers']
    nhdp_hi_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_hi) FROM tag ORDER BY nhdp_hi''').fetchall()))
    nhdp_ht_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_ht) FROM tag ORDER BY nhdp_ht''').fetchall()))
    mpr_minpdr_list = list(pylab.flatten(cursor.execute('''SELECT distinct(mpr_minpdr) FROM tag ORDER BY mpr_minpdr''').fetchall()))
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    results = dict()
    num_combos = 0
    for nhdp_hi in nhdp_hi_list:
        for nhdp_ht in nhdp_ht_list:
            for mpr_minpdr in mpr_minpdr_list:
                num_combos += 1
                data = cursor.execute('''
                            SELECT s.sources, e.iteration, e.reachability, e.gain
                            FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                            ON e.tag_key = t.key AND t.key = s.tag_key
                            WHERE t.nhdp_hi = ? AND t.nhdp_ht = ? AND t.mpr_minpdr = ?
                            ORDER BY s.sources
                        ''', (nhdp_hi, nhdp_ht, mpr_minpdr)).fetchall()

                for num_sources, iteration, reachability, gain in data:
                    while num_sources not in [10,30,50,70,90]:
                        num_sources = num_sources + 1

                    if num_sources in results.keys():
                        if nhdp_hi in results[num_sources].keys():
                            if nhdp_ht in results[num_sources][nhdp_hi].keys():
                                if mpr_minpdr in results[num_sources][nhdp_hi][nhdp_ht].keys():
                                    if iteration in results[num_sources][nhdp_hi][nhdp_ht][mpr_minpdr].keys():
                                        results[num_sources][nhdp_hi][nhdp_ht][mpr_minpdr][iteration].append((reachability, gain))
                                    else:
                                        results[num_sources][nhdp_hi][nhdp_ht][mpr_minpdr][iteration] = [(reachability, gain)]
                                else:
                                    results[num_sources][nhdp_hi][nhdp_ht][mpr_minpdr] = {iteration : [(reachability, gain)]}
                            else:
                                results[num_sources][nhdp_hi][nhdp_ht] = {mpr_minpdr : {iteration : [(reachability, gain)]}}
                        else:
                            results[num_sources][nhdp_hi] = {nhdp_ht : {mpr_minpdr : {iteration : [(reachability, gain)]}}}
                    else:
                        results[num_sources] = {nhdp_hi : {nhdp_ht : {mpr_minpdr : {iteration : [(reachability, gain)]}}}}
    max_retransmissions = max([_i for _s, _i, _r, _g in data])
    print num_combos
    for m, (num_sources, nhdp_hi_dict) in enumerate(results.iteritems()):
        index = 0
        fig_reachability_over_retransmission = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
        fig_reachability_over_retransmission.ax.set_ylim(float(0), 1)
        fig_reachability_over_retransmission.legend_title = "Parameters"
        fig_gains_line = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel='Reachability Gain',legend=True, grid=False, aspect='auto')
        for z, (nhdp_hi, nhdp_ht_dict) in enumerate(nhdp_hi_dict.iteritems()):
            for nhdp_ht, mpr_minpdr_dict in nhdp_ht_dict.iteritems():
                for k, (mpr_minpdr, iter_dict) in enumerate(mpr_minpdr_dict.iteritems()):
                    colors = options['color'](pylab.linspace(0, 1, num_combos+1))
                    mean_y = list()
                    mean_diffs = list()
                    confs = list()
                    prev_mean = 0
                    gains = None
                    for j, (iteration, r_and_g) in enumerate(iter_dict.iteritems()):
                        r_list = [r for r, g in r_and_g]
                        g_list = [g for r, g in r_and_g]
                        #probabilities = pylab.linspace(0, 1, 1000)
                        #########
                        #kernel = kde.gaussian_kde(g_list)
                        #kernel.covariance_factor = lambda : .1
                        #kernel._compute_covariance()
                        #density = kernel(probabilities)
                        #print 'integral=%f' % kernel.integrate_box_1d(0, 1)
                        #fig_gains_pdf.ax.plot(probabilities, density, color=colors[j], label='$%d$' % iteration)
                        #fig_gains_pdf3d.ax.plot(probabilities, [iteration for x in probabilities], density, color=colors[iteration])
                        #########
                        #X, Y = data2hist(g_list, bins=options['bins'], range=(0,1))
                        #fig_gains_hist3d.ax.plot(X, [iteration for x in X], Y, color=colors[iteration])
                        #########
                        #percentiles = scipy.stats.mstats.mquantiles(g_list, prob=probabilities)
                        #fig_gains_cdf.ax.plot(percentiles, probabilities, color=colors[iteration], label='$%d$' % iteration)
                        #########
                        #gain_dist, bin_edges = numpy.histogram(g_list, range=(0.0, 1.0), bins=options['bins'])
                        #gain_dist = gain_dist/float(len(g_list))
                        #gain_std = scipy.std(g_list)
                        #gain_mean = scipy.mean(g_list)
                        #if gains == None:
                        #    gains = gain_dist
                        #else:
                        #    gains = numpy.vstack((gains, gain_dist))
                        mean = scipy.mean(r_list)
                        mean_diffs.append(mean - prev_mean)
                        prev_mean = mean
                        mean_y.append(mean)
                        conf = confidence(r_list)[2]
                        confs.append(conf)
                    print index
                    fig_gains_line.ax.plot(range(0, max_retransmissions+1, 1), mean_diffs, label='$hi:%d,ht:%d,minpdr:%d$' % (nhdp_hi,nhdp_ht,mpr_minpdr), marker=markers[index], color=colors[index])
                    fig_reachability_over_retransmission.ax.set_xlim(0, max_retransmissions)
                    poly = [conf2poly(np.arange(0, max_retransmissions+1, 1), list(numpy.array(mean_y)+numpy.array(confs)), list(numpy.array(mean_y)-numpy.array(confs)), color=colors[index])]
                    patch_collection = PatchCollection(poly, match_original=True)
                    patch_collection.set_alpha(0.3)
                    patch_collection.set_linestyle('dashed')
                    fig_reachability_over_retransmission.ax.add_collection(patch_collection)
                    fig_reachability_over_retransmission.ax.plot(np.arange(0, max_retransmissions+1, 1), mean_y, label='$hi:%d,ht:%d,minpdr:%d$' % (nhdp_hi,nhdp_ht,mpr_minpdr), marker=markers[index], color=colors[index])
                    index += 1

        fig_reachability_over_retransmission.save("parallel_retransmission_hi_%d_ht_%d_minpdr_%.2f_srcs_%d" % (nhdp_hi, nhdp_ht, mpr_minpdr, num_sources))
        fig_gains_line.legend_title = "Parameters"
        fig_gains_line.save("parallel_retransmission_gains_hi_%d_ht_%d_minpdr_%.2f_srcs_%d" % (nhdp_hi, nhdp_ht, mpr_minpdr, num_sources))

@requires_parallel_sources
def plot_mpr_reachability(options, cursor=None):
    '''Plots the reachability when multiple sources sent packets at the same time.
       The reachability is plottet as function over p'''
    logging.info('')
    options['prefix'] = 'mpr'

    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    nhdp_hi_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_hi) FROM tag ORDER BY nhdp_hi''').fetchall()))
    nhdp_ht_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_ht) FROM tag ORDER BY nhdp_ht''').fetchall()))
    mpr_minpdr_list = list(pylab.flatten(cursor.execute('''SELECT distinct(mpr_minpdr) FROM tag ORDER BY mpr_minpdr''').fetchall()))
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))
    num_hosts = len(get_hosts(options))

    logging.info('intervals: %s' % str(intervals))
    logging.info('nhdp_hts: %s' % str(nhdp_ht_list))
    logging.info('nhdp_his: %s' % str(nhdp_hi_list))
    logging.info('mpr_minpdr: %s' % str(mpr_minpdr_list))
    logging.info('sources: %s' % str(sources))

    for interval in intervals:
        for nhdp_hi in nhdp_hi_list:
            data_interval = dict()
            for j, nhdp_ht in enumerate(nhdp_ht_list):
                for minpdr in mpr_minpdr_list:
                    data = cursor.execute('''
                        SELECT s.sources, AVG(e.frac)
                        FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
                        ON t.key = e.tag_key AND t.key = s.tag_key
                        WHERE t.pkg_interval=? AND t.nhdp_hi = ? AND t.nhdp_ht=? AND t.mpr_minpdr = ?
                        AND SOURCES > 1
                        GROUP BY t.key
                        ORDER BY s.sources
                    ''', (interval, nhdp_hi, nhdp_ht, minpdr)).fetchall()
                    results = data2dict(data)
                    try:
                        hashmap = data_interval[minpdr]
                    except KeyError:
                        data_interval[minpdr] = dict()
                        hashmap = data_interval[minpdr]
                    hashmap[nhdp_ht] = results
                    fracs = [results[srcs] for srcs in sorted(results.keys())]
                    if len(fracs) == 0:
                        logging.warning('no data for: interval=%.2f, nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f', interval, nhdp_hi, nhdp_ht, minpdr)
                        continue
                    fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', grid=False, aspect='auto')
                    fig.ax.boxplot(fracs, notch=1)
                    fig.ax.set_xticklabels(['$%s$' % i for i in sorted(results.keys())])
                    fig.ax.set_ylim(0,1)
                    fig.save('reachability_interval_%.2f_nhdp_hi_%d_nhdp_ht_%d_minpdr_%.2f' % (interval, nhdp_hi, nhdp_ht, minpdr))
            for minpdr, data in data_interval.iteritems():
                fig = data2fig(data, sources, options, legend_title='$t_h$ [ms]', xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$')
                fig.save('reachability_interval_%.2f_nhdp_hi_%d_minpdr_%.2f' % (interval, nhdp_hi, minpdr))


@requires_parallel_sources
def plot_mpr_forwarded(options, cursor=None):
    '''Plots the number of forwarded packets as function of p'''
    logging.info('')
    options['prefix'] = 'mpr'

    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    nhdp_hi_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_hi) FROM tag ORDER BY nhdp_hi''').fetchall()))
    nhdp_ht_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_ht) FROM tag ORDER BY nhdp_ht''').fetchall()))
    mpr_minpdr_list = list(pylab.flatten(cursor.execute('''SELECT distinct(mpr_minpdr) FROM tag ORDER BY mpr_minpdr''').fetchall()))
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))
    num_hosts = len(get_hosts(options))

    logging.info('intervals: %s' % str(intervals))
    logging.info('nhdp_hts: %s' % str(nhdp_ht_list))
    logging.info('nhdp_his: %s' % str(nhdp_hi_list))
    logging.info('mpr_minpdr: %s' % str(mpr_minpdr_list))
    logging.info('sources: %s' % str(sources))

    for j, interval in enumerate(intervals):
        for nhdp_hi in nhdp_hi_list:
            data_interval = dict()
            for nhdp_ht in nhdp_ht_list:
                for minpdr in mpr_minpdr_list:
                    data = cursor.execute('''
                        SELECT A.sources, CAST(B.forwarded AS REAL)/D.SENT/?
                        FROM eval_sources_per_tag AS A JOIN eval_fw_packets_per_tag as B JOIN tag as C JOIN (
                            SELECT tag, COUNT(*) AS SENT
                            FROM tx GROUP BY tag
                        ) AS D
                        ON A.tag_key=B.tag_key AND B.tag_key=C.key AND C.key=D.tag
                        WHERE C.pkg_interval=? AND C.nhdp_hi=? AND C.nhdp_ht=? AND C.mpr_minpdr=?
                        AND A.SOURCES > 1
                        ORDER BY A.sources
                    ''', (float(num_hosts), interval, nhdp_hi, nhdp_ht, minpdr)).fetchall()
                    results = data2dict(data)
                    try:
                        hashmap = data_interval[minpdr]
                    except KeyError:
                        data_interval[minpdr] = dict()
                        hashmap = data_interval[minpdr]
                    hashmap[nhdp_ht] = results
                    fracs = [results[srcs] for srcs in sorted(results.keys())]
                    if len(fracs) == 0:
                        logging.warning('no data for: interval=%.2f, nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f', interval, nhdp_hi, nhdp_ht, minpdr)
                        continue
                    fig = MyFig(options, figsize=(10,8), xlabel=r'Sources~$\sources$', ylabel=r'Fraction of Forwarded Packets~${\forwarded}$', grid=False, aspect='auto')
                    fig.ax.boxplot(fracs, notch=1)
                    fig.ax.set_xticklabels(['$%s$' % i for i in sorted(results.keys())])
                    fig.ax.set_ylim(0, 0.4)
                    fig.legend_title = r'Sources~$\sources$'
                    fig.save('forwarded_interval_%.2f_nhdp_hi_%d_nhdp_ht_%d_minpdr_%.2f' % (interval, nhdp_hi, nhdp_ht, minpdr))
            for minpdr, data in data_interval.iteritems():
                fig = data2fig(data, sources, options, legend_title='$t_h$ [ms]', xlabel=r'Sources~$\sources$', ylabel=r'Fraction of Forwarded Packets~${\forwarded}$')
                fig.ax.set_ylim(0, 0.4)
                fig.save('forwarded_interval_%.2f_nhdp_hi_%d_minpdr_%.2f' % (interval, nhdp_hi, minpdr))


@requires_parallel_sources
def plot_mpr_reachability_over_fw(options, cursor=None):
    '''Plots the number of forwarded packets as function of p'''
    logging.info('')
    options['prefix'] = 'mpr'

    fit_file = open(options['outdir']+ '/fit.data', 'w')
    r_file = open(options['outdir']+'/reachability.data', 'w')

    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    nhdp_hi_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_hi) FROM tag ORDER BY nhdp_hi''').fetchall()))
    nhdp_ht_list = list(pylab.flatten(cursor.execute('''SELECT distinct(nhdp_ht) FROM tag ORDER BY nhdp_ht''').fetchall()))
    mpr_minpdr_list = list(pylab.flatten(cursor.execute('''SELECT distinct(mpr_minpdr) FROM tag ORDER BY mpr_minpdr''').fetchall()))
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag WHERE SOURCES > 1 ORDER BY sources''').fetchall()))
    num_hosts = len(get_hosts(options))
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(sources)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(sources)))

    for j, interval in enumerate(intervals):
        for nhdp_hi in nhdp_hi_list:
            for nhdp_ht in nhdp_ht_list:
                for minpdr in mpr_minpdr_list:
                    data_fw = cursor.execute('''
                        SELECT A.sources, CAST(B.forwarded AS REAL)/D.SENT
                        FROM eval_sources_per_tag AS A JOIN eval_fw_packets_per_tag as B JOIN tag as C JOIN (
                            SELECT tag, COUNT(*) AS SENT
                            FROM tx GROUP BY tag
                        ) AS D
                        ON A.tag_key=B.tag_key AND B.tag_key=C.key AND C.key=D.tag
                        WHERE C.pkg_interval=? AND C.nhdp_hi=? AND C.nhdp_ht=? AND C.mpr_minpdr=?
                        AND SOURCES > 1
                        ORDER BY A.sources
                    ''', (interval, nhdp_hi, nhdp_ht, minpdr)).fetchall()
                    if len(data_fw) == 0:
                        logging.warning('no data found for: interval=%f and nhdp_ht=%d', interval, nhdp_ht)
                        continue

                    data_r = cursor.execute('''
                        SELECT s.sources, AVG(e.frac)
                        FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
                        ON t.key = e.tag_key AND t.key = s.tag_key
                        WHERE t.pkg_interval=? AND t.nhdp_hi=? AND t.nhdp_ht=? AND t.mpr_minpdr=?
                        AND SOURCES > 1
                        GROUP BY t.key
                        ORDER BY s.sources
                    ''', (interval, nhdp_hi, nhdp_ht, minpdr)).fetchall()
                    if len(data_r) == 0:
                        logging.warning('no data found for: interval=%f and nhdp_ht=%d', interval, nhdp_ht)
                        continue

                    results_r = data2dict(data_r)
                    results_fw = data2dict(data_fw)

                    fig = MyFig(options, figsize=(10,8), ylabel=r'Reachability~$\reachability$', xlabel=r'Fraction of Forwarded Packets~${\forwarded}$', grid=False, legend=True, aspect='auto', legend_pos='best')
                    ndata = list()
                    cdata = list()
                    ellipses = list()
                    alpha = 0.8
                    for i, sources in enumerate(sorted(results_r.keys())):
                        r = results_r[sources]
                        r_conf = confidence(r)[2]
                        try:
                            fw = numpy.array(results_fw[sources])/float(num_hosts-1)
                        except KeyError:
                            continue
                        fw_conf = confidence(fw)[2]
                        #fig.ax.plot(-1, -1, label='%d' % sources, color=colors[i])
                        ellipse = Ellipse((scipy.mean(fw), scipy.mean(r)), fw_conf*2, r_conf*2, edgecolor='black', facecolor=colors[i], alpha=alpha)
                        ellipse2 = Ellipse((scipy.mean(fw), scipy.mean(r)), fw_conf*2, r_conf*2, edgecolor='black', facecolor=colors[i], alpha=alpha)
                        ellipses.append((scipy.mean(fw), scipy.mean(r), ellipse, ellipse2))
                        ndata.append((scipy.mean(fw), scipy.mean(r)))
                        cdata.append((fw_conf, r_conf))
                        r_file.write('R(N=%d, nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f)=%f, %f, %f\n' % (sources, nhdp_hi, nhdp_ht, minpdr, scipy.mean(r), r_conf, fw_conf))
                        print 'R(N=%d, nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f)=%f, %f, %f' % (sources, nhdp_hi, nhdp_ht, minpdr, scipy.mean(r), r_conf, fw_conf)
                    add_legend(options, fig, ['$%s$' % s for s in sorted(results_r.keys())], alpha=alpha)
                    patch_collection = PatchCollection([e1 for x, y, e1, e2 in ellipses], match_original=True)
                    fig.ax.add_collection(patch_collection)
                    axins = zoomed_inset_axes(fig.ax, 2, loc=8)
                    axins.set_xlim(min([x for x, y, e1, e2 in ellipses])-0.02, max([x for x, y, e1, e2 in ellipses])+0.02)
                    axins.set_ylim(min([y for x, y, e1, e2 in ellipses])-0.02, max([y for x, y, e1, e2 in ellipses])+0.02)
                    patch_collection2 = PatchCollection([e2 for x, y, e1, e2 in ellipses], match_original=True)
                    axins.add_collection(patch_collection2)
                    axins.set_xticklabels([])
                    axins.set_yticklabels([])
                    pargs = numpy.polyfit([x for x,y in ndata], [y for x,y in ndata], 1)
                    fit_file.write('[nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f], R(F) = %fx + %f\n' % (nhdp_hi, nhdp_ht, minpdr, pargs[0], pargs[1]))
                    print '[nhdp_hi=%d, nhdp_ht=%d, minpdr=%.2f], R(F) = %fx + %f' % (nhdp_hi, nhdp_ht, minpdr, pargs[0], pargs[1])
                    x = pylab.linspace(0, 1, 1000)
                    fig.ax.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
                    axins.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
                    mark_inset(fig.ax, axins, loc1=2, loc2=3, fc="none", ec="0.5")
                    ##### fit top-left
                    X = numpy.array([x for x,y in ndata]) - numpy.array([dx for dx, dy in cdata])
                    #Y = [y for x,y in ndata]
                    Y = numpy.array([y for x,y in ndata]) + numpy.array([dy for dx, dy in cdata])
                    pargs2 = numpy.polyfit(X, Y, 1)
                    ##### fit bottom-right
                    #X = [x for x,y in ndata]
                    X = numpy.array([x for x,y in ndata]) + numpy.array([dx for dx, dy in cdata])
                    Y = numpy.array([y for x,y in ndata]) - numpy.array([dy for dx, dy in cdata])
                    pargs3 = numpy.polyfit(X, Y, 1)
                    #####
                    xcross = (pargs3[1]-pargs2[1]) / float(pargs2[0]-pargs3[0])
                    x = pylab.linspace(xcross, 1, 1000)

                    X = numpy.concatenate((x, x[::-1]))
                    Y = numpy.concatenate((numpy.polyval(pargs2, x), numpy.polyval(pargs3, x)[::-1]))
                    poly1 = Polygon(zip(X, Y), edgecolor='gray', facecolor='gray', closed=True, alpha=0.3)
                    poly2 = Polygon(zip(X, Y), edgecolor='gray', facecolor='gray', closed=True, alpha=0.3)
                    patch_collection1 = PatchCollection([poly1], match_original=True)
                    patch_collection1.zorder = -2
                    patch_collection2 = PatchCollection([poly2], match_original=True)
                    patch_collection2.zorder = -2
                    fig.ax.add_collection(patch_collection1)
                    axins.add_collection(patch_collection2)
                    ######
                    fig.ax.set_xlim(0, 1)
                    fig.ax.set_ylim(0, 1)
                    fig.legend_title = r'Sources~$\sources$'
                    fig.save('reachability_over_fw_interval_%.2f_nhdp_hi_%d_nhdp_ht_%d_minpdr_%.2f' % (interval, nhdp_hi, nhdp_ht, minpdr))
