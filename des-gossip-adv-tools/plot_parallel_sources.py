#!/usr/bin/python
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import scipy
import pylab
import sqlite3
import scipy.stats as stats
import logging
import numpy
from helpers import MyFig, requires_parallel_sources, requires_single_sources, convex_hull, fu_colormap, data2hist, conf2poly, confidence, get_hosts, data2dict, add_legend
from matplotlib.patches import Polygon, Ellipse, Rectangle
from matplotlib.collections import PatchCollection
from scipy import polyval, polyfit
from scipy.stats import kde
import re
import copy
from mpl_toolkits.axes_grid1.inset_locator import zoomed_inset_axes
from mpl_toolkits.axes_grid1.inset_locator import mark_inset

@requires_parallel_sources
def plot_parallel(options, cursor=None):
    '''Meta function that calls all parallel plot functions'''
    plot_reachability_over_fw(options)
    plot_parallel_hops(options)

@requires_parallel_sources
def plot_parallel_hops(options, cursor=None):
    '''Plots the number of hops the packets traveled through the
      network as function of p'''
    logging.info('')
    l = cursor.execute('''
        SELECT S.sources, T.p, D.hops, CAST(F.forwarded AS REAL)/SENT
        FROM eval_traveled_hops AS D JOIN tag AS T JOIN eval_sources_per_tag AS S JOIN eval_fw_packets_per_tag as F JOIN (
            SELECT tag, COUNT(*) AS SENT
            FROM tx GROUP BY tag
        ) AS TX
        ON D.tag_key = T.key AND T.key = S.tag_key AND F.tag_key = T.key AND TX.tag = T.key
        ORDER BY S.sources, T.p
    ''').fetchall()

    results = dict()
    for sources, p, hops, fw in l:
        try:
            d1 = results[int(sources)]
        except KeyError:
            d1 = {}
            results[int(sources)] = d1
        try:
            d2 = d1[str(p)]
        except KeyError:
            d2 = []
            d1[str(p)] = d2
        d2.append((hops, fw))

    fig = MyFig(options, figsize=(10,8), xlabel='Probability $p_s$', ylabel='Mean distance [hops]', legend=True, grid=False, aspect='auto')
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(results.keys())))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(results.keys())))
    markers = options['markers']
    for i, sources in enumerate(sorted(results.keys())):
        mean_y = list()
        confs_y = list()
        for p in sorted(results[sources].keys()):
            hops = [h for h, w in results[sources][p]]
            mean = scipy.mean(hops)
            mean_y.append(mean)
            c = 1.96 * scipy.std(hops)/scipy.sqrt(len(hops))
            confs_y.append(c)
            #fig.ax.errorbar(float(p), mean, yerr=c, color=colors[i])
        if len(np.arange(0.1, 1.01, 0.1)) !=  len(mean_y):
            logging.warning('skipping')
            continue
        ps = np.arange(0.1, 1.01, 0.1)

        poly = [conf2poly(ps, list(numpy.array(mean_y)+numpy.array(confs_y)), list(numpy.array(mean_y)-numpy.array(confs_y)), color=colors[i])]
        patch_collection = PatchCollection(poly, match_original=True)
        patch_collection.set_alpha(0.3)
        patch_collection.set_linestyle('dashed')
        fig.ax.add_collection(patch_collection)
        fig.ax.plot(ps, mean_y, label='$%d$' % sources, color=colors[i]) #, marker=markers[i])

    fig.ax.set_ylim(0, 12)
    #fig.ax.set_xlim(0,1.1)
    fig.legend_title = r'Sources $\sources$'
    fig.save('parallel-hops')

@requires_parallel_sources
def plot_mcds_forwarded(options, cursor=None):
    '''Plots the number of forwarded packets as function of p'''
    logging.info('')

    intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()

    def get_mcds_size():
        tag_ids = cursor.execute('''SELECT distinct(id) FROM tag''').fetchall()
        re_mcds = re.compile(r'.*,mcds=(.*).*')
        mcds_size = 0
        for tid, in tag_ids:
            match = re_mcds.match(tid)
            if match:
                forwarders = match.group(1).split(',')
                if mcds_size == 0:
                    mcds_size = len(forwarders)
                else:
                    if mcds_size != len(forwarders):
                        assert(False)

        if mcds_size == 0:
            forwarders = cursor.execute('''SELECT distinct(host) FROM tag group by tag''').fetchall()
        return mcds_size

    fdata = dict()
    for j, (interval,) in enumerate(intervals):
        cursor.execute('''
            SELECT A.sources, CAST(B.forwarded AS REAL)/D.SENT
            FROM eval_sources_per_tag AS A JOIN eval_fw_packets_per_tag as B JOIN tag as C JOIN (
                SELECT tag, COUNT(*) AS SENT
                FROM tx GROUP BY tag
            ) AS D
            ON A.tag_key=B.tag_key AND B.tag_key=C.key AND C.key=D.tag
            WHERE C.pkg_interval=?
            ORDER BY A.sources
        ''', (interval,))
        data = cursor.fetchall()

        results = data2dict(data)
        #results = dict()
        #for sources, p, fw in data:
            #try:
                #d1 = results[int(sources)]
            #except KeyError:
                #d1 = {}
                #results[int(sources)] = d1
            #try:
                #d2 = d1[str(p)]
            #except KeyError:
                #d2 = []
                #d1[str(p)] = d2
            #d2.append(float(fw))

        size = get_mcds_size()
        if size == 0:
            logging.warning('mcds size is 0')
            size = 1
        fig = MyFig(options, figsize=(10,8), xlabel=r'Sources~$\sources$', ylabel=r'Fraction of Forwarded Packets~${\forwarded}_{\MCDS}$', grid=False, aspect='auto')
        fig.ax.boxplot([numpy.array(results[sources])/size for sources in sorted(results.keys())], notch=1)
        #fig.ax.boxplot([numpy.array(results[sources]['1.0'])/size for sources in sorted(results.keys())], notch=1)
        fig.ax.set_xticklabels(sorted(results.keys()))
        fig.ax.set_ylim(0, 1)
        fig.legend_title = r'Sources~$\sources$'
        fig.save('parallel-forwarded_mcds-interval_%.2f' % interval)
        fdata[interval] = results
    return fdata

@requires_parallel_sources
def plot_mcds_reachability_over_fw(options, cursor=None):
    fdata = plot_mcds_forwarded(options)
    rdata = plot_mcds_reachability(options)
    num_hosts = len(get_hosts(options))

    fit_file = open(options['outdir']+ '/fit.data', 'w')
    r_file = open(options['outdir']+'/reachability.data', 'w')
    min_pdr=options['outdir'].split('/')[-2]

    for interval in fdata.keys():
        results_fw = fdata[interval]
        results_r = rdata[interval]
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(results_fw)))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(results_fw)))

        fig = MyFig(options, figsize=(10,8), ylabel=r'Reachability~$\reachability$', xlabel=r'Fraction of Forwarded Packets~${\forwarded}$', grid=False, legend=True, aspect='auto')

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
            r_file.write('R(N=%d, min_pdr=%s)=%f, %f, %f\n' % (sources, min_pdr, scipy.mean(r), r_conf, fw_conf))
            print 'R(N=%d, min_pdr=%s)=%f, %f, %f' % (sources, min_pdr, scipy.mean(r), r_conf, fw_conf)
        add_legend(options, fig, ['$%d$' % s for s in sorted(results_r.keys())], alpha=alpha)
        patch_collection = PatchCollection([e1 for x, y, e1, e2 in ellipses], match_original=True)
        fig.ax.add_collection(patch_collection)
        axins = zoomed_inset_axes(fig.ax, 4, loc=8)
        axins.set_xlim(min([x for x, y, e1, e2 in ellipses])-0.02, max([x for x, y, e1, e2 in ellipses])+0.02)
        axins.set_ylim(min([y for x, y, e1, e2 in ellipses])-0.02, max([y for x, y, e1, e2 in ellipses])+0.02)
        patch_collection2 = PatchCollection([e2 for x, y, e1, e2 in ellipses], match_original=True)
        axins.add_collection(patch_collection2)
        axins.set_xticklabels([])
        axins.set_yticklabels([])
        pargs = numpy.polyfit([x for x,y in ndata], [y for x,y in ndata], 1)
        print '[min_pdr=%s], f(F) = %fx + %f' % (min_pdr, pargs[0], pargs[1])
        fit_file.write('[min_pdr=%s], R(F) = %fx + %f\n' % (min_pdr, pargs[0], pargs[1]))
        x = pylab.linspace(0, 1, 1000)
        fig.ax.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
        axins.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
        mark_inset(fig.ax, axins, loc1=2, loc2=3, fc="none", ec="0.5")
        #####
        X = numpy.array([x for x,y in ndata]) - numpy.array([dx for dx, dy in cdata])
        #Y = [y for x,y in ndata]
        Y = numpy.array([y for x,y in ndata]) + numpy.array([dy for dx, dy in cdata])
        pargs2 = numpy.polyfit(X, Y, 1)
        #####
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
        #####
        fig.ax.set_xlim(0, 1)
        fig.ax.set_ylim(0, 1)
        fig.legend_title = r'Sources~$\sources$'
        fig.save('mcds-reachability_over_fw_interval_%.2f' % (interval,))

@requires_parallel_sources
def plot_forwarded(options, cursor=None):
    '''Plots the number of forwarded packets as function of p'''
    logging.info('')

    intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    interval_data = dict.fromkeys([s for s, in sources],  MyFig(options, figsize=(10,8), xlabel='Probability $p_s$', ylabel='Forwarded Packets per Source', legend=True, grid=False, aspect='auto'))
    if options['grayscale']:
        interval_colors = options['graycm'](pylab.linspace(0, 1, len(intervals)))
    else:
        interval_colors = options['color'](pylab.linspace(0, 1, len(intervals)))
    num_hosts = len(get_hosts(options))

    for j, (interval,) in enumerate(intervals):
        cursor.execute('''
            SELECT A.sources, C.p, CAST(B.forwarded AS REAL)/D.SENT
            FROM eval_sources_per_tag AS A JOIN eval_fw_packets_per_tag as B JOIN tag as C JOIN (
                SELECT tag, COUNT(*) AS SENT
                FROM tx GROUP BY tag
            ) AS D
            ON A.tag_key=B.tag_key AND B.tag_key=C.key AND C.key=D.tag
            WHERE C.pkg_interval=?
            ORDER BY A.sources
        ''', (interval,))
        data = cursor.fetchall()

        results = dict()
        for sources, p, fw in data:
            try:
                d1 = results[int(sources)]
            except KeyError:
                d1 = {}
                results[int(sources)] = d1
            try:
                d2 = d1[str(p)]
            except KeyError:
                d2 = []
                d1[str(p)] = d2
            d2.append(float(fw))

        for k in sorted(results.keys()):
            print k, len(results[k])

        fig = MyFig(options, figsize=(10,8), xlabel='Probability $p_s$', ylabel=r'Fraction of Forwarded Packets~$\forwarded$', legend=True, grid=False, aspect='auto')
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(results.keys())))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(results.keys())))
        markers = options['markers']
        for i, sources in enumerate(sorted(results.keys())):
            mean_y = list()
            confs_y = list()
            for p in sorted(results[sources].keys()):
                fracs = numpy.array(results[sources][p])/(num_hosts-1)
                mean = scipy.mean(fracs)
                mean_y.append(mean)

                #1.96 * scipy.std(fracs)/scipy.sqrt(len(fracs))
                upper, lower, conf = confidence(fracs)
                confs_y.append(conf)

                #fig.ax.errorbar(float(p), mean, yerr=conf, color=colors[i])
                if interval in [0.2, 0.4, 1.0, 2.0, 5.0]:
                    interval_data[sources].ax.errorbar(float(p), mean, yerr=conf, color=interval_colors[j])
            ps = sorted(results[sources].keys())
            if len(ps) !=  len(mean_y):
                logging.warning('skipping')
                continue

            poly = [conf2poly(ps, list(numpy.array(mean_y)+numpy.array(confs_y)), list(numpy.array(mean_y)-numpy.array(confs_y)), color=colors[i])]
            patch_collection = PatchCollection(poly, match_original=True)
            patch_collection.set_alpha(0.3)
            patch_collection.set_linestyle('dashed')
            fig.ax.add_collection(patch_collection)

            fig.ax.plot(ps, mean_y, label='$%d$' % sources, color=colors[i]) #, marker=markers[i])
            if interval in [0.2, 0.4, 1.0, 2.0, 5.0]:
                interval_data[sources].ax.plot(ps, mean_y, label='$%.2f$' % interval, color=interval_colors[j], marker=markers[j])
        fig.ax.set_ylim(0, 1)
        fig.ax.set_xlim(0, 1)
        fig.legend_title = r'Sources~$\sources$'
        fig.save('parallel-forwarded-interval_%.2f' % interval)
    for sources in interval_data:
        fig = interval_data[sources]
        fig.ax.set_ylim(0, 1)
        fig.ax.set_xlim(0, 1)
        fig.legend_title = 'Interval $[s]$'
        fig.save('parallel_forwarded-sources_%d' % sources)
    return results

@requires_parallel_sources
def plot_reachability_over_fw(options, cursor=None):
    '''Plots the reachability as function of the number of forwarded
       packets'''
    logging.info('')
    options['prefix'] = 'all'
    list_fracs = plot_reachability(options)
    list_fws = plot_forwarded(options)
    num_hosts = len(get_hosts(options))

    fit_file = open(options['outdir']+ '/fit.data', 'w')
    r_file = open(options['outdir']+'/reachability.data', 'w')

    fig = MyFig(options, figsize=(10,8), xlabel=r'Fraction of Forwarded Packets~$\forwarded$', ylabel=r'Reachability~$\reachability$', legend=True, grid=False, aspect='auto', legend_pos='lower right')
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(list_fracs.keys())))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(list_fracs.keys())))
    pdata = dict()
    for i, sources in enumerate(sorted(list_fracs.keys())):
        fracs_means = list()
        r_conf = list()
        fws_means = list()
        fw_conf = list()
        points = list()
        ellipses = list()
        for p in sorted(list_fracs[sources].keys()):
            fracs = list_fracs[sources][p]
            fracs_mean = scipy.mean(fracs)
            fracs_means.append(fracs_mean)
            fracs_c = 1.96 * scipy.std(fracs)/scipy.sqrt(len(fracs))
            r_conf.append(fracs_c)

            fws = numpy.array(list_fws[sources][p])/(num_hosts-1)
            fws_mean = scipy.mean(fws)
            fws_means.append(fws_mean)
            fws_c = 1.96 * scipy.std(fws)/scipy.sqrt(len(fws))
            fw_conf.append(fws_c)

            points.append((fracs_mean,         fws_mean+fws_c))
            points.append((fracs_mean,         fws_mean-fws_c))
            points.append((fracs_mean+fracs_c, fws_mean))
            points.append((fracs_mean-fracs_c, fws_mean))

            ellipse = Ellipse((fws_mean, fracs_mean), fws_c*2, fracs_c*2, edgecolor=colors[i], facecolor=colors[i], alpha=0.5)
            ellipse2 = Ellipse((fws_mean, fracs_mean), fws_c*2, fracs_c*2, edgecolor=colors[i], facecolor=colors[i], alpha=0.5)
            ellipses.append((fws_mean, fracs_mean, ellipse, ellipse2))

            try:
                pdata[p].append((fws_mean, fracs_mean))
            except KeyError:
                pdata[p] = [(fws_mean, fracs_mean)]

            #fig.ax.errorbar(fws_mean, fracs_mean, xerr=fws_c, yerr=fracs_c, color=colors[i])

        if len(fracs_means) !=  len(fws_means):
            logging.warning('skipping')
            continue

        fig.ax.plot(fws_means, fracs_means, label='$%d$' % sources, color=colors[i])
        patch_collection = PatchCollection([e1 for x, y, e1, e2 in ellipses], match_original=True)
        fig.ax.add_collection(patch_collection)
        for F, R, p, F_conf, R_conf in zip(fws_means, fracs_means, sorted(list_fracs[sources].keys()), fw_conf, r_conf):
            r_file.write('R(N=%d, p=%s)=%f, %f, %f\n' % (sources, p, R, R_conf, F_conf))
            print 'R(N=%d, p=%s)=%f, %f, %f' % (sources, p, R, R_conf, F_conf)

        if 'all' in options['mark'] or sources in options['mark']:
            logging.info('marking')
            fig.ax.annotate('$p_s=0.1$', xy=(fws_means[0], fracs_means[0]),  xycoords='data', xytext=(20, 10), textcoords='offset points', size=20,  arrowprops=dict(arrowstyle="->"))
            fig.ax.annotate('$p_s=1.0$', xy=(fws_means[-1], fracs_means[-1]),  xycoords='data', xytext=(10, 20), textcoords='offset points', size=20,  arrowprops=dict(arrowstyle="->"))

    # fit first degree polynomial
    for p in sorted(pdata.keys()):
        ndata = pdata[p]
        pargs = numpy.polyfit([x for x,y in ndata], [y for x,y in ndata], 1)
        x = pylab.linspace(0, 1, 1000)
        fig.ax.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
        print '[p=%s], R(F) = %fx + %f' % (p, pargs[0], pargs[1])
        fit_file.write('[p=%s], R(F) = %fx + %f\n' % (p, pargs[0], pargs[1]))

    fig.ax.set_xlim(0,1)
    fig.ax.set_ylim(0,1)
    fig.legend_title = r'Sources~$\sources$'
    fig.save('parallel-reachability-forwarded')

@requires_parallel_sources
def plot_gossip14_reachability(options, cursor=None):
    '''Plots the reachability when multiple sources sent packets at the same time.
       The reachability is plottet as function over p'''
    logging.info('')
    options['prefix'] = 'all'

    timeouts = list(pylab.flatten(cursor.execute('''SELECT distinct(timeout) FROM tag ORDER BY timeout''').fetchall()))
    ms = list(pylab.flatten(cursor.execute('''SELECT distinct(m) FROM tag ORDER BY m''').fetchall()))
    pkg_sizes = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()))
    #sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(ms)))
        tcolors = options['graycm'](pylab.linspace(0, 1, len(timeouts)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(ms)))
        tcolors = options['color'](pylab.linspace(0, 1, len(timeouts)))

    rdata = dict() # dict[pkg_size][timeout][m][sources]
    for i, pkg_size in enumerate(pkg_sizes):
        m_figs = dict()
        for m in ms:
            m_figs[m] = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', legend=True, grid=False, aspect='auto')

        rdata_size = dict()
        rdata[pkg_size] = rdata_size
        for k, timeout in enumerate(timeouts):
            data = cursor.execute('''
                SELECT s.sources, t.m, AVG(e.frac)
                FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
                ON t.key = e.tag_key AND t.key = s.tag_key
                WHERE t.pkg_size=? AND t.timeout=?
                GROUP BY t.key
                ORDER BY s.sources
            ''', (pkg_size, timeout)).fetchall()

            reach_per_sources = dict()
            for sources, m, reach in data:
                try:
                    lreach = reach_per_sources[m]
                except KeyError:
                    lreach = dict()
                    reach_per_sources[m] = lreach
                try:
                    lreach[sources].append(reach)
                except KeyError:
                    lreach[sources] = [reach]

            rdata_size[timeout] = reach_per_sources
            fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', grid=False, legend=True, aspect='auto')
            for j, m in enumerate(sorted(reach_per_sources.keys())):
                r = reach_per_sources[m]
                sources = sorted(r.keys())
                means = [scipy.mean(r[n]) for n in sources]
                confs = [confidence(r[n])[2] for n in sources]
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=colors[j])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                fig.ax.add_collection(patch_collection)
                fig.ax.plot(sources, means, color=colors[j], label='%d' % (m))

                m_fig = m_figs[m]
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=tcolors[k])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                m_fig.ax.add_collection(patch_collection)
                m_fig.ax.plot(sources, means, color=tcolors[k], label='%d' % (timeout))
            fig.ax.set_xticks(sources)
            fig.ax.set_xlim(min(sources), max(sources))
            fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            fig.ax.set_ylim(0,1)
            fig.legend_title = 'm'
            fig.save('parallel_reachability_gossip14_%dpkgsize_%dtimout' % (pkg_size, timeout))
        for (m, m_fig) in m_figs.iteritems():
            m_fig.ax.set_xticks(sources)
            m_fig.ax.set_xlim(min(sources), max(sources))
            m_fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            m_fig.ax.set_ylim(0,1)
            m_fig.legend_title = 'timeout [ms]'
            m_fig.save('parallel_reachability_gossip14_%dpkgsize_%dm' % (pkg_size, m))
    return rdata

@requires_parallel_sources
def plot_gossip14_forwarded(options, cursor=None):
    '''
    Plots the fraction of forwarded packets when multiple sources sent packets at the same time.
    '''
    logging.info('')
    options['prefix'] = 'all'

    timeouts = list(pylab.flatten(cursor.execute('''SELECT distinct(timeout) FROM tag ORDER BY timeout''').fetchall()))
    ms = list(pylab.flatten(cursor.execute('''SELECT distinct(m) FROM tag ORDER BY m''').fetchall()))
    pkg_sizes = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()))
    #sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    num_hosts = len(get_hosts(options))

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(ms)))
        tcolors = options['graycm'](pylab.linspace(0, 1, len(timeouts)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(ms)))
        tcolors = options['color'](pylab.linspace(0, 1, len(timeouts)))
    fdata = dict() # dict[pkg_size][timeout][m][sources]
    for i, pkg_size in enumerate(pkg_sizes):
        m_figs = dict()
        for m in ms:
            m_figs[m] = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Fraction of Forwarded Packets~$\forwarded$', legend=True, grid=False, aspect='auto')
        fdata_size = dict()
        fdata[pkg_size] = fdata_size
        for k, timeout in enumerate(timeouts):
            data = cursor.execute('''
                SELECT s.sources, t.m, CAST(f.forwarded AS REAL)/D.SENT/?
                FROM eval_sources_per_tag AS s JOIN eval_fw_packets_per_tag as f JOIN tag as t JOIN (
                    SELECT tag, COUNT(*) AS SENT
                    FROM tx GROUP BY tag
                ) AS D
                ON s.tag_key=f.tag_key AND f.tag_key=t.key AND t.key=D.tag
                WHERE t.pkg_size=? AND t.timeout=?
                ORDER BY s.sources
            ''', ((num_hosts-1), pkg_size, timeout)).fetchall()

            fw_per_sources = dict()
            for sources, m, fw in data:
                try:
                    lfw = fw_per_sources[m]
                except KeyError:
                    lfw = dict()
                    fw_per_sources[m] = lfw
                try:
                    lfw[sources].append(fw)
                except KeyError:
                    lfw[sources] = [fw]
            fdata_size[timeout] = fw_per_sources

            fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Fraction of Forwarded Packets~$\forwarded$', grid=False, legend=True, aspect='auto')
            for j, m in enumerate(sorted(fw_per_sources.keys())):
                r = fw_per_sources[m]
                sources = sorted(r.keys())
                means = [scipy.mean(r[n]) for n in sources]
                confs = [confidence(r[n])[2] for n in sources]
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=colors[j])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                fig.ax.add_collection(patch_collection)
                fig.ax.plot(sources, means, color=colors[j], label='%d' % (m))

                m_fig = m_figs[m]
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=tcolors[k])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                m_fig.ax.add_collection(patch_collection)
                m_fig.ax.plot(sources, means, color=tcolors[k], label='%d' % (timeout))
            fig.ax.set_xticks(sources)
            fig.ax.set_xlim(min(sources), max(sources))
            fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            fig.ax.set_ylim(0,1)
            fig.legend_title = 'm'
            fig.save('parallel_forwarded_gossip14_%dpkgsize_%dtimout' % (pkg_size, timeout))
        for (m, m_fig) in m_figs.iteritems():
            m_fig.ax.set_xticks(sources)
            m_fig.ax.set_xlim(min(sources), max(sources))
            m_fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            m_fig.ax.set_ylim(0,1)
            m_fig.legend_title = 'timeout [ms]'
            m_fig.save('parallel_forwarded_gossip14_%dpkgsize_%dm' % (pkg_size, m))
    return fdata

@requires_parallel_sources
def plot_gossip14_reachability_over_fw(options, cursor=None):
    rdata = plot_gossip14_reachability(options)
    fdata = plot_gossip14_forwarded(options)
    num_hosts = len(get_hosts(options))
    sources_list = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))

    fit_file = open(options['outdir']+ '/fit.data', 'w')
    r_file = open(options['outdir']+'/reachability.data', 'w')

    # dict[pkg_size][timeout][m][sources]

    for pkg_size in fdata.keys():
        results_fw = fdata[pkg_size]
        results_r = rdata[pkg_size]

        if options['grayscale']:
            colors_timeout = options['graycm'](pylab.linspace(0, 1, len(results_fw)))
            colors_m = options['graycm'](pylab.linspace(0, 1, len(results_fw[results_fw.keys()[0]])))
            colors_sources = options['graycm'](pylab.linspace(0, 1, len(sources_list)))
        else:
            colors_timeout = options['color'](pylab.linspace(0, 1, len(results_fw)))
            colors_m = options['color'](pylab.linspace(0, 1, len(results_fw[results_fw.keys()[0]])))
            colors_sources = options['color'](pylab.linspace(0, 1, len(sources_list)))

        for i, timeout in enumerate(sorted(results_r.keys())):
            results_fw2 = results_fw[timeout]
            results_r2 = results_r[timeout]
            fig = MyFig(options, figsize=(10,8), ylabel=r'Reachability~$\reachability$', xlabel=r'Fraction of Forwarded Packets~${\forwarded}$', grid=False, legend=True, aspect='auto')

            data = dict()
            for j, m in enumerate(sorted(results_r2.keys())):
                results_fw3 = results_fw2[m]
                results_r3 = results_r2[m]
                ndata = list()
                cdata = list()
                ellipses = list()
                for k, sources in enumerate(sorted(results_r3.keys())):
                    r = results_r3[sources]
                    r_mean = scipy.mean(r)
                    r_conf = confidence(r)[2]
                    fw = results_fw3[sources]
                    fw_mean = scipy.mean(fw)
                    fw_conf = confidence(fw)[2]
                    ndata.append((fw_mean, r_mean))
                    cdata.append((fw_conf, r_conf))
                    try:
                        data[sources].append((fw_mean, r_mean, fw_conf, r_conf))
                    except KeyError:
                        data[sources] = [(fw_mean, r_mean, fw_conf, r_conf)]
                    ellipse = Ellipse((fw_mean, r_mean), fw_conf*2, r_conf*2, edgecolor=colors_sources[k], facecolor=colors_sources[k], alpha=0.5)
                    ellipse2 = Ellipse((fw_mean, r_mean), fw_conf*2, r_conf*2, edgecolor=colors_sources[k], facecolor=colors_sources[k], alpha=0.5)
                    ellipses.append((scipy.mean(fw), scipy.mean(r), ellipse, ellipse2))
                    r_file.write('R(N=%d, pkg_size=%d, timeout=%d, m=%d)=%f, %f, %f\n' % (sources, pkg_size, timeout, m, scipy.mean(r), r_conf, fw_conf))
                    print 'R(N=%d, pkg_size=%d, timeout=%d, m=%d)=%f, %f, %f' % (sources, pkg_size, timeout, m, scipy.mean(r), r_conf, fw_conf)
                pargs = numpy.polyfit([x for x,y in ndata], [y for x,y in ndata], 1)
                print '[pkg_size=%d, timeout=%d, m=%d], f(F) = %fx + %f' % (pkg_size, timeout, m, pargs[0], pargs[1])
                fit_file.write('[pkg_size=%d, timeout=%d, m=%d], R(F) = %fx + %f\n' % (pkg_size, timeout, m, pargs[0], pargs[1]))
                x = pylab.linspace(0, 1, 1000)
                fig.ax.plot(x, numpy.polyval(pargs, x), linestyle='dashed', color='gray', linewidth=1, zorder=-1)
                patch_collection = PatchCollection([e1 for x, y, e1, e2 in ellipses], match_original=True)
                fig.ax.add_collection(patch_collection)
                if m==1:
                    fig.ax.annotate('$m=1$', xy=(ndata[0][0], ndata[0][1]),  xycoords='data', xytext=(-40, 20), textcoords='offset points', size=20,  arrowprops=dict(arrowstyle="->"))
                elif m==3:
                    fig.ax.annotate('$m=3$', xy=(ndata[0][0], ndata[0][1]),  xycoords='data', xytext=(20, 10), textcoords='offset points', size=20,  arrowprops=dict(arrowstyle="->"))
                #####
                X = numpy.array([x for x,y in ndata]) - numpy.array([dx for dx, dy in cdata])
                #Y = [y for x,y in ndata]
                Y = numpy.array([y for x,y in ndata]) + numpy.array([dy for dx, dy in cdata])
                pargs2 = numpy.polyfit(X, Y, 1)
                #####
                #X = [x for x,y in ndata]
                X = numpy.array([x for x,y in ndata]) + numpy.array([dx for dx, dy in cdata])
                Y = numpy.array([y for x,y in ndata]) - numpy.array([dy for dx, dy in cdata])
                pargs3 = numpy.polyfit(X, Y, 1)
                #####
                xcross = (pargs3[1]-pargs2[1]) / float(pargs2[0]-pargs3[0])
                if xcross < min([y for x,y in ndata]):
                    x = pylab.linspace(xcross, 1, 1000)
                elif xcross > max([y for x,y in ndata]):
                    x = pylab.linspace(0, xcross, 1000)
                else:
                    x = pylab.linspace(0, 1, 1000)

                X = numpy.concatenate((x, x[::-1]))
                Y = numpy.concatenate((numpy.polyval(pargs2, x), numpy.polyval(pargs3, x)[::-1]))
                poly1 = Polygon(zip(X, Y), edgecolor='gray', facecolor='gray', closed=True, alpha=0.3)
                poly2 = Polygon(zip(X, Y), edgecolor='gray', facecolor='gray', closed=True, alpha=0.3)
                patch_collection1 = PatchCollection([poly1], match_original=True)
                patch_collection1.zorder = -2
                patch_collection2 = PatchCollection([poly2], match_original=True)
                patch_collection2.zorder = -2
                fig.ax.add_collection(patch_collection1)
                #axins.add_collection(patch_collection2)
                #####
            for k, sources in enumerate(sources_list):
                fig.ax.plot([fw for fw, r, fw_conf, r_conf in data[sources]], [r for fw, r, fw_conf, r_conf in data[sources]], label='%d' % sources, color=colors_sources[k])
            fig.ax.set_xlim(0, 1)
            fig.ax.set_ylim(0, 1)
            fig.legend_title = r'Sources~$\sources$'
            fig.save('gossip14-reachability_over_fw_timeout_%d_pkg_size_%d' % (timeout, pkg_size))

@requires_parallel_sources
def plot_mcds_reachability(options, cursor=None):
    '''Plots the reachability when multiple sources sent packets at the same time.
       The reachability is plottet as function over p'''
    logging.info('')
    options['prefix'] = 'all'

    intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    interval_data = dict.fromkeys([s for s, in sources],  MyFig(options, figsize=(10, 8), xlabel='Probability $p_s$', ylabel=r'Reachability~$\reachability$', legend=True, legend_pos='upper left', grid=False, aspect='auto'))

    if options['intervals']:
        logging.info('using only data for select intervals')
        intervals = [(i,) for (i,) in intervals if i in options['intervals']]
    interval_colors = options['color'](pylab.linspace(0, 1, len(intervals)))

    rdata = dict()
    for j, (interval,) in enumerate(intervals):
        data = cursor.execute('''
            SELECT s.sources, AVG(e.frac)
            FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
            ON t.key = e.tag_key AND t.key = s.tag_key
            WHERE t.pkg_interval=?
            GROUP BY t.key
            ORDER BY s.sources
        ''', (interval,)).fetchall()


        results = data2dict(data)
        #for sources, frac in data:
            #try:
                #d1 = results[int(sources)]
            #except KeyError:
                #d1 = list()
                #results[int(sources)] = d1
            #d1.append(float(frac))

        fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', grid=False, aspect='auto')
        fracs = [results[sources] for sources in sorted(results.keys())]
        fig.ax.boxplot(fracs, notch=1)
        fig.ax.set_xticklabels(sorted(results.keys()))
        fig.ax.set_ylim(0,1)
        #fig.ax.set_xlim(0,1)
        fig.save('parallel_mean_reachability_mcds-interval_%.2f' % interval)
        rdata[interval] = results
    return rdata

@requires_parallel_sources
def plot_reachability(options, cursor=None):
    '''Plots the reachability when multiple sources sent packets at the same time.
       The reachability is plottet as function over p'''
    logging.info('')
    options['prefix'] = 'all'

    intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    interval_data = dict.fromkeys([s for s, in sources],  MyFig(options, figsize=(10, 8), xlabel='Probability $p_s$', ylabel=r'Reachability~$\reachability$', legend=True, legend_pos='upper left', grid=False, aspect='auto'))

    if options['intervals']:
        logging.info('using only data for select intervals')
        intervals = [(i,) for (i,) in intervals if i in options['intervals']]
    if options['grayscale']:
        interval_colors = options['graycm'](pylab.linspace(0, 1, len(intervals)))
    else:
        interval_colors = options['color'](pylab.linspace(0, 1, len(intervals)))

    for j, (interval,) in enumerate(intervals):
        cursor.execute('''
            SELECT s.sources, t.p, e.frac
            FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
            ON t.key = e.tag_key AND t.key = s.tag_key
            WHERE t.pkg_interval=?
            ORDER BY s.sources, t.p
        ''', (interval,))
        data = cursor.fetchall()

        results = {}
        for sources, p, frac in data:
            try:
                d1 = results[int(sources)]
            except KeyError:
                d1 = {}
                results[int(sources)] = d1
            try:
                d2 = d1[str(p)]
            except KeyError:
                d2 = []
                d1[str(p)] = d2
            d2.append(float(frac))
        fig = MyFig(options, figsize=(10, 8), xlabel='Probability $p_s$', ylabel=r'Reachability~$\reachability$', legend=True, grid=False, aspect='auto')
        if options['grayscale']:
            colors = options['graycm'](pylab.linspace(0, 1, len(results.keys())))
        else:
            colors = options['color'](pylab.linspace(0, 1, len(results.keys())))
        markers = options['markers']
        for i, sources in enumerate(sorted(results.keys())):
            mean_y = list()
            confs_y = list()
            for p in sorted(results[sources].keys()):
                fracs = results[sources][p]
                mean = scipy.mean(fracs)
                mean_y.append(mean)

                #conf = 1.96 * scipy.std(fracs)/scipy.sqrt(len(fracs))
                upper, lower, conf = confidence(fracs)
                confs_y.append(conf)
                #fig.ax.errorbar(float(p), mean, yerr=conf, color=colors[i])
                interval_data[sources].ax.errorbar(float(p), mean, yerr=conf, color=interval_colors[j])
            ps = sorted(results[sources].keys())
            if len(ps) !=  len(mean_y):
                logging.warning('skipping')
                continue

            poly = [conf2poly(ps, list(numpy.array(mean_y)+numpy.array(confs_y)), list(numpy.array(mean_y)-numpy.array(confs_y)), color=colors[i])]
            patch_collection = PatchCollection(poly, match_original=True)
            patch_collection.set_alpha(0.3)
            patch_collection.set_linestyle('dashed')
            fig.ax.add_collection(patch_collection)

            fig.ax.plot(ps, mean_y, label='$%d$' % sources, color=colors[i]) #, marker=markers[i])
            results_file = open('/tmp/testbed-results', 'w')
            results_file.write('# Gossip routing process: mean reachability and mean packet forwarding\n\n')
            for run, (x, y) in enumerate(zip(ps, mean_y)):
                results_file.write('RUN%d: %s, 1.0, %f, 0, 0, 0 \n' % (run, x, y))
            interval_data[sources].ax.plot(ps, mean_y, label='$%.2f$' % interval, color=interval_colors[j], marker=markers[j])
        fig.ax.set_ylim(0,1)
        fig.ax.set_xlim(0,1)
        fig.legend_title = r'Sources~$\sources$'
        print 'interval', interval
        fig.save('parallel_mean_reachability-interval_%.2f' % interval)
    for sources in interval_data:
        fig = interval_data[sources]
        fig.ax.set_ylim(0,1)
        fig.ax.set_xlim(0,1)
        fig.legend_title = 'Interval $[s]$'
        fig.save('parallel_mean_reachability-sources_%d' % sources)
    return results

#@requires_parallel_sources
#def plot_parallel_mean_reachability(options, cursor=None):
    #'''Plots the reachability when multiple sources sent packets at the same time.
       #The reachability is plottet as function over p'''
    #logging.info('')
    #options['prefix'] = 'all'

    #intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    #sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    #interval_data = dict.fromkeys([s for s, in sources],  MyFig(options, figsize=(10, 8), xlabel='Probability $p$', ylabel=r'Reachability~$\reachability$', legend=True, legend_pos='upper left', grid=False, aspect='auto'))

    #if options['intervals']:
        #logging.info('using only data for select intervals')
        #intervals = [(i,) for (i,) in intervals if i in options['intervals']]
    #interval_colors = options['color'](pylab.linspace(0, 1, len(intervals)))

    #for j, (interval,) in enumerate(intervals):
        #cursor.execute('''
            #SELECT s.sources, t.p, e.frac
            #FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
            #ON t.key = e.tag_key AND t.key = s.tag_key
            #WHERE t.pkg_interval=?
            #ORDER BY s.sources, t.p
        #''', (interval,))
        #data = cursor.fetchall()

        #results = {}
        #for sources, p, frac in data:
            #try:
                #d1 = results[int(sources)]
            #except KeyError:
                #d1 = {}
                #results[int(sources)] = d1
            #try:
                #d2 = d1[str(p)]
            #except KeyError:
                #d2 = []
                #d1[str(p)] = d2
            #d2.append(float(frac))
        #fig = MyFig(options, figsize=(10, 8), xlabel='Probability $p$', ylabel=r'Reachability~$\reachability$', legend=True, grid=False, aspect='auto')
        #colors = options['color'](pylab.linspace(0, 1, len(results.keys())))
        #markers = options['markers']
        #for i, sources in enumerate(sorted(results.keys())):
            #mean_y = []
            #for p in sorted(results[sources].keys()):
                #fracs = results[sources][p]
                #mean = scipy.mean(fracs)
                #mean_y.append(mean)
                ##fig.ax.scatter([float(p) for x in range(0, len(fracs))], fracs, color=colors[i])
                #c = 1.96 * scipy.std(fracs)/scipy.sqrt(len(fracs))
                #fig.ax.errorbar(float(p), mean, yerr=c, color=colors[i])
                #interval_data[sources].ax.errorbar(float(p), mean, yerr=c, color=interval_colors[j])
            #ps = sorted(results[sources].keys())
            #if len(ps) !=  len(mean_y):
                #logging.warning('skipping')
                #continue
            #fig.ax.plot(ps, mean_y, label='$%d$' % sources, color=colors[i], marker=markers[i])
            #interval_data[sources].ax.plot(ps, mean_y, label='$%.2f$' % interval, color=interval_colors[j], marker=markers[j])
        #fig.ax.set_ylim(0,1)
        #fig.ax.set_xlim(0,1)
        #fig.legend_title = 'Sources'
        #fig.save('parallel_mean_reachability-interval_%.2f' % interval)
    #for sources in interval_data:
        #fig = interval_data[sources]
        #fig.ax.set_ylim(0,1)
        #fig.ax.set_xlim(0,1)
        #fig.legend_title = 'Interval $[s]$'
        #fig.save('parallel_mean_reachability-sources_%d' % sources)
    #return results

@requires_single_sources
def plot_single_retransmission(options, cursor = None):
    ''' This function allows to plot the reachability for an experiment only
    using a SINGLE source. It is just meant for testing.'''
    logging.info('')
    data = cursor.execute('''
            SELECT e.src, t.p, t.pkg_size, e.iteration, e.reachability
            FROM eval_reachability AS e
            JOIN tag AS t
            ON e.tag_key = t.key
            ORDER BY e.src, t.pkg_size;
        ''').fetchall()

    results = dict()
    for i, (src, p, pkg_size, iteration, reachability) in enumerate(data):
        #logging.info("%d %s %f %d %d %f" % (i, src, p, pkg_size, iteration,
        #                                    reachability))
        if (src, pkg_size) in results.keys():
            if p in results[(src, pkg_size)].keys():
                if iteration in results[(src, pkg_size)][p].keys():
                    results[(src, pkg_size)][p][iteration].append(reachability)
                else:
                    results[(src, pkg_size)][p][iteration] = [reachability]
            else:
                results[(src, pkg_size)][p] = {iteration:[reachability]}
        else:
            results[(src, pkg_size)] = {p:{iteration:[reachability]}}

    markers = options['markers']
    for i, (k, p_dict) in enumerate(results.iteritems()):
        fig = MyFig(options, xlabel='%s,%s [bytes]' % (k[0], k[1]),
                    ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
        for j, (p, iter_dict) in enumerate(sorted(p_dict.iteritems())):
            fig2 = MyFig(options, xlabel='%s,%s [bytes], p=%f' % (k[0], k[1], p),
                    ylabel='Reachability gain',legend=False, grid=False, aspect='auto')
            mean_y = []
            mean_diffs = []
            old_mean = 0
            colors = options['color'](pylab.linspace(0, 1, len(iter_dict.keys())))
            for l, (x, r_list) in enumerate(iter_dict.iteritems()):
                mean = scipy.mean(r_list)
                mean_diffs.append(mean - old_mean)
                old_mean = mean
                mean_y.append(mean)
                c = 1.96 * scipy.std(r_list)/scipy.sqrt(len(r_list))
                fig.ax.errorbar(float(x), mean, yerr=c, color=colors[l])
            fig2.ax.bar(np.arange(0, 10, 1), mean_diffs)
            fig2.ax.set_ylim(0, 1)
            fig2.save("%s-%s-%f-gains" % (k[0],k[1], p))
            fig.ax.plot(np.arange(0, 10, 1), mean_y, label='$%s$' % p,
                        marker=markers[j])
            fig.ax.set_ylim(float(0.2), 1)
            fig.ax.set_xlim(0,9)
        fig.legend_title = "Probabilities"
        fig.save("%s-%s" % (k[0],k[1]))

@requires_parallel_sources
def plot_time_interval_reachability(options, cursor = None):
    logging.info('')
    options['prefix'] = 'time'
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))
    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    ps = list(pylab.flatten(cursor.execute('''SELECT distinct(p) FROM tag ORDER BY p''').fetchall()))
    pkg_sizes = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()))
    restart_deadline = options['deadline']
    for num_xticks in range(1, len(intervals)):
        if len(intervals[::num_xticks]) <= 5:
            break

    for _size_index, pkg_size in enumerate(pkg_sizes):
        logging.info('\tpks_size=%d (%d/%d)' % (pkg_size, _size_index+1, len(pkg_sizes)))
        for _sources_index, num_sources in enumerate(sources):
            logging.info('\t\tsources=%d (%d/%d)' % (num_sources, _sources_index+1, len(sources)))
            pcolor_data = dict()
            duration = restart_deadline
            time_slots = duration/min(intervals)
            for num_yticks in range(1, int(time_slots)):
                if len(range(0, int(time_slots))[::num_yticks]) <= 5:
                    break
            slot_time = min(intervals)
            for p in ps:
                pcolor_data[p] = numpy.zeros((len(intervals), time_slots))
            data = cursor.execute('''
                    SELECT t.p, t.pkg_interval, e.iteration, AVG(e.reachability)
                    FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                    ON e.tag_key = t.key AND t.key = s.tag_key
                    WHERE t.pkg_size=? AND s.sources=?
                    GROUP BY t.p, t.pkg_interval, e.iteration
                    ORDER BY t.p, t.pkg_interval, e.iteration
                ''', (pkg_size, num_sources)).fetchall()

            for p, interval, iteration, reach in data:
                array = pcolor_data[p]
                t1 = (iteration+1)*interval
                t2 = iteration*interval
                if t2 > duration:
                    continue
                t3 = t2*(1.0/min(intervals))
                t4 = t1*(1.0/min(intervals))
                if t3 >= time_slots:
                    continue
                array[intervals.index(interval)][t3:time_slots-1] = reach

            vmin = 0.0
            vmax = 1.0
            logging.info('\t\tprinting')
            if options['grayscale']:
                cmap = options['graycm']
            else:
                cmap = pylab.get_cmap('jet')
            for p in ps:
                if 'right' in options['special']:
                    fig_pcolor = MyFig(options, figsize=(10,8), xlabel=r'Restart Interval $\tau~[s]$', ylabel='', legend=False, grid=False, aspect='auto')
                elif 'left' in options['special']:
                    fig_pcolor = MyFig(options, figsize=(8,8), xlabel=r'Restart Interval $\tau~[s]$', ylabel='Time [s]', legend=False, grid=False, aspect='auto')
                else:
                    fig_pcolor = MyFig(options, figsize=(10,8), xlabel=r'Restart Interval $\tau~[s]$', ylabel='Time [s]', legend=False, grid=False, aspect='auto')
                fig_surface = MyFig(options, figsize=(10,8), xlabel=r'Restart Interval $\tau~[s]$', ylabel='Time [s]', legend=False, grid=False, aspect='auto', ThreeD=True)
                array = numpy.ma.masked_less_equal(pcolor_data[p], 0.0)
                transposed = numpy.transpose(array)
                X, Y = pylab.meshgrid(intervals, numpy.arange(0, time_slots))
                fig_surface.ax.plot_surface(X, Y, transposed, rstride=1, cstride=1, linewidth=1, antialiased=True, cmap=cmap)
                pcolor = fig_pcolor.ax.pcolorfast(transposed, vmin=vmin, vmax=vmax, cmap=cmap) #, edgecolors='none', shading='flat')
                if 'left' in options['special']:
                    pass
                else:
                    fig_pcolor.colorbar(pcolor)
                for fig in [fig_pcolor]:
                    fig.ax.set_xticks(numpy.array(range(0, len(intervals), num_xticks))+0.5)
                    fig.ax.set_xticklabels(intervals[::num_xticks])
                    yticks = numpy.arange(0, time_slots, num_yticks)
                    fig.ax.set_yticks(yticks+0.5)
                    if 'right' in options['special']:
                        fig.ax.set_yticklabels(['' for _x in range(0, len(yticks))])
                    else:
                        fig.ax.set_yticklabels(yticks/time_slots*duration)
                #for fig in [fig_surface]:
                    #fig.ax.w_xaxis.set_ticks(numpy.array(range(0, len(intervals), 5))+0.5)
                    #fig.ax.w_xaxis.set_ticklabels(intervals[::5])
                    #yticks = numpy.arange(0, time_slots, duration)
                    #fig.ax.w_yaxis.set_ticks(yticks+0.5)
                    #fig.ax.w_yaxis.set_ticklabels(yticks/time_slots*duration)
                fig_pcolor.save('pcolor-srcs_%d-pkgsize_%d-p_%.2f' % (num_sources, pkg_size, p))
                fig_surface.save('surface-srcs_%d-pkgsize_%d-p_%.2f' % (num_sources, pkg_size, p))

@requires_parallel_sources
def plot_time_over_frac(options, cursor = None):
    logging.info('')
    options['prefix'] = 'maxtime'
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))
    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    ps = list(pylab.flatten(cursor.execute('''SELECT distinct(p) FROM tag ORDER BY p''').fetchall()))
    pkg_sizes = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()))


    for _size_index, pkg_size in enumerate(pkg_sizes):
        logging.info('\tpks_size=%d (%d/%d)' % (pkg_size, _size_index+1, len(pkg_sizes)))
        for _interval_index, interval in enumerate(intervals):
            logging.info('\tinterval=%.3f (%d/%d)' % (interval, _interval_index+1, len(intervals)))
            for _sources_index, num_sources in enumerate(sources):
                logging.info('\t\t\tsources=%d (%d/%d)' % (num_sources, _sources_index+1, len(sources)))
                for _p_index, p in enumerate(ps):
                    logging.info('\t\t\t\tp=%.2f (%d/%d)' % (p, _p_index+1, len(ps)))
                    tags = cursor.execute('''
                        SELECT key
                        FROM tag JOIN eval_sources_per_tag
                        ON tag.key = eval_sources_per_tag.tag_key
                        WHERE p=? AND eval_sources_per_tag.sources=? AND pkg_size=? AND pkg_interval=?
                    ''', (p, num_sources, pkg_size, interval))
                    tags_string = ','.join('%d' % d for d in pylab.flatten(tags))

                    query = '''
                        SELECT FF, T.max_time
                        FROM eval_time AS T JOIN (
                            SELECT F.tag_key AS FT, F.frac AS FF
                            FROM eval_rx_fracs_for_tag AS F
                            WHERE F.tag_key IN (%s)
                            AND FF > 0.7
                        ) as F
                        ON T.tag_key = F.FT
                        ORDER BY FF
                    ''' % (tags_string)
                    raw_data = cursor.execute(query).fetchall()
                    if len(raw_data) == 0:
                        continue
                    fig_scatter = MyFig(options, figsize=(10,8), xlabel=r'Reachability~$\reachability$', ylabel='Time [$s$]', legend=False, grid=False, aspect='auto')
                    fig_scatter.ax.scatter([x for x, y in raw_data], [y for x, y in raw_data])
                    fig_scatter.save('scatter-srcs_%d-pkgsize_%d-interval_%.3f-p_%.2f' % (num_sources, pkg_size, interval, p))

@requires_parallel_sources
def plot_parallel_retransmission(options, cursor = None):
    ''' This function allows to plot the reachability for an experiment only
    using PARALLEL sources.'''
    logging.info('')

    intervals = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()))
    if options['intervals']:
        intervals = [i for i in intervals if i in options['intervals']]
    sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    interval_figs = dict.fromkeys([s for s, in sources],  MyFig(options, xlabel='Probability $p_s$', ylabel='Forwarded Packets (normalized)', legend=True, grid=False, aspect='auto'))
    if options['grayscale']:
        interval_colors = options['graycm'](pylab.linspace(0, 1, len(intervals)))
    else:
        interval_colors = options['color'](pylab.linspace(0, 1, len(intervals)))
    markers = options['markers']

    for j, interval in enumerate(intervals):
        if options['restarts'] == numpy.infty:
            data = cursor.execute('''
                SELECT s.sources, t.pkg_size, t.p, e.iteration, e.reachability, e.gain
                FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                ON e.tag_key = t.key AND t.key = s.tag_key
                WHERE t.pkg_interval=?
                ORDER BY s.sources
            ''', (interval,)).fetchall()
        else:
            data = cursor.execute('''
                SELECT s.sources, t.pkg_size, t.p, e.iteration, e.reachability, e.gain
                FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                ON e.tag_key = t.key AND t.key = s.tag_key
                WHERE t.pkg_interval=? AND e.iteration <= ?
                ORDER BY s.sources
            ''', (interval, options['restarts'])).fetchall()

        results = dict()
        logging.info('preparing data')
        for num_sources, pkg_size, p, iteration, reachability, gain in data:
            if (num_sources, pkg_size) in results.keys():
                if p in results[(num_sources, pkg_size)].keys():
                    if iteration in results[(num_sources, pkg_size)][p].keys():
                        results[(num_sources, pkg_size)][p][iteration].append((reachability, gain))
                    else:
                        results[(num_sources, pkg_size)][p][iteration] = [(reachability, gain)]
                else:
                    results[(num_sources, pkg_size)][p] = {iteration:[(reachability, gain)]}
            else:
                results[(num_sources, pkg_size)] = {p:{iteration:[(reachability, gain)]}}

        logging.info('plotting')
        for i, ((num_sources, pkg_size), p_dict) in enumerate(results.iteritems()):
            if 'right' in options['special']:
                fig_reachability_over_retransmission = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel='',legend=True, grid=False, aspect='auto')
            elif 'left' in options['special']:
                fig_reachability_over_retransmission = MyFig(options, figsize=(8,8), xlabel='Restarts', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            else:
                fig_reachability_over_retransmission = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            fig_gains_line = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel='Reachability Gain',legend=True, grid=False, aspect='auto')
            if options['grayscale']:
                p_colors = options['graycm'](pylab.linspace(0, 1, len(p_dict)))
            else:
                p_colors = options['color'](pylab.linspace(0, 1, len(p_dict)))
            for k, (p, iter_dict) in enumerate(sorted(p_dict.iteritems())):
                max_retransmissions = max([_i for _s, _t, _p, _i, _e, _g in data if _p == p and num_sources == _s and  pkg_size == _t])
                fig_gains_bar = MyFig(options, figsize=(10,8), xlabel='Restarts', ylabel='Reachability Gain', legend=False, grid=False, aspect='auto')
                fig_gains_dist = MyFig(options, figsize=(10,8), xlabel='Reachability Gain', ylabel='Restarts', legend=False, grid=False, aspect='auto')
                fig_gains_cdf = MyFig(options, figsize=(10,8), xlabel='Reachability Gain', ylabel='CDF', legend=True, grid=False, aspect='auto')
                fig_gains_cdf.legend_title = 'Restarts'
                fig_gains_pdf = MyFig(options, figsize=(10,8), xlabel='Reachability Gain', ylabel='Density', legend=True, grid=False, aspect='auto')
                fig_gains_pdf.legend_title = 'Restarts'
                fig_gains_pdf3d = MyFig(options, figsize=(10,8), xlabel='Reachability Gain', ylabel='Restart', zlabel='Density', legend=False, grid=False, aspect='auto', ThreeD=True)
                fig_gains_hist3d = MyFig(options, figsize=(10,8), xlabel='Reachability Gain', ylabel='Restart', zlabel='Relative Frequency', legend=False, grid=False, aspect='auto', ThreeD=True)

                mean_y = list()
                mean_diffs = list()
                confs = list()
                prev_mean = 0
                if options['grayscale']:
                    colors = options['graycm'](pylab.linspace(0, 1, len(iter_dict.keys())))
                else:
                    colors = options['color'](pylab.linspace(0, 1, len(iter_dict.keys())))
                gains = None
                for j, (iteration, r_and_g) in enumerate(iter_dict.iteritems()):
                    r_list = [r for r, g in r_and_g]
                    g_list = [g for r, g in r_and_g]
                    probabilities = pylab.linspace(0, 1, 1000)
                    #########
                    kernel = kde.gaussian_kde(g_list)
                    #kernel.covariance_factor = lambda : .1
                    #kernel._compute_covariance()
                    density = kernel(probabilities)
                    #print 'integral=%f' % kernel.integrate_box_1d(0, 1)
                    fig_gains_pdf.ax.plot(probabilities, density, color=colors[j], label='$%d$' % iteration)
                    fig_gains_pdf3d.ax.plot(probabilities, [iteration for x in probabilities], density, color=colors[iteration])
                    #########
                    X, Y = data2hist(g_list, bins=options['bins'], range=(0,1))
                    fig_gains_hist3d.ax.plot(X, [iteration for x in X], Y, color=colors[iteration])
                    #########
                    percentiles = scipy.stats.mstats.mquantiles(g_list, prob=probabilities)
                    fig_gains_cdf.ax.plot(percentiles, probabilities, color=colors[iteration], label='$%d$' % iteration)
                    #########
                    gain_dist, bin_edges = numpy.histogram(g_list, range=(0.0, 1.0), bins=options['bins'])
                    gain_dist = gain_dist/float(len(g_list))
                    gain_std = scipy.std(g_list)
                    gain_mean = scipy.mean(g_list)
                    if gains == None:
                        gains = gain_dist
                    else:
                        gains = numpy.vstack((gains, gain_dist))
                    mean = scipy.mean(r_list)
                    mean_diffs.append(mean - prev_mean)
                    prev_mean = mean
                    mean_y.append(mean)
                    conf = confidence(r_list)[2]
                    confs.append(conf)

                    #fig_reachability_over_retransmission.ax.errorbar(float(iteration), mean, yerr=conf, color=p_colors[k])
                X, Y = pylab.meshgrid(pylab.linspace(0, 1, options['bins']),  range(0, max_retransmissions+2))
                print X.shape, Y.shape, gains.shape
                #pcolor = fig_gains_dist.ax.pcolorfast(X, Y, gains, cmap=fu_colormap(), vmin=0.0, vmax=1.0) #, edgecolor='none', shading='flat')
                #cbar = fig_gains_dist.colorbar(pcolor)
                fig_gains_dist.save("parallel_retransmission-gain-distribution-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))
                fig_gains_cdf.save("parallel_retransmission-gain-cdf-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))
                fig_gains_pdf.save("parallel_retransmission-gain-pdf-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))

                #fig_gains_pdf3d.ax.set_zlim3d(0, 1)
                fig_gains_pdf3d.save("parallel_retransmission-gain-pdf3d-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))
                fig_gains_hist3d.ax.set_zlim3d(0, 1)
                fig_gains_hist3d.save("parallel_retransmission-gain-hist3d-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))

                fig_gains_line.ax.plot(range(0, max_retransmissions+1, 1), mean_diffs, label='$%s$' % p, marker=markers[k], color=p_colors[k])
                fig_gains_bar.ax.bar(range(0, max_retransmissions+1, 1), mean_diffs, align='center', facecolor=colors[k])
                fig_gains_bar.ax.set_ylim(0, 1)
                fig_gains_bar.ax.set_xlim(0, max_retransmissions)
                fig_gains_bar.save("parallel_retransmission-gains-%d_srcs-%.2f_p-interval_%.2f" % (num_sources, p, interval))

                poly = [conf2poly(np.arange(0, max_retransmissions+1, 1), list(numpy.array(mean_y)+numpy.array(confs)), list(numpy.array(mean_y)-numpy.array(confs)), color=p_colors[k])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                patch_collection.set_linestyle('dashed')
                fig_reachability_over_retransmission.ax.add_collection(patch_collection)
                fig_reachability_over_retransmission.ax.plot(np.arange(0, max_retransmissions+1, 1), mean_y, label='$%.2f$' % float(p), color=p_colors[k])

            fig_gains_line.legend_title = "Probability"
            fig_gains_line.save("parallel_retransmission-gains-%d_srcs-interval_%.2f" % (num_sources, interval))
            fig_reachability_over_retransmission.ax.set_ylim(float(0), 1)
            fig_reachability_over_retransmission.ax.set_xlim(0, max_retransmissions)
            fig_reachability_over_retransmission.legend_title = "Probability"
            fig_reachability_over_retransmission.save("parallel_retransmission-%d_srcs-pkgsize_%-d-interval_%.2f" % (num_sources, pkg_size, interval))

@requires_parallel_sources
def plot_parallel_convergence(options, cursor = None):
    logging.info('')
    pkg_sizes = cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()
    ps = cursor.execute('''SELECT distinct(p) FROM tag ORDER BY p''').fetchall()
    num_sources = cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()
    pkg_intervals = cursor.execute('''SELECT distinct(pkg_interval) FROM tag ORDER BY pkg_interval''').fetchall()
    markers = options['markers']*10

    for _sources_index, (sources,) in enumerate(num_sources):
        logging.info('sources=%d (%d/%d)' % (sources, _sources_index+1, len(num_sources)))
        for _size_index, (pkg_size,) in enumerate(pkg_sizes):
            logging.info('\tpks_size=%d (%d/%d)' % (pkg_size, _size_index+1, len(pkg_sizes)))
            fig = MyFig(options, xlabel='Restarts until Convergence', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            fig2 = MyFig(options, xlabel='Time until Convergence $[s]$', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            selected_intervals = options['intervals']
            if not selected_intervals:
                selected_intervals = [_i for _i, in pkg_intervals]
            max_num_restarts = 40/min(selected_intervals)
            colors = options['color'](pylab.linspace(0, 1, len(selected_intervals)))
            counter = -1
            for i, (interval,) in enumerate(pkg_intervals):
                logging.info('\t\tinterval=%.3f (%d/%d)', interval, i+1, len(pkg_intervals))
                if not interval in selected_intervals:
                    logging.warning('skipping interval %.3f' % interval)
                    continue
                counter += 1
                interval_data = list()
                for p, in ps:
                    max_avg_reachability, = cursor.execute('''
                        SELECT MAX(R) FROM (
                            SELECT e.iteration AS I, AVG(e.reachability) AS R
                            FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                            ON e.tag_key = t.key AND t.key = s.tag_key
                            WHERE s.sources=? AND t.pkg_size=? AND t.pkg_interval=? AND t.p=?
                            GROUP BY e.iteration
                        )
                    ''', (sources, pkg_size, interval, p)).fetchone()
                    data = cursor.execute('''
                        SELECT e.iteration, AVG(e.reachability)
                        FROM eval_reachability AS e JOIN tag AS t JOIN eval_sources_per_tag as s
                        ON e.tag_key = t.key AND t.key = s.tag_key
                        WHERE s.sources=? AND t.pkg_size=? AND t.pkg_interval=? AND t.p=?
                        GROUP BY e.iteration
                        ORDER BY e.iteration
                    ''', (sources, pkg_size, interval, p)).fetchall()
                    if not len(data):
                        logging.warning('no data found')
                        continue
                    for iteration in range(0, len(data)):
                        if abs(max_avg_reachability - data[iteration][1]) < 0.01:
                            break
                    interval_data.append((p, iteration+1, max_avg_reachability))
                if not len(interval_data):
                    logging.warning('no data found for interval=%.2f' % interval)
                    continue
                fig.ax.plot([itera for _p, itera, _r in interval_data], [reach for _p, _i, reach in interval_data], label='%.2f' % interval, color=colors[counter], marker=markers[counter])
                for (x, y, p) in zip([itera for _p, itera, _r in interval_data], [reach for _p, _i, reach in interval_data], [p for p, _i, _reach in interval_data]):
                    fig.ax.text(x, y, '%.2f' % p, color=colors[counter])
                fig2.ax.plot([itera*interval for _p, itera, _r in interval_data], [reach for _p, _i, reach in interval_data], label='%.2f' % interval, color=colors[counter], marker=markers[counter])
            fig.legend_title = r"Interval $\tau~[s]$"
            fig.ax.set_ylim(0.9, 1.0)
            fig.save("parallel_convergence-%d_srcs-pkgsize_%-d" % (sources, pkg_size))
            fig2.legend_title = r"Interval $\tau~[s]$"
            fig2.ax.set_ylim(0.9, 1.0)
            fig2.save("parallel_convergence2-%d_srcs-pkgsize_%-d" % (sources, pkg_size))

@requires_parallel_sources
def plot_reachability_over_interval(options, cursor = None):
    '''Evaluate the reachability per p for different pkg_intervals as an application of the restart problem'''
    logging.info('')
    data = cursor.execute('''
        SELECT DISTINCT(pkg_interval)
        FROM tag
        ORDER BY pkg_interval;
        ''').fetchall()
    if len(data) <= 1:
        logging.warning('no different intervals found')

    num_sources = list(pylab.flatten(cursor.execute('''
        SELECT DISTINCT(sources)
        FROM eval_sources_per_tag
        ORDER BY sources;
        ''').fetchall()))

    ps = list(pylab.flatten(cursor.execute('''
        SELECT DISTINCT(p)
        FROM tag
        ORDER BY p;
        ''').fetchall()))

    pkg_sizes = list(pylab.flatten(cursor.execute('''
        SELECT DISTINCT(pkg_size)
        FROM tag
        ORDER BY pkg_size;
        ''').fetchall()))

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(ps)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(ps)))
    markers = options['markers']
    for pkg_size in pkg_sizes:
        for i, srcs in enumerate(num_sources):
            logging.info('sources=%s (%d/%d)', srcs, i+1, len(num_sources))
            if 'right' in options['special']:
                fig = MyFig(options, figsize=(8,8), xlabel=r'Restart Interval $\tau$ [s]', ylabel='',legend=True, grid=False, aspect='auto')
            elif 'left' in options['special']:
                fig = MyFig(options, figsize=(8,8), xlabel=r'Restart Interval $\tau$ [s]', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            else:
                fig = MyFig(options, figsize=(10,8), xlabel=r'Restart Interval $\tau$ [s]', ylabel=r'Reachability~$\reachability$',legend=True, grid=False, aspect='auto')
            for j, p in enumerate(ps):
                if options['deadline'] == numpy.infty:
                    data = cursor.execute('''
                        SELECT tag.pkg_interval, eval_reachability.reachability, eval_reachability.iteration
                        FROM eval_reachability JOIN tag JOIN eval_sources_per_tag
                        ON eval_reachability.tag_key = tag.key AND eval_reachability.tag_key = tag.key AND eval_sources_per_tag.tag_key = tag.key
                        WHERE tag.p=? AND eval_sources_per_tag.sources=? AND tag.pkg_size=?
                        GROUP BY eval_reachability.tag_key, eval_reachability.src
                        HAVING MAX(eval_reachability.iteration)
                        ORDER BY tag.pkg_interval
                    ''', (p, srcs, pkg_size)).fetchall()
                else:
                    data = cursor.execute('''
                        SELECT tag.pkg_interval, eval_reachability.reachability, eval_reachability.iteration*tag.pkg_interval AS rtime
                        FROM eval_reachability JOIN tag JOIN eval_sources_per_tag
                        ON eval_reachability.tag_key = tag.key AND eval_reachability.tag_key = tag.key AND eval_sources_per_tag.tag_key = tag.key
                        WHERE tag.p=? AND eval_sources_per_tag.sources=? AND tag.pkg_size=? AND rtime <= ?
                        GROUP BY eval_reachability.tag_key, eval_reachability.src
                        ORDER BY tag.pkg_interval
                    ''', (p, srcs, pkg_size, options['deadline'])).fetchall()

                fractions = dict()
                for interval, fraction, sfsdfsdf in data:
                    try:
                        fractions[interval].append(fraction)
                    except KeyError:
                        fractions[interval] = [fraction]
                means = list()
                errors = list()
                for interval in sorted(fractions.keys()):
                    means.append(scipy.mean(fractions[interval]))
                    c = 1.96 * scipy.std(fractions[interval])/scipy.sqrt(len(fractions[interval]))
                    errors.append(c)
                if len(means)==0 or len(fractions.keys())==0:
                    continue
                fig.ax.plot(sorted(fractions.keys()), means, label='$%.2f$' % p, color=colors[j]) #, marker=markers[j])

                poly = [conf2poly(sorted(fractions.keys()), list(numpy.array(means)+numpy.array(errors)), list(numpy.array(means)-numpy.array(errors)), color=colors[j])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                patch_collection.set_linestyle('dashed')
                fig.ax.add_collection(patch_collection)
                #fig.ax.errorbar(sorted(fractions.keys()), means, yerr=errors, color=colors[j], marker=markers[j])
            fig.ax.set_ylim(0, 1)
            fig.legend_title = "Probability"
            suffix = 'max'
            if 'right' in options['special']:
                fig.ax.set_yticklabels(['' for _y in range(0, len(fig.ax.get_yticklabels()))])
            xticks = fig.ax.get_xticks()
            for num_xticks in range(1, len(xticks)):
                if len(xticks[::num_xticks]) <= 5:
                    break
            fig.ax.set_xticks(xticks[::num_xticks])
            fig.ax.set_xlim(min(xticks), max(xticks))
            if options['deadline'] != numpy.infty:
                suffix = '%.2f' % options['deadline']
            fig.save("parallel_reachability_interval-%d_srcs-pkgsize_%d-%s" % (srcs, pkg_size, suffix))

@requires_parallel_sources
def plot_gossip13_reachability(options, cursor=None):
    '''Plots the reachability when multiple sources sent packets at the same time.
       The reachability is plottet as function over p'''
    logging.info('')
    options['prefix'] = 'all'

    pkg_sizes = list(pylab.flatten(cursor.execute('''SELECT distinct(pkg_size) FROM tag ORDER BY pkg_size''').fetchall()))
    taus = list(pylab.flatten(cursor.execute('''SELECT distinct(tau) FROM tag ORDER BY tau''').fetchall()))
    restarts = list(pylab.flatten(cursor.execute('''SELECT distinct(restarts) FROM tag ORDER BY restarts''').fetchall()))
    sources = list(pylab.flatten(cursor.execute('''SELECT distinct(sources) FROM eval_sources_per_tag ORDER BY sources''').fetchall()))

    if options['grayscale']:
        tau_colors = options['graycm'](pylab.linspace(0, 1, len(taus)))
        restarts_colors = options['graycm'](pylab.linspace(0, 1, len(restarts)))
        sources_colors = options['graycm'](pylab.linspace(0, 1, len(sources)))
    else:
        tau_colors = options['color'](pylab.linspace(0, 1, len(taus)))
        restarts_colors = options['color'](pylab.linspace(0, 1, len(restarts)))
        sources_colors = options['color'](pylab.linspace(0, 1, len(sources)))

    for i, pkg_size in enumerate(pkg_sizes):
        raw_data = cursor.execute('''
            SELECT s.sources, t.tau, t.restarts, AVG(e.frac)
            FROM tag as t JOIN eval_rx_fracs_for_tag AS e JOIN eval_sources_per_tag AS s
            ON t.key = e.tag_key AND t.key = s.tag_key
            WHERE t.pkg_size=?
            GROUP BY t.key
            ORDER BY s.sources
        ''', (pkg_size,)).fetchall()

        for tau in taus:
            fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', grid=False, legend=True, aspect='auto')
            for j, restart in enumerate(restarts):
                data = [(n, r) for n, t, d, r in raw_data if tau==t and d==restart]
                means = list()
                confs = list()
                for num_sources in sources:
                    ndata = [r for n, r in data if n==num_sources]
                    means.append(scipy.mean(ndata))
                    confs.append(confidence(ndata)[2])
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=restarts_colors[j])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                fig.ax.add_collection(patch_collection)
                fig.ax.plot(sources, means, color=restarts_colors[j], label='%d' % (restart))
            fig.ax.set_xticks(sources)
            fig.ax.set_xlim(min(sources), max(sources))
            fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            fig.ax.set_ylim(0, 1)
            fig.legend_title = 'restarts'
            fig.save('parallel_reachability_gossip13_%dpkgsize_%dtau' % (pkg_size, tau))

        for restart in restarts:
            fig = MyFig(options, figsize=(10, 8), xlabel=r'Sources~$\sources$', ylabel=r'Reachability~$\reachability$', grid=False, legend=True, aspect='auto')
            for j, tau in enumerate(taus):
                data = [(n, r) for n, t, d, r in raw_data if tau==t and d==restart]
                means = list()
                confs = list()
                for num_sources in sources:
                    ndata = [r for n, r in data if n==num_sources]
                    means.append(scipy.mean(ndata))
                    confs.append(confidence(ndata)[2])
                poly = [conf2poly(sources, list(numpy.array(means)+numpy.array(confs)), list(numpy.array(means)-numpy.array(confs)), color=tau_colors[j])]
                patch_collection = PatchCollection(poly, match_original=True)
                patch_collection.set_alpha(0.3)
                fig.ax.add_collection(patch_collection)
                fig.ax.plot(sources, means, color=tau_colors[j], label='%d' % (tau))
            fig.ax.set_xticks(sources)
            fig.ax.set_xlim(min(sources), max(sources))
            fig.ax.set_yticks(numpy.arange(0.2, 1.1, 0.2))
            fig.ax.set_ylim(0, 1)
            fig.legend_title = r'$\tau$ [ms]'
            fig.save('parallel_reachability_gossip13_%dpkgsize_%drestarts' % (pkg_size, restart))

