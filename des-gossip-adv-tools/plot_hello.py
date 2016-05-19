#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
import matplotlib.cm as cm
from matplotlib.patches import Polygon, Rectangle
from matplotlib.collections import PatchCollection
import numpy, math
import pylab
from helpers import *
import scipy
from scipy import stats, polyfit, polyval
import networkx as nx


def plot_rssi(options):
    plot_hello_rssi_over_pdr(options)
    plot_hello_rssi_over_distance(options)
    plot_hello_rssi_bidirectional(options)

def plot_rssi_extra(options):
    plot_hello_rssi_normaltest(options)
    plot_hello_rssi_std(options)

def plot_pdr(options):
    plot_hello_pdr(options)
    plot_hello_pdr_diff(options)
    plot_hello_pdr_distance(options)
    plot_hello_pdr_asymmetry(options)

def plot_hello_no_rssi(options):
    plot_pdr(options)
    plot_hello_etx(options)
    plot_hello_links(options)
    plot_hello_links_uni(options)
    plot_hello_degree(options)
    plot_hello_topology(options)
    plot_hello_components(options)
    plot_hello_diameter(options)
    plot_hello_nodes_dists(options)

def plot_hello(options):
    plot_rssi(options)
    plot_hello_cds(options)
    plot_hello_no_rssi(options)

def plot_hello_all(options):
    plot_hello(options)
    plot_rssi_extra(options)

@requires_hellos
def plot_hello_cds(options, tags=None, cursor=None):
    options['prefix'] = 'cds'
    ################################################################################
    min_pdrs = list(pylab.flatten(cursor.execute('''SELECT DISTINCT(min_pdr) FROM eval_mcds ORDER BY min_pdr''').fetchall()))
    hosts = num_hosts(options)
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)
    colors = options['color'](pylab.linspace(0, 1.0, len(min_pdrs)))
    linestyles = options['linestyles']*10
    ################################################################################
    fig = MyFig(options, legend=True, grid=False, xlabel='Packet Size [bytes]', ylabel='Fractional $CDS$ Size', aspect='auto')
    for i, min_pdr in enumerate(min_pdrs):
        data = cursor.execute('''
            SELECT helloSize, MIN(size), max_reach
            FROM eval_mcds JOIN tag
            ON eval_mcds.tag_key = tag.key
            WHERE min_pdr=? AND bidirectional=1
            GROUP BY helloSize
            ORDER BY helloSize
        ''', (min_pdr,)).fetchall()
        if len(data) == 0:
            continue
        fig.ax.plot([p for p, s, r in data], [s/(hosts*r) for p, s, r in data], color=colors[i], linestyle=linestyles[i], label='$%.2f$' % min_pdr)
    fig.ax.set_xticks([h for _k, _i, h, _t in tags])
    fig.ax.set_ylim(0, 0.5)
    fig.legend_title = 'Minimum $PDR$'
    fig.save('cds')

@requires_hellos
def plot_hello_rssi_over_pdr(options, tags=None, cursor=None):
    options['prefix'] = 'rssi'
    ################################################################################
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        if options['neg_rssi']:
            data = cursor.execute('''
                SELECT pdr, rssi
                FROM eval_helloPDR
                WHERE tag_key=?
                AND rssi >= -255
                AND rssi < 0
            ''', (tag_key,)).fetchall()
        else:
            data = cursor.execute('''
                SELECT pdr, rssi-255
                FROM eval_helloPDR
                WHERE tag_key=?
                AND rssi <= 255
                AND rssi > 0
            ''', (tag_key,)).fetchall()
        assert(len(data))

        ############################################################
        fig = MyFig(options, legend=False, grid=False, xlabel='$Avr(PDR)$', ylabel='$Avr(RSSI)~[dBm]$', aspect='auto')
        fig.ax.set_ylim(-120, -30)
        PDR = [pdr for pdr, rssi in data]
        RSSI = [rssi for pdr, rssi in data]
        #fig.ax.scatter(PDR, RSSI)

        ############################################################
        def classify(pdr, rssi, classes):
            for i, c in enumerate(classes):
                if c <= metric(pdr, rssi, pdr_faktor):
                    break
            return len(classes)-i-1

        def metric(PDR, RSSI, pdr_faktor=1):
            N = RSSI+numpy.array(PDR)*pdr_faktor
            return N/(2.0 + pdr_faktor/2.0)

        pdr_faktor = 2.0
        classes = pylab.linspace(0.6, 0.0, 5)

        RSSIn = normRSSI(RSSI)
        cls = [list() for i in range(0, len(classes))]
        for pdr, (r0, r1) in zip(PDR, RSSIn):
            c = classify(pdr, r1, classes)
            cls[c].append((pdr, r0))
        colors = options['color'](pylab.linspace(0, 1.0, len(cls)))
        for i, d in enumerate(cls):
            if len(d):
                fig.ax.scatter([x for x,y in d], [y for x,y in d], color=colors[i])
        ############################################################
        polycoeffs = list(scipy.polyfit(PDR, RSSI, 3))
        xfit = pylab.linspace(min(PDR), max(PDR), 100)
        yfit = polyval(polycoeffs, xfit)
        poly = coeffs2polystr(polycoeffs)
        fig.ax.text(0.0, -35, '$f(x)=%s$' % poly, color='black', fontsize=options['fontsize']*0.8)
        fig.ax.plot(xfit, yfit, color='black', lw=2)
        fig.save('over_pdr_%d' % (helloSize))
        ############################################################
        #N = metric(PDR, [r1 for r0, r1 in RSSIn], pdr_faktor)
        #X, Y = data2hist(N, bins=40, range=(0,1))
        #fig = MyFig(options, legend=False, grid=False, xlabel=r'$Average(PDR + RSSI_{normed})$', ylabel='Fraction', aspect='auto')
        #fig.ax.plot(X, Y)
        #fig.ax.set_ylim(0, 0.4)
        #fig.save('times_pdr_%d' % (helloSize))
        ############################################################
        fig = MyFig(options, legend=False, grid=False, xlabel=r'$Avr(PDR)$', ylabel='$Avr(RSSI)~[dBm]$', aspect='auto')
        H, xedges, yedges = numpy.histogram2d(PDR, RSSI, bins=[40, 80], range=[[0, 1],[-100, -30]], normed=False, weights=None)
        H = H/len(PDR)
        mH = numpy.ma.masked_less_equal(H, 0.0)
        X, Y = pylab.meshgrid(xedges, yedges)
        if options['grayscale']:
            pcolor = fig.ax.pcolorfast(X, Y, numpy.transpose(mH), cmap=options['graycm'], vmin=0.0)
        else:
            pcolor = fig.ax.pcolorfast(X, Y, numpy.transpose(mH), cmap=options['color'], vmin=0.0)
        cbar = fig.colorbar(pcolor)
        fig.save('over_pdr_pcolor_%d' % (helloSize))

@requires_hellos
def plot_hello_rssi_over_distance(options, tags=None, cursor=None):
    options['prefix'] = 'rssi'
    ################################################################################
    locs = options['locs']
    ################################################################################
    hist_list = list()
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        if options['neg_rssi']:
            data = cursor.execute('''
            SELECT src, host, rssi
            FROM eval_helloPDR
            WHERE tag_key=?
            AND rssi >= -255
            AND rssi < 0
            ''', (tag_key,)).fetchall()
        else:
            data = cursor.execute('''
            SELECT src, host, rssi-255
            FROM eval_helloPDR
            WHERE tag_key=?
            AND rssi <= 255
            AND rssi > 0
            ''', (tag_key,)).fetchall()
        results = list()
        for src, host, rssi in data:
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
                src_xpos, src_ypos, src_zpos = locs[src]
            except KeyError:
                logging.warning('no position found for node %s or %s', host, src)
                continue
            dist = math.sqrt((host_xpos-src_xpos)**2 + (host_ypos-src_ypos)**2 + (host_zpos-src_zpos)**2)
            results.append((dist, rssi))
        fig = MyFig(options, legend=False, grid=False, xlabel=r'distance~$[m]$', ylabel=r'$Avr(RSSI)~[dBm]$', aspect='auto')
        fig.ax.set_ylim(-100, -30)
        X = [dist for dist, rssi in results]
        Y = [rssi for dist, rssi in results]
        fig.ax.scatter(X, Y)
        xfit = pylab.linspace(min(X), max(X), 100)
        ###
        #polycoeffs = list(scipy.polyfit(X, Y, 3))
        #yfit = polyval(polycoeffs, pylab.linspace(min(X), max(X), 100))
        #poly = coeffs2polystr(polycoeffs)
        #fig.ax.text(0.0, -35, '$f(x)=%s$' % poly, color='red', fontsize=options['fontsize']*0.8)
        #fig.ax.plot(xfit, yfit, color='red', lw=2)
        ################################################################################
        fitfunc = lambda p, x: p[0]/(map(lambda x: x**2, x) + p[1]) + p[2]
        errfunc = lambda p, x, y: fitfunc(p, x) - y
        p0 = [1, 1, -40]
        p1, success = scipy.optimize.leastsq(errfunc, p0[:], args=(X, Y))
        func = r'\frac{%.2f}{x^2 + %.2f} + %.2f' % (p1[0], p1[1], p1[2])
        fig.ax.text(20, -35, '$RSSI(m)=%s$' % func, color='red', fontsize=options['fontsize'])
        fig.ax.plot(xfit, fitfunc(p1, xfit), color='red', lw=2)
        fig.ax.set_xlim(0, 120)
        fig.ax.set_ylim(-100, -30)
        fig.save('over_distance_%d' % (helloSize))
        ################################################################################
        H, xedges, yedges = numpy.histogram2d(X, Y, bins=[40, 80], range=[[0, 120],[-100, -30]], normed=False, weights=None)
        H = H/len(X)
        mH = numpy.ma.masked_less_equal(H, 0.0)
        hist_list.append((mH, helloSize))
    logging.info('creating pseudo color plots')
    X, Y = pylab.meshgrid(xedges, yedges)
    vmax = max([mH.max() for mH, _helloSize in hist_list])
    for mH, helloSize in hist_list:
        fig_pcolor = MyFig(options, legend=False, grid=False, xlabel=r'distance~$[m]$', ylabel=r'$Avr(RSSI)~[dBm]$', aspect='auto')
        if options['grayscale']:
            pcolor = fig_pcolor.ax.pcolorfast(X, Y, numpy.transpose(mH), cmap=options['graycm'], vmin=0.0, vmax=vmax)
        else:
            pcolor = fig_pcolor.ax.pcolorfast(X, Y, numpy.transpose(mH), cmap=options['color'], vmin=0.0, vmax=vmax)
        cbar = fig_pcolor.colorbar(pcolor)
        fig_pcolor.save('over_distance_pcolor_%d' % (helloSize))

@requires_hellos
def plot_hello_rssi_normaltest(options, tags=None, cursor=None):
    options['prefix'] = 'rssi'
    ################################################################################
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        if options['neg_rssi']:
            cursor.execute('''
                SELECT he.tx_if, rh.rx_if, rh.rssi AS RSSI
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag AND he.seq = rh.seq AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND RSSI < 0
                AND RSSI >= -255
                ORDER BY tag.key, he.tx_if, rh.rx_if
            ''', (tag_key,))
        else:
            cursor.execute('''
                SELECT he.tx_if, rh.rx_if, rh.rssi AS RSSI
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag AND he.seq = rh.seq AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND RSSI <= 255
                AND RSSI > 0
                ORDER BY tag.key, he.tx_if, rh.rx_if
            ''', (tag_key,))
        data = dict()
        for s, d, r in cursor.fetchall():
            try:
                data[(s,d)].append(r)
            except KeyError:
                data[(s,d)] = [r]
        pvalues = list()
        pvalues = [stats.normaltest(v) for v in data.values() if len(v) >= 8]
        fig = MyFig(options, legend=False, grid=False, xlabel=r'p-value', ylabel=r'Fraction', aspect='auto')
        X, Y = data2hist([p for s, p in pvalues], bins=100, range=(0,1))
        fig.ax.plot(X, Y)
        fig.save('normaltest_%d' % (helloSize))

@requires_hellos
def plot_hello_rssi_std(options, tags=None, cursor=None):
    options['prefix'] = 'rssi'
    ################################################################################
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        if options['neg_rssi']:
            cursor.execute('''
                SELECT he.tx_if, rh.rx_if, rh.rssi
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag AND he.seq = rh.seq AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND rh.rssi >= -255
                AND rh.rssi < 0
                ORDER BY tag.key, he.tx_if, rh.rx_if
            ''', (tag_key,))
        else:
            cursor.execute('''
                SELECT he.tx_if, rh.rx_if, rh.rssi-255
                FROM tag JOIN he LEFT JOIN rh
                ON tag.key = he.tag AND he.seq = rh.seq AND he.tx_if = rh.prev
                WHERE tag.key = ?
                AND NOT he.host = rh.host
                AND rh.rssi <= 255
                AND rh.rssi > 0
                ORDER BY tag.key, he.tx_if, rh.rx_if
            ''', (tag_key,))
        data = dict()
        for s, d, r in cursor.fetchall():
            try:
                data[(s,d)].append(r)
            except KeyError:
                data[(s,d)] = [r]
        avr_hellos, = cursor.execute('''SELECT AVG(c) FROM (SELECT COUNT(*) AS c FROM he GROUP BY host, tag)''').fetchone()
        for _i, min_count in enumerate(numpy.arange(0.02, 0.11, 0.02)):
            logging.info('\tmin_count=%.2f (%d/%d)', min_count, _i+1, len(numpy.arange(0.02, 0.11, 0.02)))
            stds = [scipy.std(v) for v in data.values() if len(v) >= min_count*avr_hellos]
            means = [scipy.mean(v) for v in data.values() if len(v) >= min_count*avr_hellos]
            rssi_set = list(set(pylab.flatten(data.values())))

            fig = MyFig(options, legend=False, grid=False, xlabel=r'$RSSI Standard Deviation \sigma$', ylabel=r'Fraction', aspect='auto')
            X, Y = data2hist(stds, bins=10, range=(0,10))
            fig.ax.plot(X, Y)
            fig.ax.set_ylim(0,1)
            fig.ax.set_xlim(0,10)
            fig.save('std_%d_g%.2f' % (helloSize, min_count))

            fig = MyFig(options, legend=False, grid=False, xlabel=r'Mean RSSI $\mu$', ylabel=r'RSSI Standard Deviation $\sigma$', aspect='auto')
            fig.ax.scatter(means, stds)
            fig.save('mean_std_%d_g%.2f' % (helloSize, min_count))

        #fig = MyFig(options, legend=False, grid=False, xlabel=r'Mean RSSI $\mu$', ylabel=r'Standard deviation $\sigma$', aspect='auto')
        #X, Y = data2hist(rssi_set, bins=max(rssi_set)-min(rssi_set)+1)
        #fig.ax.plot(X, Y)
        #fig.ax.set_ylim(0,1)
        #fig.ax.set_xlim(min(rssi_set), max(rssi_set))
        #fig.ax.plot(X, Y)
        #fig.save('granularity_%d' % (helloSize))

@requires_hellos
def plot_hello_rssi_bidirectional(options, tags=None, cursor=None):
    options['prefix'] = 'rssi'
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)
    colors = options['color'](pylab.linspace(0, 1.0, len(tags)))
    linestyles = options['linestyles']*10
    ################################################################################
    fig_cdf = MyFig(options, legend=True, grid=False, xlabel=r'$Abs(RSSI_{Diff})~[dBm]$', ylabel=r'CDF', aspect='auto')
    fig_pdf = MyFig(options, legend=True, grid=False, xlabel=r'$Abs(RSSI_{Diff})~[dBm]$', ylabel=r'Fraction', aspect='auto')
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        if options['neg_rssi']:
            data = [d for d, in cursor.execute('''
                SELECT ABS(A.rssi - B.rssi)
                FROM eval_helloPDR as A JOIN eval_helloPDR as B
                ON A.host=B.src AND A.src=B.host
                WHERE A.tag_key=? AND B.tag_key=?
                AND A.rssi >= -255 AND A.rssi < 0
                AND B.rssi >= -255 AND B.rssi < 0
            ''', (tag_key, tag_key)).fetchall()]
        else:
            data = [d for d, in cursor.execute('''
                SELECT ABS(A.rssi - B.rssi)
                FROM eval_helloPDR as A JOIN eval_helloPDR as B
                ON A.host=B.src AND A.src=B.host
                WHERE A.tag_key=? AND B.tag_key=?
                AND A.rssi <= 255 AND A.rssi > 0
                AND B.rssi <= 255 AND B.rssi > 0
            ''', (tag_key, tag_key)).fetchall()]
        quantiles_all = stats.mstats.mquantiles(data, prob=quantiles)
        X, Y = data2hist(data, 35, (0, 35))
        fig_cdf.ax.plot(quantiles_all, quantiles, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
        fig_pdf.ax.plot(X, Y, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
    fig_cdf.legend_title = 'Packet sizes [bytes]'
    fig_cdf.save('bidirectional_cdf')
    fig_pdf.legend_title = 'Packet sizes [bytes]'
    fig_pdf.save('bidirectional_pdf')

@requires_hellos
def plot_hello_pdr_diff(options, tags=None, cursor=None):
    '''
    Plot how the PDR changes when the packet size is increased
    '''
    options['prefix'] = 'pdr'
    ################################################################################
    min_size_key, min_hello_size, = cursor.execute('''
        SELECT key, helloSize
        FROM tag
        WHERE helloSize = (
            SELECT MIN(helloSize) FROM tag
        )''').fetchone()

    min_int_key, max__size, = cursor.execute('''
        SELECT key, helloSize
        FROM tag
        WHERE helloSize = (
            SELECT MIN(helloSize) FROM tag
        )''').fetchone()

    tags = cursor.execute('''
        SELECT key, id, helloSize, hello_interval
        FROM tag
        WHERE NOT key=?
        ORDER BY helloSize, hello_interval
        ''', (min_size_key,)).fetchall()
    ################################################################################
    colors = options['color'](pylab.linspace(0, 1.0, len(tags)))
    linestyles = options['linestyles']
    ################################################################################
    fig_pdrdiff_abs = MyFig(options, legend=True, grid=False, xlabel=r'$Abs(PDR_{Diff}$)', ylabel='$CDF$', aspect='auto', legend_pos='lower right')
    fig_pdrdiff_rel  = MyFig(options, legend=True, grid=False, xlabel=r'$Rel(PDR_{Diff}$)', ylabel='$CDF$', aspect='auto', legend_pos='lower right')
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        pdr_diffs = cursor.execute('''
            SELECT A.src,  A.host, A.pdr-B.pdr, (A.pdr-B.pdr)/B.pdr
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host = B.host and A.src = B.src
            WHERE A.tag_key = ? AND B.tag_key = ?
        ''', (tag_key, min_size_key)).fetchall()
        pdr_diffAbs = [pdr_abs for _h, _s, pdr_abs, _pdr_rel in pdr_diffs]
        pdr_diffRel = [pdr_rel for _h, _s, _pdr_abs, pdr_rel in pdr_diffs]
        quantiles = pylab.linspace(0, 1, 100)
        values_abs = stats.mstats.mquantiles(pdr_diffAbs, prob=quantiles)
        values_rel = stats.mstats.mquantiles(pdr_diffRel, prob=quantiles)

        pos = stats.percentileofscore(pdr_diffAbs, 0)/100
        fig_pdrdiff_abs.ax.plot(values_abs, quantiles, linestyle=linestyles[j], label=r'$\alpha=%d, \beta=%d$' % (min_hello_size, helloSize), color=colors[j])
        fig_pdrdiff_abs.ax.plot([-1, 0], [pos, pos], linestyle='dashed', color=colors[j])

        pos = stats.percentileofscore(pdr_diffRel, 0)/100
        fig_pdrdiff_rel.ax.plot(values_rel, quantiles, linestyle=linestyles[j], label=r'$\alpha=%d, \beta=%d$' % (min_hello_size, helloSize), color=colors[j])
        fig_pdrdiff_rel.ax.plot([-1, 0], [pos, pos], linestyle='dashed', color=colors[j])

    for fig in [fig_pdrdiff_abs, fig_pdrdiff_rel]:
        fig.ax.set_autoscalex_on(False)
        fig.ax.set_autoscaley_on(False)
        fig.ax.axis((-1, 1, 0, 1))
    fig_pdrdiff_rel.legend_title = 'Packet sizes [bytes]'
    fig_pdrdiff_rel.save('difference_relative')

    fig_pdrdiff_abs.ax.plot([-0.1, -0.1], [0, 1], linestyle='dotted', color='black')
    fig_pdrdiff_abs.ax.plot([0.1, 0.1], [0, 1], linestyle='dotted', color='black')
    fig_pdrdiff_abs.legend_title = 'Packet sizes [bytes]'
    fig_pdrdiff_abs.save('difference_absolute')

@requires_hellos
def plot_hello_pdr_distance(options, tags=None, cursor=None):
    """
    Plot the PDR between each pair of adjacent nodes over the euclidean distance
    """
    options['prefix'] = 'pdr'
    ################################################################################
    locs = options['locs']
    colors = options['color'](pylab.linspace(0, 1.0, len(tags)))
    markers = options['markers']
    quantiles = pylab.linspace(0, 1, 100)
    ################################################################################
    tab_distance = r'\begin{table}[t]\centering\begin{tabular}{rrrrrr}\toprule' + '\n'
    tab_distance += r'Packet size [bytes] & Mode $[m]$ & $\mu~[m]$ & $\sigma~[m]$ & $\tilde{x}~[m]$ & $\gamma$ \\ \midrule' + '\n'
    ################################################################################
    fig_cdf_dist = MyFig(options, figsize=(10, 8), xlabel='Euclidean Distance [m]', ylabel='$CDF$', legend=True, grid=False, aspect='auto')
    fig_pdf_dist = MyFig(options, figsize=(10, 8), xlabel='Euclidean Distance [m]', ylabel='Fraction', legend=True, grid=False, aspect='auto')
    hist_list = list()
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        results = []
        results_ext = []
        results_ext_int = []
        cursor.execute('''
            SELECT src, host, pdr
            FROM eval_helloPDR
            WHERE tag_key=?
        ''', (tag_key,))
        for src, host, pdr in cursor.fetchall():
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
                src_xpos, src_ypos, src_zpos = locs[src]
            except KeyError:
                logging.warning('no position found for node %s or %s', host, src)
                continue
            dist = math.sqrt((host_xpos-src_xpos)**2 + (host_ypos-src_ypos)**2 + (host_zpos-src_zpos)**2)
            if src.find('ext') > -1 and host.find('ext') > -1:
                results_ext.append((dist, pdr))
            elif src.find('ext') > -1 or host.find('ext') > -1:
                results_ext_int.append((dist, pdr))
            else:
                results.append((dist, pdr))
        fig_pdrdist = MyFig(options, figsize=(10, 8), xlabel='Euclidean Distance [m]', ylabel='$PDR$', legend=True, aspect='auto')

        pdrs = [y[1] for y in results] + [y[1] for y in results_ext] + [y[1] for y in results_ext_int]
        dists = [x[0] for x in results] + [x[0] for x in results_ext] + [x[0] for x in results_ext_int]

        #quantiles_data = stats.mstats.mquantiles(dists, prob=pylab.linspace(0.0, 1.0, 20))

        def _plot_medians():
            median_pdr = scipy.median(pdrs)
            median_dist = scipy.median(dists)
            fig_pdrdist.ax.plot([-50, 200], [median_pdr, median_pdr], linestyle='dashed', color='black', label='Median')
            fig_pdrdist.ax.plot([median_dist, median_dist], [-0.2, 1.2], linestyle='dashed', color='black')
            return median_dist

        def _plot_means():
            mean_pdr = scipy.mean(pdrs)
            mean_dist = scipy.mean(dists)
            mean_dist_std = scipy.std(dists)
            fig_pdrdist.ax.plot([-50, 200], [mean_pdr, mean_pdr], linestyle='dotted', color='red', label='Mean')
            fig_pdrdist.ax.plot([mean_dist, mean_dist], [-0.2, 1.2], linestyle='dotted', color='red')
            return mean_dist, mean_dist_std

        def _plot_scatter():
            fig_pdrdist.ax.scatter([x[0] for x in results], [y[1] for y in results], marker=markers[0], s=15, color=colors[0], label='Indoor Link')
            fig_pdrdist.ax.scatter([x[0] for x in results_ext], [y[1] for y in results_ext], marker=markers[1], s=15, color=colors[len(colors)/2], label='Outdoor Link')
            fig_pdrdist.ax.scatter([x[0] for x in results_ext_int], [y[1] for y in results_ext_int], marker=markers[2], s=15, color=colors[-1], label='Indoor/Outdoor Link')

        def _plot_dist_cdf():
            """
            Actually plots a histogram at this moment!!!
            """
            dist_quantils = stats.mstats.mquantiles(dists, prob=quantiles)
            fig_cdf_dist.ax.plot(dist_quantils, quantiles, marker=markers[j], markevery=20, color=colors[j], label='$%d$' % helloSize)
            avr = scipy.average(dists)
            std = scipy.std(dists)
            norm_dist = stats.norm.cdf(dist_quantils, loc=avr, scale=std)
            X = sorted(list(set(dists)))
            pdf = stats.norm.pdf(X, loc=avr, scale=std)
            #skew = scipy.stats.skew(dists)
            #print avr, std, skew
            #X = pylab.linspace(0,120,1000)
            #Y = cdf_skew_norm(X=X, loc=avr, scale=std, a=-skew)
            #fig_cdf_dist.ax.plot(dist_quantils, norm_dist, linestyle='dotted', color='black')
            #fig_cdf_dist.ax.plot(X, Y, linestyle='dotted', color='black')
            #fig_pdf_dist.ax.plot(X, pdf, linestyle='dotted', color=colors[j])

            X, Y = data2hist(dists, bins=options['bins'], range=(0, 120))
            fig_pdf_dist.ax.plot(X, Y, linestyle='-', color=colors[j], alpha=0.5, label='$%d$' % helloSize)

        median_dist = _plot_medians()
        mean_dist, mean_dist_std = _plot_means()
        mode_dist = stats.mode(dists)[0][0]
        skew_dist = stats.skew(dists)
        tab_distance += r'%d & %.2f & %.2f & %.2f & %.2f & %.2f \\ \midrule' % (helloSize, mode_dist, mean_dist, mean_dist_std, median_dist, skew_dist) + '\n'
        _plot_scatter()
        _plot_dist_cdf()

        fig_pdrdist.ax.set_autoscalex_on(False)
        fig_pdrdist.ax.set_autoscaley_on(False)
        fig_pdrdist.ax.set_ylim(-0.05, 1.05)
        fig_pdrdist.ax.set_xlim(-5, 130)
        fig_pdrdist.save('distance_%d' % (helloSize))
        ########################################################################
        H, xedges, yedges = numpy.histogram2d(dists, pdrs, bins=[40, 40], range=[[0, 120],[0, 1]], normed=False, weights=None)
        H = H/len(dists)
        mH = numpy.ma.masked_less_equal(H, 0.0)
        hist_list.append((mH, helloSize))
    fig_pdf_dist.legend_title = 'Packet size [bytes]'
    fig_cdf_dist.legend_title = 'Packet size [bytes]'
    options['prefix'] = 'dist'
    fig_pdf_dist.save('pdf')
    fig_cdf_dist.save('cdf')
    tab_distance += r'\end{tabular}'
    tab_distance += r'\caption{Central moments of the link ranges distributions on channel}' + '\n'
    tab_distance += r'\label{tab:distance}' + '\n'
    tab_distance += r'\end{table}'  + '\n'
    options['latex_file'].write(tab_distance)

    options['prefix'] = 'pdr'
    logging.info('creating pseudo color plots')
    X, Y = pylab.meshgrid(xedges, yedges)
    vmax = max([mH.max() for mH, _helloSize in hist_list])
    for mH, helloSize in hist_list:
        fig_pcolor = MyFig(options, figsize=(10, 7), legend=False, grid=False, xlabel=r'distance~$[m]$', ylabel=r'$PDR$', aspect='auto')
        pcolor = fig_pcolor.ax.pcolor(X, Y, numpy.transpose(mH), cmap=options['color'], vmin=0.0, vmax=vmax)
        cbar = fig_pcolor.colorbar(pcolor)
        fig_pcolor.save('distance_pcolor_%d' % (helloSize))


@requires_hellos
def plot_hello_pdr_asymmetry(options, tags=None, cursor=None):
    """
    Plot the CDR of the link asymmetry. Asymmetry is defined as the absolute
    difference of the PDRs in both directions. Unidirectional links are not
    considered asymmetric!
    """
    options['prefix'] = 'pdr'
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(tags)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(tags)))
    linestyles = options['linestyles']*10
    ################################################################################
    xlabel = r'$\PDR{_{\asymp}}$'
    fig_cdf_asym = MyFig(options, xlabel=xlabel, ylabel='$CDF$', legend=True, grid=False)
    fig_hist_asym = MyFig(options, figsize=(10, 8), xlabel=xlabel, ylabel='Fraction $f_i$', legend=True, grid=False, aspect='auto')
    fig_hist_asym_pcolor = MyFig(options, figsize=(10, 7), xlabel=xlabel, ylabel='Packet Size [bytes]', grid=False, aspect='auto')
    fig_hist_asym_surface = MyFig(options, figsize=(10, 7), xlabel=xlabel, ylabel=r'Packet Size [bytes]', zlabel='Fraction of Links', grid=False, aspect='auto', ThreeD=True)
    surface_plots = list()
    data = list()
    max_z = 0
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        fig_scatter_asym = MyFig(options, xlabel=r'$\PDR(e_{a,b})$', ylabel=r'$\PDR(e_{b,a})$', grid=False)
        fig_pcolor_asym = MyFig(options, xlabel=r'$\PDR(e_{a,b})$', ylabel=r'$\PDR(e_{b,a})$', grid=False)
        fig_surface_asym = MyFig(options, xlabel=r'$\PDR(e_{a,b})$', ylabel=r'$\PDR(e_{b,a})$', grid=False, ThreeD=True)
        cursor.execute('''
            SELECT A.host, B.host, A.pdr, B.pdr
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host=B.src AND A.src=B.host
            WHERE A.tag_key=? AND B.tag_key=?
        ''', (tag_key, tag_key))
        t = cursor.fetchall()
        pdrdiffs = [abs(a_pdr-b_pdr) for h1, h2, a_pdr, b_pdr in t]
        data.append((helloSize, pdrdiffs))
        quantiles_all = stats.mstats.mquantiles(pdrdiffs, prob=quantiles)
        fig_cdf_asym.ax.plot(quantiles_all, quantiles, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
        X, Y = data2hist(pdrdiffs, bins=pylab.linspace(0, 1, options['bins']+1))
        fig_hist_asym.ax.plot(X, Y, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
        fig_scatter_asym.ax.scatter([a for h1, h2, a, b in t], [b for h1, h2, a, b in t], marker='x', color=colors[j])
        fig_scatter_asym.ax.set_xlim(-0.05, 1.05)
        fig_scatter_asym.ax.set_ylim(-0.05, 1.05)
        fig_scatter_asym.save('asymmetry_scatter_%d' % (helloSize))

        t = [(max(a, b), min(a, b)) for h1, h2, a, b in t if h1 < h2]
        H, xedges, yedges = numpy.histogram2d(numpy.array([a for a, b in t]), numpy.array([b for a, b in t]), bins=options['bins'])
        H = H/len(t)
        # make the plot symmetric
        for n, r in enumerate(H):
            for m, c in enumerate(r):
                if m >= n:
                    break
                H[m, n] = H[n, m]
        X, Y = pylab.meshgrid(xedges, yedges)
        mH = numpy.ma.masked_less_equal(H, 0.0)
        pcolor = fig_pcolor_asym.ax.pcolorfast(X, Y, mH, cmap=options['color'], vmin=0.0)#, vmax=0.075)
        cbar = fig_pcolor_asym.colorbar(pcolor)
        fig_pcolor_asym.save('asymmetry_pcolor_%d' % (helloSize))

        fig_surface_asym.ax.plot_surface(X, Y, H, rstride=1, cstride=1, linewidth=1, antialiased=True, color=colors[j])
        surface_plots.append((fig_surface_asym, 'asymmetry_surface_%d' % (helloSize)))
        max_z = max(max_z, numpy.max(H))
    for scatter_plot, filename in surface_plots:
        scatter_plot.ax.set_zlim3d(0, max_z)
        scatter_plot.save(filename)

    fig_cdf_asym.ax.set_autoscalex_on(False)
    fig_cdf_asym.ax.set_autoscaley_on(False)
    fig_cdf_asym.legend_title = 'Packet size [bytes]'
    fig_cdf_asym.save('asymmetry_cdf')
    fig_hist_asym.legend_title = 'Packet size [bytes]'
    fig_hist_asym.save('asymmetry_hist')

    if len(set(s for (s, pdrs) in data)) > 1:
        data2pcolor(fig_hist_asym_pcolor, data, options, range=(0.0, 1.0), masked=False, fast=True)
        fig_hist_asym_pcolor.save('asymmetry_hist_pcolor')
        data2surface(fig_hist_asym_surface, data, options, range=(0.0, 1.0))
        fig_hist_asym_surface.save('asymmetry_hist_surface')

@requires_hellos
def plot_hello_rssi_asymmetry(options, tags=None, cursor=None):
    """
    Plot the CDR of the link asymmetry. Asymmetry is defined as the absolute
    difference of the PDRs in both directions. Unidirectional links are not
    considered asymmetric!
    """
    options['prefix'] = 'rssi'
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)
    colors = options['color'](pylab.linspace(0, 1.0, len(tags)))
    linestyles = options['linestyles']*10
    ################################################################################
    fig_cdf_asym = MyFig(options, xlabel=r'$RSSI_{Asym}$', ylabel='$CDF$', legend=True, grid=False)
    fig_hist_asym = MyFig(options, xlabel=r'$RSSI_{Asym}$', ylabel='Fraction $f_i$', legend=True, grid=False, aspect='auto')
    surface_plots = list()
    max_z = 0
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        fig_scatter_asym = MyFig(options, xlabel=r'$RSSI(e_{a,b})$', ylabel='$RSSI(e_{b,a})$', grid=False)
        fig_pcolor_asym = MyFig(options, xlabel=r'$RSSI(e_{a,b})$', ylabel='$RSSI(e_{b,a})$', grid=False)
        fig_surface_asym = MyFig(options, xlabel=r'$RSSI(e_{a,b})$', ylabel='$RSSI(e_{b,a})$', grid=False, ThreeD=True)
        cursor.execute('''
            SELECT A.host, B.host, A.rssi, B.rssi
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host=B.src AND A.src=B.host
            WHERE A.tag_key=? AND B.tag_key=?
        ''', (tag_key, tag_key))
        t = cursor.fetchall()
        pdrdiffs = [abs(a_pdr-b_pdr) for h1, h2, a_pdr, b_pdr in t]
        quantiles_all = stats.mstats.mquantiles(pdrdiffs, prob=quantiles)
        fig_cdf_asym.ax.plot(quantiles_all, quantiles, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
        X, Y = data2hist(pdrdiffs, bins=pylab.linspace(0, 1, options['bins']+1))
        fig_hist_asym.ax.plot(X, Y, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)
        fig_scatter_asym.ax.scatter([a for h1, h2, a, b in t], [b for h1, h2, a, b in t], marker='x', color=colors[j])
        fig_scatter_asym.ax.set_xlim(-110, -30)
        fig_scatter_asym.ax.set_ylim(-110, -30)
        fig_scatter_asym.save('asymmetry_scatter_%d' % (helloSize))

        t = [(max(a, b), min(a, b)) for h1, h2, a, b in t if h1 < h2]
        H, xedges, yedges = numpy.histogram2d(numpy.array([a for a, b in t]), numpy.array([b for a, b in t]), bins=options['bins'], range=[[-110, -30], [-110, -30]])
        H = H/len(t)
        # make the plot symmetric
        for n, r in enumerate(H):
            for m, c in enumerate(r):
                if m >= n:
                    break
                H[m, n] = H[n, m]
        X, Y = pylab.meshgrid(xedges, yedges)
        mH = numpy.ma.masked_less_equal(H, 0.0)
        pcolor = fig_pcolor_asym.ax.pcolor(X, Y, mH, cmap=options['color'], vmin=0.0)#, vmax=0.075)
        cbar = fig_pcolor_asym.colorbar(pcolor)
        fig_pcolor_asym.save('asymmetry_pcolor_%d' % (helloSize))

        fig_surface_asym.ax.plot_surface(X, Y, mH, rstride=1, cstride=1, linewidth=1, antialiased=True, color=colors[j])
        surface_plots.append((fig_surface_asym, 'asymmetry_surface_%d' % (helloSize)))
        max_z = max(max_z, numpy.max(H))
    for surface_plot, filename in surface_plots:
        surface_plot.ax.set_zlim3d(0, max_z)
        surface_plot.save(filename)

    fig_cdf_asym.ax.set_autoscalex_on(False)
    fig_cdf_asym.ax.set_autoscaley_on(False)
    fig_cdf_asym.legend_title = 'Packet size [bytes]'
    fig_cdf_asym.save('asymmetry_cdf')
    fig_hist_asym.legend_title = 'Packet size [bytes]'
    fig_hist_asym.save('asymmetry_hist')

@requires_hellos
def plot_hello_links(options, tags=None, cursor=None):
    """
    Plot total number of links for each helloSize
    """
    options['prefix'] = 'links'
    ################################################################################
    min_pdrs = pylab.linspace(0, 0.1, 6)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(min_pdrs)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(min_pdrs)))
    markers = options['markers']
    linestyles = options['linestyles']
    ################################################################################
    fig_links = MyFig(options, figsize=(10, 8), xlabel='Packet Size [bytes]', ylabel='Number of Links $E$', legend=True, aspect='auto', grid=False)
    for i, min_pdr in enumerate(min_pdrs):
        values = []
        for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
            logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
            allLinks, = cursor.execute('''
                SELECT COUNT(src)
                FROM eval_helloPDR
                WHERE tag_key=? AND pdr >= ?
            ''', (tag_key, min_pdr)).fetchone()
            values.append(allLinks)
        fig_links.ax.plot(range(0, len(values), 1), values, linestyle=linestyles[i], label='$G_{PDR \geq %.2f}$' % min_pdr, color=colors[i])
    fig_links.legend_title = 'Subgraph'
    sizes = sorted(list(set([helloSize for _tag_key, _tag_id, helloSize, _helloInterval in tags])))
    intervals = sorted(list(set([helloInterval for _tag_key, _tag_id, _helloSize, helloInterval in tags])))

    assert((len(sizes) == 1 and len(intervals)>1) or (len(sizes) > 1 and len(intervals) == 1))
    if len(sizes) > 1:
        xlabels = sizes
    else:
        xlabels = intervals
        fig_links.ax.set_xlabel('Packet Interval [ms]')

    if len(xlabels) > 10:
        xlabels = list(pylab.flatten([[x, '', '', ''] for x in xlabels[::4]]))
        xticks = range(0, len(values), 1)[::4]
    else:
        xticks = range(0, len(values), 1)
    fig_links.ax.set_xticks(xticks)
    fig_links.ax.set_xticklabels(xlabels)
    fig_links.ax.set_ylim(0, 2000)
    fig_links.ax.set_yticks(range(500, 2001, 500))
    fig_links.save('count')

@requires_hellos
def plot_hello_links_uni(options, tags=None, cursor=None):
    """
    Plot fraction of unidirectional links for each helloSize
    """
    options['prefix'] = 'links'
    ################################################################################
    min_pdrs = pylab.linspace(0, 0.1, 6)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(min_pdrs)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(min_pdrs)))
    markers = options['markers']
    linestyles = options['linestyles']
    ################################################################################
    ylabel = r'Fraction of Unidirectional Links $\nicefrac{E_{\text{uni}}}{E}$'
    fig_cdf_all = MyFig(options, figsize=(10, 8), xlabel='Packet Size [bytes]', ylabel=ylabel, legend=True, aspect='auto', grid=False)
    fig_frac_uni_b = MyFig(options, figsize=(10, 8), xlabel='Packet Size [bytes]', ylabel=ylabel, legend=True, grid=False, aspect='auto', legend_pos='upper left')
    for i, min_pdr in enumerate(min_pdrs):
        values = []
        values2 = []
        for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
            logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
            bidirectionalLinks, = cursor.execute('''
                SELECT COUNT(A.src)
                FROM eval_helloPDR as A LEFT JOIN eval_helloPDR as B
                ON A.host=B.src AND B.host=A.src
                WHERE A.tag_key=? AND B.tag_key=? AND
                A.pdr >= ? AND B.pdr >= ?
            ''', (tag_key, tag_key, min_pdr, min_pdr)).fetchone()
            badLinks, = cursor.execute('''
                SELECT COUNT(A.src)
                FROM eval_helloPDR as A LEFT JOIN eval_helloPDR as B
                ON A.host=B.src AND B.host=A.src
                WHERE A.tag_key=? AND B.tag_key=? AND
                A.pdr < ? AND B.pdr < ?
            ''', (tag_key, tag_key, min_pdr, min_pdr)).fetchone()
            allLinks, = cursor.execute('''
                SELECT COUNT(src)
                FROM eval_helloPDR
                WHERE tag_key=?
            ''', (tag_key, )).fetchone()
            values.append(float(allLinks - bidirectionalLinks)/allLinks)
            values2.append(float(allLinks - bidirectionalLinks - badLinks)/allLinks)
        fig_cdf_all.ax.plot(range(0, len(values), 1), values, ls=linestyles[i], marker=markers[i], label='$G_{bi,\PDR{\geq %.2f}}$' % min_pdr, color=colors[i])
        fig_frac_uni_b.ax.plot(range(0, len(values2), 1), values2, ls=linestyles[i], label='$G_{bi,\PDR{\geq %.2f}}$' % min_pdr, color=colors[i])
        #print min_pdr, min(values), max(values)

    for fig in [fig_cdf_all, fig_frac_uni_b]:
        fig.legend_title = 'Subgraph'
        fig.ax.set_xticks(range(0, len(values), 1))
        fig.ax.set_xticklabels(['$%s$'% helloSize for _tag_key, _tag_id, helloSize, _helloInterval in tags])
        #fig.ax.axis((-0.5, len(values)-0.5, 0, 1))
        fig.ax.set_ylim(0, 0.5)
    fig_frac_uni_b.ax.set_yticks(numpy.arange(0.1, 0.6, 0.1))
    fig_cdf_all.save('frac_uni_over_size')
    fig_frac_uni_b.save('frac_uni_over_size_without_bad_links')

@requires_hellos
def plot_hello_degree(options, tags=None, cursor=None):
    """
    Plot the CDF of the node degrees. Additionally, the average node degree
    is depicted.
    """
    options['prefix'] = 'degree'
    ################################################################################
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(tags)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(tags)))
    linestyles = options['linestyles']*100
    ################################################################################
    fig_cdf_all = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='$CDF$', legend=True, grid=False, aspect='auto')
    fig_hist_all = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='Fraction of Links', legend=True, grid=False, aspect='auto')
    fig_qq_all = MyFig(options, figsize=(10, 8), xlabel='Node Degree Distribution', ylabel='Fitted Gamma Distribution', legend=True, grid=False, aspect='auto')
    fig_qq_all.ax.plot([0, 50], [0, 50], linestyle='dotted', color=matplotlib.rcParams['grid.color'], linewidth=matplotlib.rcParams['grid.linewidth'])
    fig_qq_erlang = MyFig(options, figsize=(10, 8), xlabel='Node Degree Distribution', ylabel='Fitted Erlang Distribution', legend=True, grid=False, aspect='auto')
    fig_qq_erlang.ax.plot([0, 50], [0, 50], linestyle='dotted', color=matplotlib.rcParams['grid.color'], linewidth=matplotlib.rcParams['grid.linewidth'])
    fig_gamma_pdf = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='Probability Density', legend=True, grid=False, aspect='auto')
    fig_gamma_cdf = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='Cumulative Distribution', legend=True, grid=False, aspect='auto')
    fig_degree_cleaned = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='$CDF$', legend=True, grid=False, aspect='auto')
    fig_pcolor_g00 = MyFig(options, figsize=(10, 7), xlabel='Node Degree', ylabel='Packet Size [bytes]', grid=False, aspect='auto')
    fig_pcolor_g01 = MyFig(options, figsize=(10, 7), xlabel='Node Degree', ylabel='Packet Size [bytes]', grid=False, aspect='auto')

    tmax_degree = 0
    pdr_filter = 0.1
    degrees_g00 = list()
    degrees_g01 = list()
    tab_degree_fit  = r'\begin{table}[t]\centering\begin{tabular}{rrrr}\toprule' + '\n'
    tab_degree_fit += r'Packet Size [bytes] & Shape~$\alpha$ & Location~$\mu$ & Scale~$\beta$ \\ \midrule' + '\n'
    tab_degree = r'\begin{table}[t]\centering\begin{tabular}{rlrrrrrr}\toprule' + '\n'
    tab_degree += r'Packet Size [bytes] & $G$ & Max & Mode & $\mu$  & $\sigma$ & $\tilde{d_G}$ & $\gamma$ \\ \midrule' + '\n'

    chisquare_test_gamma = list()
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        cursor.execute('''
            SELECT host, COUNT(src)
            FROM eval_helloPDR
            WHERE tag_key=?
            GROUP BY host
        ''', (tag_key,))
        degrees = [x[1] for x in cursor.fetchall()]
        cursor.execute('''
            SELECT host
            FROM addr
            WHERE host NOT IN (
                SELECT DISTINCT(host)
                FROM eval_helloPDR
                WHERE tag_key=?
            )
        ''', (tag_key, ))
        isolated = [n for n, in cursor.fetchall()]
        degrees.extend([0 for n in isolated])

        cursor.execute('''
            SELECT host, COUNT(src)
            FROM eval_helloPDR
            WHERE tag_key=? AND pdr >= ?
            GROUP BY host
        ''', (tag_key, pdr_filter))
        degrees_cleaned = [x[1] for x in cursor.fetchall()]
        cursor.execute('''
            SELECT host
            FROM addr
            WHERE host NOT IN (
                SELECT DISTINCT(host)
                FROM eval_helloPDR
                WHERE tag_key=? AND pdr >= ?
            )
        ''', (tag_key, pdr_filter))
        isolated = [n for n, in cursor.fetchall()]
        degrees_cleaned.extend([0 for n in isolated])

        max_degree = max(degrees)
        min_degree = min(degrees)
        degrees_mean = scipy.mean(degrees)
        degrees_std = scipy.std(degrees)
        degrees_median = scipy.median(degrees)
        degrees_mode = stats.mode(degrees)[0][0]
        degrees_skew = stats.skew(degrees)

        degrees_max2 = max(degrees_cleaned)
        degrees_min2 = min(degrees_cleaned)
        degrees_mean2 = scipy.mean(degrees_cleaned)
        degrees_std2 = scipy.std(degrees_cleaned)
        degrees_median2 = scipy.median(degrees_cleaned)
        degrees_mode2 = stats.mode(degrees_cleaned)[0][0]
        degrees_skew2 = stats.skew(degrees_cleaned)

        tab_degree += r'\multirow{2}{*}{%s}' % (helloSize)
        tab_degree += r'& $G_{PDR>0}$ & %d & %d & %.2f & %.2f & %.1f & %.2f \\ \cmidrule(r){2-8}' % (max_degree, degrees_mode, degrees_mean, degrees_std, degrees_median, degrees_skew) + '\n'
        tab_degree += r'& $G_{PDR\geq 0.1}$ & %d & %d & %.2f & %.2f & %.1f & %.2f \\ \midrule' % (degrees_max2, degrees_mode2, degrees_mean2, degrees_std2, degrees_median2, degrees_skew2) + '\n'

        hist, bin_edges = numpy.histogram(degrees, bins=max_degree-min_degree+1, normed=False)
        hist = numpy.cumsum(hist/float(len(degrees)))
        xs = range(min_degree, max_degree+1)
        fig_cdf_all.ax.plot(xs, hist, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])
        fig_cdf_all.ax.plot([degrees_mean, degrees_mean], [0, 1], color=colors[j], linestyle='dashed')

        X, Y = data2hist(degrees, bins=options['bins'], range=(0, 45))
        fig_hist_all.ax.plot(X, Y, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])

        quantiles = pylab.linspace(0, 1, 10000)
        quantiles_data = scipy.stats.mstats.mquantiles(degrees, quantiles)

        gamma_fit_shape, gamma_fit_loc, gamma_fit_scale = scipy.stats.gamma.fit(degrees)
        quantiles_gamma = scipy.stats.gamma.ppf(quantiles, gamma_fit_shape, gamma_fit_loc, gamma_fit_scale)
        fig_qq_all.ax.plot(quantiles_data, quantiles_gamma, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])

        erlang_fit_shape, erlang_fit_loc, erlang_fit_scale = scipy.stats.erlang.fit(degrees)
        quantiles_erlang = scipy.stats.erlang.ppf(quantiles, erlang_fit_shape, erlang_fit_loc, erlang_fit_scale)
        fig_qq_erlang.ax.plot(quantiles_data, quantiles_erlang, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])

        tab_degree_fit += r'%d & %.3f & %.3f & %.3f \\' % (helloSize, gamma_fit_shape, gamma_fit_loc, gamma_fit_scale)
        tab_degree_fit += '\n'

        x = pylab.linspace(0, 43, 100)
        fig_gamma_pdf.ax.plot(x, scipy.stats.gamma.pdf(x, gamma_fit_shape, gamma_fit_loc, gamma_fit_scale), color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])
        fig_gamma_cdf.ax.plot(scipy.stats.gamma.ppf(quantiles, gamma_fit_shape, gamma_fit_loc, gamma_fit_scale), quantiles, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])

        fig_cdf = MyFig(options, figsize=(10, 8), xlabel='Node Degree', ylabel='Cumulative Distribution', legend=True, grid=False, aspect='auto')
        fig_cdf.ax.plot(scipy.stats.gamma.ppf(quantiles, gamma_fit_shape, gamma_fit_loc, gamma_fit_scale), quantiles, color=colors[0], label='statistical', linestyle='dashed')

        points = zip(scipy.stats.gamma.ppf(quantiles, gamma_fit_shape, gamma_fit_loc-2, gamma_fit_scale), quantiles)[1:-1]
        points += zip(scipy.stats.gamma.ppf(quantiles, gamma_fit_shape, gamma_fit_loc+2, gamma_fit_scale), quantiles)[1:-1][::-1]

        poly = Polygon(points, edgecolor='black', facecolor=colors[0], closed=True)
        patch_collection = PatchCollection([poly], match_original=True)
        patch_collection.set_alpha(0.3)
        patch_collection.set_linestyle('dashed')
        fig_cdf.ax.add_collection(patch_collection)

        fig_cdf.ax.plot(xs, hist, color=colors[-1], label='empirical', linestyle='solid')
        fig_cdf.legend_title = 'Packet size [bytes]'
        fig_cdf.save('cdf-%d' % helloSize)

        hist_cleaned, bin_edges = numpy.histogram(degrees_cleaned, bins=degrees_max2 -degrees_min2+1, normed=False)
        hist_cleaned = numpy.cumsum(hist_cleaned/float(len(degrees_cleaned)))
        xs_cleaned = range(degrees_min2, degrees_max2+1)
        fig_degree_cleaned.ax.plot(xs_cleaned, hist_cleaned, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])
        fig_degree_cleaned.ax.plot([degrees_mean2, degrees_mean2], [0, 1], color=colors[j], linestyle='dashed')

        degrees_g00.append((helloSize, degrees))
        degrees_g01.append((helloSize, degrees_cleaned))
        tmax_degree = max(tmax_degree, max_degree)

    tab_degree += r'\end{tabular}'
    tab_degree += r'\caption{Standardized moments of the node degree distributions on channel~XXX}' + '\n'
    tab_degree += r'\label{tab:node_degree}' + '\n'
    tab_degree += r'\end{table}'  + '\n'
    options['latex_file'].write(tab_degree)

    tab_degree_fit += r'\bottomrule' + '\n'
    tab_degree_fit += r'\end{tabular}' + '\n'
    tab_degree_fit += r'\caption{Parameters of the fitted gamma distributions.}' + '\n'
    tab_degree_fit += r'\label{tab:node_degree}' + '\n'
    tab_degree_fit += r'\end{table}'  + '\n'
    print tab_degree_fit

    for c in chisquare_test_gamma:
        print c

    fig_cdf_all.ax.axis((0, tmax_degree, 0, 1))
    fig_cdf_all.legend_title = 'Packet size [bytes]'
    fig_cdf_all.save('cdf')

    fig_hist_all.ax.axis((0, tmax_degree, 0, 1))
    fig_hist_all.legend_title = 'Packet size [bytes]'
    fig_hist_all.save('hist')

    fig_degree_cleaned.ax.axis((0, tmax_degree, 0, 1))
    fig_degree_cleaned.legend_title = 'Packet size [bytes]'
    fig_degree_cleaned.save('cleaned')

    #fig_qq_all.ax.axis((0, 1, 0, 1))
    fig_qq_all.legend_title = 'Packet size [bytes]'
    fig_qq_all.save('qq-gamma')

    fig_qq_erlang.legend_title = 'Packet size [bytes]'
    fig_qq_erlang.save('qq-erlang')

    fig_gamma_pdf.legend_title = 'Packet size [bytes]'
    fig_gamma_pdf.save('gamma-pdf')

    fig_gamma_cdf.legend_title = 'Packet size [bytes]'
    fig_gamma_cdf.save('gamma-cdf')

    if len(set(s for s, d in degrees_g00)) > 1:
        data2pcolor(fig_pcolor_g00, degrees_g00, options, masked=False, range=(0, tmax_degree))#, overwrite_bins=tmax_degree)
        fig_pcolor_g00.legend_title = 'Packet size [bytes]'
        fig_pcolor_g00.save('pcolor_g00')

    if len(set(s for s, d in degrees_g01)) > 1:
        data2pcolor(fig_pcolor_g01, degrees_g01, options, masked=False, range=(0, tmax_degree))#, overwrite_bins=tmax_degree)
        fig_pcolor_g01.legend_title = 'Packet size [bytes]'
        fig_pcolor_g01.save('pcolor_g01')

@requires_hellos
def plot_hello_pdr(options, tags=None, cursor=None):
    """
    Plot the CDF of the link PDR
    """
    options['prefix'] = 'pdr'
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(tags)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(tags)))
    markers = options['markers']*100
    linestyles=options['linestyles']*100

    num_sizes = len(set(cursor.execute('''
        SELECT DISTINCT(helloSize)
        FROM tag
    ''').fetchall()))

    num_intervals = len(set(cursor.execute('''
        SELECT DISTINCT(hello_interval)
        FROM tag
    ''').fetchall()))

    assert(num_sizes == 1 or num_intervals == 1)
    metric = 'Packet Size'
    unit = '[bytes]'
    if num_intervals > 1:
        metric = 'Packet Interval'
        unit = '[$ms$]'
    ################################################################################
    fig_cdf_all = MyFig(options, xlabel='$PDR$', ylabel='$CDF$', legend=True, grid=False)
    fig_hist_all = MyFig(options, figsize=(10, 8), xlabel='$PDR$', ylabel='Fraction', legend=True, grid=False, aspect='auto')
    fig_beta_cdf = MyFig(options, figsize=(10, 8), xlabel='$PDR$', ylabel='$CDF$', legend=True, grid=False, aspect='auto')
    fig_beta_pdf = MyFig(options, figsize=(10, 8), xlabel='$PDR$', ylabel='$PDF$', legend=True, grid=False, aspect='auto')
    fig_qq = MyFig(options, figsize=(10, 8), xlabel='$PDR$ Distribution', ylabel='Fitted Beta Distribution', legend=True, grid=False, aspect='auto')
    fig_qq.ax.plot([0, 1], [0, 1], linestyle='dotted', color=matplotlib.rcParams['grid.color'], linewidth=matplotlib.rcParams['grid.linewidth'])
    fig_hist_all_pcolor = MyFig(options, figsize=(10, 7), xlabel='$PDR$', ylabel='%s %s' % (metric, unit), grid=False, aspect='auto')
    fig_hist_all_surface = MyFig(options, figsize=(10, 7), xlabel=r'$PDR$', ylabel=r'Packet Size', zlabel='Fraction of Links', grid=False, aspect='auto', ThreeD=True)
    fig_cdf_bi = MyFig(options, xlabel='$PDR$', ylabel='$CDF$', legend=True, grid=False)

    tab_degree_fit  = r'\begin{table}[t]\centering\begin{tabular}{rrrrr}\toprule' + '\n'
    tab_degree_fit += r'Packet Size [bytes] & Shape~$a$ & Shape~$b$ & Location~$\mu$ & Scale~$s$ \\ \midrule' + '\n'

    data = list()
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        metric_val = helloSize
        if metric == 'Packet Interval':
            metric_val = helloInterval
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        cursor.execute('''
            SELECT pdr
            FROM eval_helloPDR
            WHERE tag_key=?
        ''', (tag_key,))
        pdrs = [x[0] for x in cursor.fetchall()]
        quantiles_all = stats.mstats.mquantiles(pdrs, prob=quantiles)
        fig_cdf_all.ax.plot(quantiles_all, quantiles, color=colors[j], label='$%d$' % metric_val, linestyle=linestyles[j])

        X, Y = data2hist(pdrs, bins=options['bins'], range=(0, 1))
        fig_hist_all.ax.plot(X, Y, color=colors[j], label='$%d$' % metric_val, linestyle=linestyles[j])
        data.append((metric_val, pdrs))
        a, b, loc, scale = scipy.stats.beta.fit(pdrs)
        quantiles = pylab.linspace(0, 1, 10000)
        x = pylab.linspace(0, 1, 100)[1:-1]
        fig_beta_pdf.ax.plot(x, scipy.stats.beta.pdf(x, a, b, loc, scale), color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])
        fig_beta_cdf.ax.plot(scipy.stats.beta.ppf(quantiles, a, b, loc, scale), quantiles, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])
        tab_degree_fit += r'%d & %.3f & %.3f & %.3f & %.3f \\' % (helloSize, a, b, loc, scale)
        tab_degree_fit += '\n'
        quantiles_beta = scipy.stats.beta.ppf(quantiles, a, b, loc, scale)
        quantiles_data = scipy.stats.mstats.mquantiles(pdrs, quantiles)
        fig_qq.ax.plot(quantiles_data, quantiles_beta, color=colors[j], label='$%d$' % helloSize, linestyle=linestyles[j])

        cursor.execute('''
            SELECT A.pdr
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host=B.src AND A.src=B.host
            WHERE A.tag_key=? AND B.tag_key=?
        ''', (tag_key, tag_key))
        pdrsbi = [x[0] for x in cursor.fetchall()]
        quantiles_bi = stats.mstats.mquantiles(pdrsbi, prob=quantiles)
        fig_cdf_bi.ax.plot(quantiles_bi, quantiles, color=colors[j], label='$%d$' % metric_val, linestyle=linestyles[j])

    tab_degree_fit += r'\bottomrule' + '\n'
    tab_degree_fit += r'\end{tabular}' + '\n'
    tab_degree_fit += r'\caption{Parameters of the fitted gamma distributions.}' + '\n'
    tab_degree_fit += r'\label{tab:node_degree}' + '\n'
    tab_degree_fit += r'\end{table}'  + '\n'
    print tab_degree_fit

    fig_cdf_bi.legend_title = '%s %s' % (metric, unit)
    fig_cdf_bi.save('cdf_bidirectional')
    fig_cdf_all.legend_title = '%s %s' % (metric, unit)
    fig_cdf_all.save('cdf_all')
    fig_hist_all.legend_title = '%s %s' % (metric, unit)
    fig_hist_all.save('hist')
    fig_qq.legend_title = '%s %s' % (metric, unit)
    fig_qq.save('qq')
    fig_beta_cdf.legend_title = '%s %s' % (metric, unit)
    fig_beta_cdf.save('fig_beta_cdf')
    fig_beta_pdf.legend_title = '%s %s' % (metric, unit)
    fig_beta_pdf.save('fig_beta_pdf')

    if len(set(s for (s, pdrs) in data)) > 1:
        data2pcolor(fig_hist_all_pcolor, data, options, range=(0.0, 1.0))
        fig_hist_all_pcolor.save('hist_pdr_pcolor')
        data2surface(fig_hist_all_surface, data, options, range=(0.0, 1.0))
        fig_hist_all_surface.save('hist_pdr_surface')

def data2surface(fig, data, options, range=(0.0, 1.0), overwrite_bins=None):
        """
        data := [(yticklabel, sample)]
        """
        if overwrite_bins:
            bins = overwrite_bins
        else:
            bins = options['bins']
        array = numpy.zeros((len(data), bins))
        for i, (s, pdrs) in enumerate(data):
            hist, _bin_edges = numpy.histogram(pdrs, bins=bins, range=range, normed=False)
            hist_norm = hist/float(len(pdrs))
            array[i] = hist_norm

        #x = pylab.linspace(0.0, 1.0, bins+1)
        #y = numpy.array([s for s, Y in data])
        #y = numpy.hstack((y, y[-1]+(y[1]-y[0])))
        #X, Y = pylab.meshgrid(x, y)

        x = pylab.linspace(0.0, 1.0, bins+1)
        x = x.repeat(2)[1:-1]

        yticks = pylab.linspace(0.0, 1.0, len(data)+1)
        yticks = numpy.array([s for s,d in data])
        y = numpy.hstack((yticks.repeat(2)[1:], yticks[-1]+yticks[1]-yticks[0]))
        X, Y = pylab.meshgrid(x, y)
        array = array.repeat(2, axis=1).repeat(2, axis=0)
        fig.ax.plot_surface(X, Y, array, rstride=1, cstride=1, linewidth=1, antialiased=True, cmap=options['color'], alpha=0.8)
        import matplotlib.ticker as ticker
        fig.ax.w_yaxis.set_major_locator(ticker.FixedLocator(yticks))

def data2pcolor(fig, data, options, range=(0.0, 1.0), overwrite_bins=None, masked=True, fast=True):
        """
        data := [(yticklabel, sample)]
        """
        if options['grayscale']:
            colors = options['graycm']
        else:
            colors = options['color']

        if overwrite_bins:
            bins = overwrite_bins
        else:
            bins = options['bins']
        array = numpy.zeros((len(data), bins))
        for i, (s, pdrs) in enumerate(data):
            hist, _bin_edges = numpy.histogram(pdrs, bins=bins, range=range, normed=False)
            hist_norm = hist/float(len(pdrs))
            array[i] = hist_norm

        x = pylab.linspace(0.0, 1.0, bins+1)
        y = numpy.array([s for s, Y in data])
        #if len(y) > 1:
        y = numpy.hstack((y, y[-1]+(y[1]-y[0])))
        #else:
            #y = numpy.hstack((y, y))
        X, Y = pylab.meshgrid(x, y)
        if masked:
            marray = numpy.ma.masked_less_equal(array, 0.0)
            if not fast:
                pcolor = fig.ax.pcolor(X, Y, marray, cmap=colors, vmin=0.0) #, vmax=1.0)
            else:
                pcolor = fig.ax.pcolorfast(X, Y, marray, cmap=colors, vmin=0.0) #, vmax=1.0)
        else:
            if not fast:
                pcolor = fig.ax.pcolor(X, Y, array, cmap=colors, vmin=0.0) #, vmax=1.0)
            else:
                pcolor = fig.ax.pcolorfast(X, Y, array, cmap=colors, vmin=0.0) #, vmax=1.0)
        fig.ax.set_yticks(y[0:-1]+0.5*(y[1]-y[0]))
        fig.ax.set_yticklabels(['$%d$' % l for l in y])

        #if overwrite_bins:
        fig.ax.set_xticks(numpy.linspace(0, 1, 6))
        fig.ax.set_xticklabels(['$%.1f$' % l for l in pylab.linspace(0, range[1], 6)])

        cbar = fig.colorbar(pcolor)
        #cbar = fig.fig.colorbar(pcolor, shrink=0.8, aspect=20, ticks=pylab.linspace(0.0, max(pylab.flatten(array)), 11))
        #cbar.ax.set_yticklabels(['$%.2f$' % f for f in pylab.linspace(0.0, max(pylab.flatten(array)), 11)], fontsize=0.8*options['fontsize'])
        fig.ax.set_ylim(y[0], y[-1])

@requires_hellos
def plot_hello_etx(options, tags=None, cursor=None):
    """
    Plot the CDF of the link ETX
    """
    options['prefix'] = 'etx'
    ################################################################################
    quantiles = pylab.linspace(0, 1, 100)
    colors = options['color'](pylab.linspace(0, 1.0, len(tags)))
    linestyles = options['linestyles']
    bins = options['bins']
    ################################################################################
    fig_ext = MyFig(options, xlabel='$1/ETX$', ylabel='$CDF$', legend=True, grid=False)
    fig_ext_diff = MyFig(options, ylabel='Fraction $f_i$', xlabel='$ETX_{Diff}$', legend=True, grid=False, aspect='auto')

    etx_diffs = list()
    for j, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, j+1, len(tags))
        cursor.execute('''
            SELECT A.pdr, B.pdr
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host=B.src AND A.src=B.host
            WHERE A.tag_key=? AND B.tag_key=?
        ''', (tag_key, tag_key))
        data = cursor.fetchall()
        etxs = [a*b for a, b in data]
        etx_diff = [math.sqrt(a*b)-a for a,b in data] + [math.sqrt(a*b)-a for a,b in data]
        etx_diffs.append((helloSize, etx_diff))
        etx_values = stats.mstats.mquantiles(etxs, prob=quantiles)
        fig_ext.ax.plot(etx_values, quantiles, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)

        X, Y = data2hist(etx_diff, bins=pylab.linspace(-1, 0, bins+1))
        fig_ext_diff.ax.plot(X, Y, color=colors[j], linestyle=linestyles[j], label='$%d$' % helloSize)

    def pcolor_plot():
        if len(set(s for (s, etx_diff) in etx_diffs)) <= 1:
            return
        fig_etx_pcolor = MyFig(options, figsize=(10, 7), ylabel='Packet size', xlabel='$ETX_{Diff}$', aspect='auto')
        array = numpy.zeros((len(etx_diffs), bins))
        for i, (s, etx_diff) in enumerate(etx_diffs):
            hist, _bin_edges = numpy.histogram(etx_diff, bins=pylab.linspace(-1, 0, bins+1), normed=False)
            hist_norm = hist/float(len(etx_diff))
            array[i] = hist_norm
        x = pylab.linspace(-1, 0, bins+1)
        y = numpy.array([s for s, Y in etx_diffs])
        y = numpy.hstack((y, y[-1]+(y[1]-y[0])))
        X, Y = pylab.meshgrid(x, y)
        marray = numpy.ma.masked_less_equal(array, 0.0)
        pcolor = fig_etx_pcolor.ax.pcolor(X, Y, marray, cmap=options['color'], vmin=0.0) #, vmax=1.0)
        fig_etx_pcolor.ax.set_yticks(y[0:-1]+0.5*(y[1]-y[0]))
        fig_etx_pcolor.ax.set_yticklabels(y)
        ticks = pylab.linspace(0.0, max(pylab.flatten(array)), 11)
        fig_etx_pcolor.colorbar(pcolor)
        fig_etx_pcolor.ax.set_ylim(y[0], y[-1])
        fig_etx_pcolor.save('diff_pcolor')

    fig_ext.legend_title = 'Packet size [bytes]'
    fig_ext.ax.axis((0, 1, 0, 1))
    fig_ext.save('cdf')

    fig_ext_diff.legend_title = 'Packet size [bytes]'
    #fig_ext_diff.ax.set_xlim(-1, 1)
    fig_ext_diff.save('diff-hist')

    pcolor_plot()


@requires_hellos
def plot_hello_topology(options, tags=None, cursor=None):
    """
    Plots quality for all (unidirectional) links between the nodes.
    A 2d and 3d representation of the network graph is plotted.
    """
    options['prefix'] = 'topo'
    ################################################################################
    locs = options['locs']
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, 101))
    else:
        colors = options['color2'](pylab.linspace(0, 1, 101))
    ################################################################################
    circ_max = 5
    line_max = 10
    floor_factor = 2
    floor_skew = -0.25
    line_min = 1

    hosts = get_hosts(options)

    for q, (tag_key, tag_id, tag_helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        fig2d = MyFig(options, figsize=(10, 10), rect=[0.1, 0.1, 0.8, 0.7], xlabel='$x$-coordinate [m]', ylabel='$y$-coordinate [m]')
        fig3d = MyFig(options, figsize=(10, 10), rect=[0.1, 0.1, 0.8, 0.7], xlabel='$x$-coordinate [m]', ylabel='$y$-coordinate [m]', ThreeD=True)
        if not q:
            fig3d_onlynodes = MyFig(options, figsize=(10, 10), xlabel='$x$-coordinate [m]', ylabel='$y$-coordinate [m]', ThreeD=True)

        min_x = min_y = min_z = numpy.infty
        max_x = max_y = max_z = 0

        # first draw the edges...
        for host in hosts:
            logging.debug('plotting host=%s', host)
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue
            ################################################################################
            # Evaluate each neighbor (src) of the current node (host) for which a
            # HELLO was received
            ################################################################################
            cursor.execute('''
                SELECT src, pdr
                FROM eval_helloPDR
                WHERE host=? and tag_key=?
            ''', (host, tag_key))
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

        # ...then draw the nodes
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
        fig2d.colorbar = fig2d.fig.add_axes([0.2, 0.775, 0.6, 0.025])
        fig3d.colorbar = fig3d.fig.add_axes([0.2, 0.775, 0.6, 0.025])
        drawBuildingContours(fig3d.ax, options)
        drawBuildingContours(fig3d_onlynodes.ax, options)

        fig3d.ax.text(90, -10, -4, 't9')
        fig3d.ax.text(40, 80, 6, 'a6')
        fig3d.ax.text(80, 100, 8, 'a3')

        alinspace = numpy.linspace(0, 1, 100)
        alinspace = numpy.vstack((alinspace, alinspace))
        for tax in [fig2d.colorbar, fig3d.colorbar]:
            if options['grayscale']:
                tax.imshow(alinspace, aspect='auto', cmap=options['graycm'])
            else:
                tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
            tax.set_xticks(pylab.linspace(0, 100, 5))
            tax.set_xticklabels(['$%.2f$' % l for l in pylab.linspace(0, 1, 5)], fontsize=0.8*options['fontsize'])
            tax.set_yticks([])
            tax.set_title(r'$\PDR$', size=options['fontsize'])
        fig2d.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
        fig2d.save('2d_%d' % (tag_helloSize))
        fig3d.save('3d_%d' % (tag_helloSize))
        if not q:
            fig3d_onlynodes.save('3d')

@requires_hellos
def plot_hello_topology_special(options, tags=None, cursor=None):
    """
    Plots quality for all (unidirectional) links between the nodes.
    A 2d and 3d representation of the network graph is plotted.
    """
    options['prefix'] = 'topo'
    ################################################################################
    locs = options['locs']
    colors = options['color2'](pylab.linspace(0, 1, 101))
    ################################################################################
    circ_max = 4

    hosts = get_hosts(options)

    for q, (tag_key, tag_id, tag_helloSize, helloInterval) in enumerate(tags):
        if tag_helloSize != 128:
            continue
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, q+1, len(tags))
        fig3d = MyFig(options, figsize=(10, 10), rect=[0.1, 0.1, 0.8, 0.7], xlabel='', ylabel='', ThreeD=True, aspect='equal')

        min_x = min_y = min_z = numpy.infty
        max_x = max_y = max_z = 0

        # first draw the edges...
        for host in hosts:
            logging.debug('plotting host=%s', host)
            try:
                host_xpos, host_ypos, host_zpos = locs[host]
            except KeyError:
                logging.warning('no position found for node %s', host)
                continue
            ################################################################################
            # Evaluate each neighbor (src) of the current node (host) for which a
            # HELLO was received
            ################################################################################
            cursor.execute('''
                SELECT src, pdr
                FROM eval_helloPDR
                WHERE host=? AND tag_key=? AND pdr >= 0.9
            ''', (host, tag_key))
            for src, pdr in cursor.fetchall():
                try:
                    src_xpos, src_ypos, src_zpos = locs[src]
                except KeyError:
                    logging.warning('no position found for node %s', src)
                    continue

                fig3d.ax.plot(
                    [host_xpos, src_xpos],
                    [host_ypos, src_ypos],
                    [host_zpos, src_zpos],
                    linestyle='-', color='#99CC00', linewidth=1, alpha=0.3)

        # ...then draw the nodes
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

            fig3d.ax.plot([xpos], [ypos], [zpos], 'o', color='#003366', ms=circ_max)
        drawBuildingContours(fig3d.ax, options)

        #fig3d.ax.text(90, -10, -4, 't9')
        #fig3d.ax.text(40, 80, 6, 'a6')
        #fig3d.ax.text(80, 100, 8, 'a3')

        #fig3d.ax.w_zaxis.set_ticks([-15])
        #fig3d.ax.w_xaxis.set_ticks([0])
        #fig3d.ax.w_yaxis.set_ticks([-35])
        for wax in [fig3d.ax.w_xaxis, fig3d.ax.w_yaxis, fig3d.ax.w_zaxis]:
            wax.set_ticklabels([])
            wax.set_visible(False)
            wax.grid(color='r', linestyle='', linewidth=0, alpha=0)

        fig3d.ax.plot([0, 0], [0, 0], [-15, 25], alpha=0)
        fig3d.ax.axis("off")
        fig3d.ax.set_axis_off()
        fig3d.ax.set_xticks([])
        fig3d.ax.set_yticks([])
        fig3d.ax.view_init(45, 155)
        fig3d.ax.autoscale_view(scalex=False, scaley=False, scalez=False)
        fig3d.save('3d_%d_special' % (tag_helloSize))

@requires_hellos
def plot_hello_components(options, tags=None, cursor=None):
    '''
    Plot the number of components in the network graph.
    Edges are successively removed (lowest PDR first) and the number of
    components is than determined.
    '''
    options['prefix'] = 'components'
    ################################################################################
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(tags)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(tags)))
    markers = options['markers']*100
    linestyles = options['linestyles']*100
    hosts = get_hosts(options)
    ################################################################################
    fig_components = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel='$G_{PDR \geq}$', ylabel='Fraction of Strongly Connected Components', aspect='auto')
    fig_numnodesmax = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel='$G_{PDR \geq}$', ylabel='Fraction of Nodes', aspect='auto', legend_pos='lower left')
    #fig_numnodesmax.ax_2nd.set_ylabel('Fraction of Filtered Links', size=options['fontsize'])
    fig_vs = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel='Fraction of Nodes', ylabel='Strongly Connected Components', aspect='auto')
    fig_vs.ax.plot([1, len(hosts)], [len(hosts), 1], linestyle='dashed', color='black', linewidth=0.5)

    digraphx = nx.DiGraph()
    digraphx.add_nodes_from(hosts)
    for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, i+1, len(tags))
        pdrs = cursor.execute('''
            SELECT DISTINCT(pdr)
            FROM eval_helloPDR
            WHERE tag_key=? ORDER BY pdr
        ''', (tag_key,)).fetchall()
        pdrs = list(pylab.flatten(pdrs))
        pdrs[:0] = [0.0]
        links = cursor.execute('''
            SELECT src, host, pdr
            FROM eval_helloPDR
            WHERE tag_key=?
        ''', (tag_key,)).fetchall()
        # remove edge(-s) with lowest pdr first and see what number of components the graph has
        num_components = []
        nodes_in_larges_component = []
        frac_rm_edges = []
        rmedges = set()
        for minpdr in pdrs:
            digraph_tmp = digraphx.copy()
            _rmedges = set()
            for src, host, pdr in links:
                if pdr > minpdr:
                    digraph_tmp.add_edge(src, host, weight=pdr)
                else:
                    _rmedges.add((src, host))
            rmedges = rmedges.union(_rmedges)

            components = nx.strongly_connected_components(digraph_tmp)
            if not len(components):
                nodes_in_larges_component.append(1)
                num_components.append(digraph_tmp.number_of_nodes())
            else:
                nodes_in_larges_component.append(len(components[0]))
                num_components.append(len(components))
            frac_rm_edges.append(float(len(rmedges))/len(links))

        fig_components.ax.plot(pdrs, numpy.array(num_components)/float(len(hosts)), linestyle=linestyles[i], color=colors[i], label='$%d$' % helloSize)
        fig_numnodesmax.ax.plot(pdrs, numpy.array(nodes_in_larges_component)/float(len(hosts)), linestyle=linestyles[i], color=colors[i], label='$%d$' % helloSize)
        #fig_numnodesmax.ax_2nd.plot(pdrs, frac_rm_edges, linestyle='dotted', color=colors[i], label='$%d$' % helloSize, alpha=0.5)
        fig_vs.ax.plot(num_components, nodes_in_larges_component, linestyle=linestyles[i], color=colors[i], label='$%d$' % helloSize, marker=markers[i])

    #fig_components.ax.set_ylim(1, len(hosts))
    fig_components.legend_title = 'Packet size [bytes]'
    fig_components.ax.set_yticks(numpy.arange(0.2, 1.01, 0.2))
    fig_components.save('over_pdr')
    #fig_numnodesmax.ax.set_ylim(1, len(hosts))
    fig_numnodesmax.legend_title = 'Packet size [bytes]'
    fig_numnodesmax.ax.set_yticks(numpy.arange(0.2, 1.01, 0.2))
    fig_numnodesmax.save('max_nodes_over_pdr')

    fig_vs.ax.set_xlim(1, len(hosts))
    fig_vs.ax.set_ylim(1, len(hosts))
    fig_vs.legend_title = 'Packet size [bytes]'
    fig_vs.save('max_nodes_over_components')

@requires_hellos
def plot_hello_nodes_dists(options, tags=None, cursor=None):
    '''
    Plot diameter, eccentricity, etc
    '''
    options['prefix'] = 'dist'
    ################################################################################
    colors = options['color'](pylab.linspace(0, 1, len(tags)))
    markers = options['markers']
    linestyles = options['linestyles']
    hosts = get_hosts(options)
    ################################################################################
    graphx = nx.Graph()
    graphx.add_nodes_from(hosts)
    fig_all_sizes = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel='Nodes', ylabel='Average Distance [Hops]', aspect='auto')
    for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
        fig = MyFig(options, grid=False, xlabel='Nodes', ylabel='Distance [Hops]', aspect='auto')
        fig_weighted = MyFig(options, grid=False, xlabel='Nodes', ylabel='Distance [Hops]', aspect='auto')
        logging.info('tag_id=\"%s\" (%d/%d)', tag_id, i+1, len(tags))
        links = cursor.execute('''
            SELECT A.src, A.host, (A.pdr*B.pdr) AS rETX
            FROM eval_helloPDR as A JOIN eval_helloPDR as B
            ON A.host=B.src AND A.src=B.host
            WHERE A.tag_key=? AND B.tag_key=?
            AND rETX >= 0.5
            ORDER BY A.src
        ''', (tag_key, tag_key)).fetchall()
        graph_tmp = graphx.copy()
        for src, host, rETX in links:
            graph_tmp.add_edge(src, host, weight=1/rETX)

        if not nx.is_connected(graph_tmp):
            logging.warning('Graph is not connected, using largest component')
            graph_tmp = nx.subgraph(graph_tmp, nx.connected_components(graph_tmp)[0])

        for f, weighted in [(fig, False), (fig_weighted, True)]:
            spaths = nx.shortest_path(graph_tmp, weight=weighted)
            data = []
            for node in spaths:
                dists = [len(d) for d in spaths[node].values()]
                max_dist = max(dists)
                min_dist = min(dists)
                avr_dist = scipy.average(dists)
                l, r, _unused  = confidence(dists)
                data.append((min_dist, avr_dist, max_dist, (l,r)))
            data = sorted(data, key=lambda d: d[1])
            for j, d in enumerate(data):
                l, r = d[3]
                f.ax.plot([j, j], [l, r], '-', color='blue')
                f.ax.plot(j, d[1], 'o', color='red')
                f.ax.plot(j, d[2], 'x', color='red')
            if not weighted:
                fig_all_sizes.ax.plot([j for j in xrange(0, len(data))], [d[1] for d in data], linestyle=linestyles[i], color=colors[i])
                fig_all_sizes.ax.plot([j for j in xrange(0, len(data), 5)], [d[1] for d in data][::5], linestyle=' ', color=colors[i],  marker=markers[i])
                fig_all_sizes.ax.plot([0, 1], [-1, -1], linestyle=linestyles[i], color=colors[i], label='$%d$' % helloSize, marker=markers[i])
                avr = scipy.average([d[1] for d in data])
                #fig_all_sizes.ax.plot([0, 110], [avr, avr], linestyle='-', color=colors[i], alpha=0.5)
            f.ax.set_ylim(0,12)
            f.ax.set_xlim(0,110)
        fig.save('size%d' % helloSize)
        fig_weighted.save('size%d-weighted' % helloSize)
    fig_all_sizes.ax.set_xlim(0, len(hosts)-1)
    fig_all_sizes.ax.set_ylim(0,11)
    fig_all_sizes.legend_title = 'Packet size [bytes]'
    fig_all_sizes.save('dists')

def multi_bar(fig, data, xticklabels, bar_labels, options):
    N = len(data)
    M = len(data[0])
    ax = fig.ax
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, N))
    else:
        colors = options['color'](pylab.linspace(0, 1, N))

    ind = numpy.arange(N, dtype=float)
    med = numpy.arange(N)
    width = (1-0.2)/N
    ind -= 0.5*N*width

    rects = list()
    for i, d in enumerate(data):
        rect = ax.bar(ind+i*width, d, width, color=colors[i])
        rects.append(rect)

    ax.set_xticks(med)
    ax.set_xticklabels(xticklabels)

    ax.legend([rect[0] for rect in rects], bar_labels, loc='upper left', ncol=2, fancybox=False, shadow=True, labelspacing=0)
    ax.set_ylim(0, 14)

@requires_hellos
def plot_hello_diameter(options, tags=None, cursor=None):
    '''
    Plot diameter, eccentricity, etc
    '''
    options['prefix'] = 'diameter'
    ################################################################################
    min_pdrs = pylab.linspace(0, 0.1, 6)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(min_pdrs)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(min_pdrs)))
    markers = options['markers']
    linestyles = options['linestyles']
    hosts = get_hosts(options)
    ################################################################################
    fig = MyFig(options, figsize=(10, 8), rect=[.15,.1,.75,.8], legend=True, grid=False, xlabel='Packet Size [bytes]', ylabel='Diameter [hops]', aspect='auto', twinx = True)
    fig.ax_2nd.set_ylabel(r'Average Shortest Path $\avrdist$', size=options['fontsize'])

    fig_diameter = MyFig(options, figsize=(10, 8), rect=[.15,.1,.75,.8], legend=True, grid=False, xlabel='Packet Size [bytes]', ylabel='Diameter [hops]', aspect='auto')
    fig_asp = MyFig(options, figsize=(10, 8), rect=[.15,.1,.75,.8], legend=True, grid=False, xlabel='Packet Size [bytes]', ylabel=r'Average Shortest Path $\avrdist$ [hops]', aspect='auto')
    graphx = nx.DiGraph()
    graphx.add_nodes_from(hosts)
    max_y = 0
    data = []
    for j, min_pdr in enumerate(min_pdrs):
        diameters = []
        average_shortest_path_lengths = []
        for i, (tag_key, tag_id, helloSize, helloInterval) in enumerate(tags):
            logging.info('tag_id=\"%s\" (%d/%d)', tag_id, i+1, len(tags))
            links = cursor.execute('''
                SELECT src, host, pdr
                FROM eval_helloPDR
                WHERE tag_key=? AND pdr >= ?
            ''', (tag_key, min_pdr)).fetchall()
            graph_tmp = graphx.copy()
            for src, host, pdr in links:
                graph_tmp.add_edge(src, host, weight=pdr)

            if not nx.is_strongly_connected(graph_tmp):
                graph_tmp = nx.subgraph(graph_tmp, nx.strongly_connected_components(graph_tmp)[0])
                logging.warning('Graph is not connected, using largest component, nodes=%d' % len(graph_tmp))
            diameters.append(nx.diameter(graph_tmp))
            max_y = max(max_y, diameters[-1])
            average_shortest_path_lengths.append(nx.average_shortest_path_length(graph_tmp, weight=False))

        fig.ax.plot(range(0, len(tags)), diameters, '-', color=colors[j], marker=markers[j], label='$G_{PDR \geq %.2f}$' % min_pdr)
        data.append(diameters)
        fig.ax_2nd.plot(range(0, len(tags)), average_shortest_path_lengths, linestyle='dashed', color=colors[j], marker=markers[j])

        fig_asp.ax.plot(range(0, len(tags)), average_shortest_path_lengths, color=colors[j], label='$G_{PDR \geq %.2f}$' % min_pdr, linestyle=linestyles[j])

    bar_labels = ['$G_{PDR \geq %.2f}$' % min_pdr for min_pdr in min_pdrs]
    xtick_labels = tags
    multi_bar(fig_diameter, data, xtick_labels, bar_labels, options)

    for f in [fig, fig_diameter, fig_asp]:
        f.ax.set_xticklabels(['$%s$'% helloSize for _tag_key, _tag_id, helloSize, helloInterval in tags])
    max_y = 10
    for ax in [fig.ax, fig.ax_2nd]:
        ax.axis((0, len(tags)+1, 0, max_y+1))
        ax.axis((0, len(tags)+1, 0, max_y+1))
        #ax.ylim(0, max_y+1)
        #ax_2nd.ylim(0, max_y+1)

    for f in [fig, fig_diameter, fig_asp]:
        f.legend_title = 'Subgraph'
    fig_asp.ax.set_ylim(0, 4)
    fig_asp.ax.set_yticks(range(1,5,1))
    fig.save('diameter_and_asp')
    fig_diameter.save('diameter', legend_cols=3)
    fig_asp.save('asp')
