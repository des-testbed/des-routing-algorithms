#!/usr/bin/env python

import matplotlib
import argparse
import pylab
import scipy
import os
import numpy
from helpers import *
from matplotlib.patches import Ellipse
from matplotlib.collections import PatchCollection
from mpl_toolkits.axes_grid1.inset_locator import zoomed_inset_axes
from mpl_toolkits.axes_grid1.inset_locator import mark_inset

names = [('uu-results', 'undirected \& reliable'), ('du-results', 'directed \& reliable'), ('dw-results', 'directed \& lossy')]

def parse_site_only(path, name):
    data_file = open('%s/%s' % (path, name), 'r')
    data = list()
    for line in data_file:
        if line.startswith('#') or line.isspace():
            continue
        tokens = line.split(':')[1].split(',')
        if len(tokens) == 9:
            ps, pb, r, std, conf, n, fw, fw_std, fw_conf = tokens
            data.append((float(ps), float(r), float(std), float(conf), int(n), float(fw), float(fw_std), float(fw_conf)))
        else:
            ps, pb, r, std, conf, n = tokens
            data.append((float(ps), float(r), float(std), float(conf), int(n), 0, 0, 0))
    data = sorted(data, key=lambda t: t[0])
    return data

def plot_site(options):
    options['prefix'] = 'site'
    fig = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel=r'Probability $p_s$', ylabel=r'Reachability~$\reachability$', aspect='auto')
    fig_vs = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel=r'Fraction of Forwarded Packets~$\forwarded$', ylabel=r'Reachability~$\reachability$', aspect='auto')

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, 3))
    else:
        colors = fu_colormap()(pylab.linspace(0, 1.0, 3))
    nodes = 105

    axins = None
    if options['inset_loc'] >= 0:
        axins = zoomed_inset_axes(fig_vs.ax, options['inset_zoom'], loc=options['inset_loc'])
        axins.set_xlim(options['inset_xlim'])
        axins.set_ylim(options['inset_ylim'])
        axins.set_xticklabels([])
        axins.set_yticklabels([])
        mark_inset(fig_vs.ax, axins, loc1=2, loc2=3, fc="none", ec="0.5")

    for i, (name, label) in enumerate(names):
        data = parse_site_only(options['datapath'][0], name)
        rs = numpy.array([r for p, r, std, conf, n, fw, fw_std, fw_conf in data if r <= options['limit']])
        fws = numpy.array([fw for p, r, std, conf, n, fw, fw_std, fw_conf in data if r <= options['limit']])/(nodes-1)
        ps = numpy.array([p for p, r, std, conf, n, fw, fw_std, fw_conf in data]) #[0:len(rs)])
        yerr = numpy.array([conf for p,r,std,conf, n, fw, fw_std, fw_conf in data]) #[0:len(rs)])

        patch_collection = PatchCollection([conf2poly(ps, list(rs+yerr), list(rs-yerr), color=colors[i])], match_original=True)
        patch_collection.set_alpha(0.3)
        patch_collection.set_linestyle('dashed')
        fig.ax.add_collection(patch_collection)

        fig.ax.plot(ps, rs, label=label, color=colors[i])
        fig_vs.ax.plot(fws, rs, label=label, color=colors[i])
        if axins:
            axins.plot(fws, rs, color=colors[i])

    fig.ax.set_ylim(0, options['limit'])
    fig.legend_title = 'Graph'
    fig.save('graphs')
    fig_vs.legend_title = 'Graph'
    fig_vs.ax.set_xlim(0,1)
    fig_vs.ax.set_ylim(0,1)
    fig_vs.save('vs_pb')

def plot_event_vs_set(options):
    options['prefix'] = 'site'
    fig = MyFig(options, figsize=(10, 8), legend=True, grid=False, xlabel=r'Discrete-Event-based', ylabel=r'Round-based', aspect='auto')
    fig.ax.plot([0,1], [0,1], color=matplotlib.rcParams['grid.color'], ls=matplotlib.rcParams['grid.linestyle'], linewidth=matplotlib.rcParams['grid.linewidth'])

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, 3))
    else:
        colors = fu_colormap()(pylab.linspace(0, 1.0, 3))
    for i, (name, label) in enumerate(names):
        data1 = parse_site_only(options['datapath'][0], name)
        r1 = numpy.array([r for p, r, std, conf, n, fw, fw_std, fw_conf in data1 if r <= options['limit']])
        data2 = parse_site_only(options['datapath'][1], name)
        r2 = numpy.array([r for p, r, std, conf, n, fw, fw_std, fw_conf in data2 if r <= options['limit']])
        fig.ax.plot(r1, r2, label=label, color=colors[i])

    fig.ax.set_xlim(0, 1)
    fig.ax.set_ylim(0, options['limit'])
    fig.save('vs')

def plot_simu_vs_testbed(options):
    options['prefix'] = 'site'
    fig = MyFig(options, figsize=(10, 8), legend=False, grid=False, xlabel=r'Simulation', ylabel=r'Testbed', aspect='auto')
    fig.ax.plot([0,1], [0,1], color=matplotlib.rcParams['grid.color'], ls=matplotlib.rcParams['grid.linestyle'], linewidth=matplotlib.rcParams['grid.linewidth'])

    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, 2))
    else:
        colors = fu_colormap()(pylab.linspace(0, 1.0, 2))
    simu = parse_site_only(options['datapath'][0], 'dw-results')
    r1 = numpy.array([r for p, r, std, conf, n, fw, fw_std, fw_conf in simu if r <= options['limit']])
    testbed = parse_site_only(options['datapath'][1], 'testbed-results')
    r2 = numpy.array([r for p, r, std, conf, n, fw, fw_std, fw_conf in testbed if r <= options['limit']])
    fig.ax.plot(r1, r2, color=colors[0])

    fig.ax.set_xlim(0, 1)
    fig.ax.set_ylim(0, options['limit'])
    fig.save('simu_vs_testbed')

def calc_metrics(merge):
    data = list()
    for (ps, pb) in merge.keys():
        results = merge[(ps, pb)]
        reachability = [r for _ps, _pb, r, std, _conf, _n, _fw, fw_std, _fw_conf in results]
        fws = [fw for _ps, _pb, _r, std, _conf, _n, fw, _fw_std, _fw_conf in results]
        sample_size = list(set([n for _ps, _pb, _r, _std, _conf, n, _fw, _fw_std, _fw_conf in results]))
        assert(len(sample_size) == 1)
        sample_size = sample_size[0]
        reachability_mean = scipy.mean(reachability)
        fw_mean = scipy.mean(fws)
        if len(reachability) > 1:
            reachability_std  = scipy.std(reachability)
            fw_std  = scipy.std(fws)
        else:
            reachability_std   = std
            fw_std             = fw_std
        m = sum([n for _ps, _pb, _r, _std, _conf, n, _fw, _fw_std, _fw_conf in results])
        if m == 0:
            print 'WARNING: number of replications is zero'
        reachability_conf = 1.96*reachability_std/scipy.sqrt(m)
        fw_conf = 1.96*fw_std/scipy.sqrt(m)
        data.append((ps, pb, reachability_mean, reachability_std, reachability_conf, sample_size, fw_mean, fw_std, fw_std))
    return sorted(data)

def plot_bond_site(options):
    options['prefix'] = 'bond_site'
    epsilon = numpy.finfo(numpy.float).eps

    bins = 11
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, bins))
    else:
        colors = options['color'](pylab.linspace(0, 1.0, bins))
    try:
        data_files = [(open('%s/results' % (options['datapath'][0]), 'r'), '')]
    except IOError:
        dirList = os.listdir(options['datapath'][0])
        files = [f for f in dirList if f.startswith('results')]
        data_files = [(open('%s/%s' % (options['datapath'][0], f), 'r'), f.replace('results', '')) for f in files]

    for (data_file, suffix) in data_files:
        bond_site = False
        max_cluster = False
        merge = dict()
        for line_num, line in enumerate(data_file):
            if line_num < 100 and line.find('bond-site') >= 0:
                bond_site = True
            if line_num < 10 and line.find('maximum cluster size') >=0:
                max_cluster = True
            if line.startswith('#') or line.isspace():
                continue
            tokens = line.split(':')[1].split(',')
            # ps, pb, reachability, std, conf, replications
            if len(tokens) == 9:
                ps, pb, r, std, conf, n, fw, fw_std, fw_conf = tokens
                t = ((float(ps), float(pb), float(r), float(std), float(conf), int(n), float(fw), float(fw_std), float(fw_conf)))
            else:
                ps, pb, r, std, conf, n = tokens
                t = ((float(ps), float(pb), float(r), float(std), float(conf), int(n), 0.0, 0.0, 0.0))

            try:
                merge[(float(tokens[0]), float(tokens[1]))].append(t)
            except KeyError:
                merge[(float(tokens[0]), float(tokens[1]))] = [t]

        fig_contour = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Probability $p_b$', aspect='auto')
        fig_pcolor = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Probability $p_b$', aspect='auto')
        fig_vs_pb = None
        fig_vs_ps = None
        if bond_site:
            if max_cluster:
                ylabel= 'Max. Percolation Cluster Size'
            else:
                ylabel= 'Mean Percolation Cluster Size'
        else:
            fig_vs_pb = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Fraction of Forwarded Packets~$\forwarded$', ylabel=r'Reachability~$\reachability$', aspect='auto')
            fig_vs_ps = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Fraction of Forwarded Packets~$\forwarded$', ylabel=r'Reachability~$\reachability$', aspect='auto')
            fig_fw_pb = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Probability $p_s$', ylabel=r'Fraction of Forwarded Packets~$\forwarded$', aspect='auto')
            fig_fw_ps = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Probability $p_b$', ylabel=r'Fraction of Forwarded Packets~$\forwarded$', aspect='auto')
            ylabel = r'Reachability~$\reachability$'
        fig_site = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Probability $p_s$', ylabel=ylabel, aspect='auto')
        fig_bond = MyFig(options, figsize=(10, 8), legend=(not options['no_legend']), grid=False, xlabel=r'Probability $p_b$', ylabel=ylabel, aspect='auto')

        def fit_data(X, Y, options):
            init_params = [1.0, 0.0, 0.0, 1.0]
            hyperbolic2 = lambda (a,b,c,d), x: 1/(map(lambda _x: a*_x, x)+b)    # f(x) = 1/(ax+b)
            hyperbolic4 = lambda (a,b,c,d), x: d/(map(lambda _x: a*_x, x)+b)+c  # f(x) = d/(ax+b)+c
            exponential = lambda (a,b,c,d), x: scipy.e**(a*(x+b))+c             # f(x) = e^(-a(x+b))+c
            fitfunc = locals()[options['fit']]
            errfunc = lambda (a,b,c,d), x, y: y-fitfunc((a,b,c,d), x)
            return scipy.optimize.leastsq(errfunc, init_params, args=(X, Y)), fitfunc

        def contour_plot(tuples):
            X = pylab.linspace(0, 1, scipy.sqrt(len(tuples)))
            Y = pylab.linspace(0, 1, scipy.sqrt(len(tuples)))
            Z = numpy.array([r for p, pb, r in tuples]).reshape((len(X), len(Y))).transpose()
            X, Y = pylab.meshgrid(X, Y)
            if options['grayscale']:
                pcolor = fig_contour.ax.contourf(X, Y, Z, levels=pylab.linspace(0, 1, 101), cmap=options['graycm'], antialiased=True, linestyles=None)
            else:
                pcolor = fig_contour.ax.contourf(X, Y, Z, levels=pylab.linspace(0, 1, 101), cmap=fu_colormap(), antialiased=True, linestyles=None)
            cbar = fig_contour.colorbar(pcolor)
            cbar.set_ticks(pylab.linspace(0, 1, 11))

            if options['grayscale']:
                pcolor = fig_pcolor.ax.pcolormesh(X, Y, Z, vmin=0, vmax=1, cmap=options['graycm'])
            else:
                pcolor = fig_pcolor.ax.pcolormesh(X, Y, Z, vmin=0, vmax=1, cmap=fu_colormap())
            cbar = fig_pcolor.colorbar(pcolor)
            cbar.set_ticks(pylab.linspace(0, 1, 11))

        data = calc_metrics(merge)
        tuples = [(ps, pb, r) for ps, pb, r, _std, _conf, _n, _fw_mean, _fw_std, _fw_std in data]
        contour_plot(tuples)
        nodes = options['nodes']

        for i, bin in enumerate(pylab.linspace(0, 1, bins)):
            r_pb1       = numpy.array([r for _ps, pb, r, _std, _conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])
            fw_pb1      = numpy.array([fw_mean for _ps, pb, _r, _std, _conf, _n, fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])/nodes
            fw_conf_pb1 = numpy.array([fw_conf for _ps, pb, _r, _std, _conf, _n, fw_mean, _fw_std, fw_conf in data if abs(pb - bin) < epsilon*2])/nodes
            conf_pb1    = numpy.array([conf for _ps, pb, _r, _std, conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])
            p_pb1       = numpy.array([ps for ps, pb, _r, _std, _conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])

            r_ps1       = numpy.array([r for ps, _pb, r, _std, _conf, n, _fw_mean, _fw_std, _fw_conf in data if abs(ps - bin) < epsilon*2])
            fw_ps1      = numpy.array([fw_mean for ps, _pb, r, _std, _conf, n, fw_mean, _fw_std, _fw_conf in data if abs(ps - bin) < epsilon*2])/nodes
            fw_conf_ps1 = numpy.array([fw_conf for ps, _pb, r, _std, _conf, n, fw_mean, _fw_std, fw_conf in data if abs(ps - bin) < epsilon*2])/nodes
            conf_ps1    = numpy.array([conf for ps, _pb, _r, _std, conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(ps - bin) < epsilon*2])
            p_ps1       = numpy.array([pb for ps, pb, _r, _std, _conf, n, _fw_mean, _fw_std, _fw_conf in data if abs(ps - bin) < epsilon*2])

            assert(len(r_pb1) == len(conf_pb1) == len(p_pb1))
            assert(len(r_ps1) == len(conf_ps1) == len(p_ps1))

            if len(r_pb1) > 0:
                if not options['noconfidence']:
                    patch_collection = PatchCollection([conf2poly(p_pb1, list(r_pb1+conf_pb1), list(r_pb1-conf_pb1), color=colors[i])], match_original=True)
                    patch_collection.set_alpha(0.3)
                    patch_collection.set_linestyle('dashed')
                    fig_site.ax.add_collection(patch_collection)
                    fig_site.ax.plot(p_pb1, r_pb1, color=colors[i], label='$%.2f$' % bin)

                    if fig_vs_pb:
                        fig_vs_pb.ax.plot(fw_pb1, r_pb1, color=colors[i], label='$%.2f$' % bin)

                        patch_collection = PatchCollection([conf2poly(p_pb1, list(fw_pb1+fw_conf_pb1), list(fw_pb1-fw_conf_pb1), color=colors[i])], match_original=True)
                        patch_collection.set_alpha(0.3)
                        patch_collection.set_linestyle('dashed')
                        fig_fw_pb.ax.add_collection(patch_collection)
                        fig_fw_pb.ax.plot(p_pb1, fw_pb1, color=colors[i], label='$%.2f$' % bin)
                else:
                    fig_site.ax.plot(p_pb1, r_pb1, color=colors[i], label='$%.2f$' % bin)
                    if fig_vs_pb:
                        fig_vs_pb.ax.plot(fw_pb1, r_pb1, color=colors[i], label='$%.2f$' % bin)
                        fig_fw_pb.ax.plot(p_pb1, fw_pb1, color=colors[i], label='$%.2f$' % bin)
            if len(r_ps1) > 0:
                if not options['noconfidence']:
                    patch_collection = PatchCollection([conf2poly(p_ps1, list(r_ps1+conf_ps1), list(r_ps1-conf_ps1), color=colors[i])], match_original=True)
                    patch_collection.set_alpha(0.3)
                    patch_collection.set_linestyle('dashed')
                    fig_bond.ax.add_collection(patch_collection)
                    fig_bond.ax.plot(p_ps1, r_ps1, color=colors[i], label='$%.2f$' % bin)

                    if fig_vs_ps:
                        fig_vs_ps.ax.plot(fw_ps1, r_ps1, color=colors[i], label='$%.2f$' % bin)

                        patch_collection = PatchCollection([conf2poly(p_ps1, list(fw_ps1+fw_conf_ps1), list(fw_ps1-fw_conf_ps1), color=colors[i])], match_original=True)
                        patch_collection.set_alpha(0.3)
                        patch_collection.set_linestyle('dashed')
                        fig_fw_ps.ax.add_collection(patch_collection)
                        fig_fw_ps.ax.plot(p_ps1, fw_ps1, color=colors[i], label='$%.2f$' % bin)
                else:
                    fig_bond.ax.plot(p_ps1, r_ps1, color=colors[i], label='$%.2f$' % bin)
                    if fig_vs_ps:
                        fig_vs_ps.ax.plot(fw_ps1, r_ps1, color=colors[i], label='$%.2f$' % bin)
                        fig_fw_ps.ax.plot(p_ps1, fw_ps1, color=colors[i], label='$%.2f$' % bin)

            X = [p for p, pb, r in tuples if r < bin and r >= bin-0.1]
            Y = [pb for ps, pb, r in tuples if r < bin and r >= bin-0.1]
            assert(len(X) == len(Y))
            if len(X) >= 4 and len(options['fit']):
                (fitted_params, success), fitfunc = fit_data(X, Y, options)
                #print fitted_params, success
                min_x = min(X)
                while fitfunc(fitted_params, [min_x]) <= 1.0 and min_x >= 0.0:
                    min_x -= 0.01
                max_x = max(X)
                while fitfunc(fitted_params, [max_x]) >= 0.0 and max_x <= 1.0:
                    max_x += 0.01
                xfit = pylab.linspace(min_x, max_x, 100)
                fig_contour.ax.plot(xfit, fitfunc(fitted_params, xfit), color='black', lw=2, label='%.2f' % bin)

        for f in [fig_contour, fig_pcolor, fig_site, fig_bond]:
            f.ax.set_xlim(0, 1)
            f.ax.set_ylim(0, 1)
        fig_site.legend_title = '$p_b$'
        fig_bond.legend_title = '$p_s$'
        fig_contour.save('contour%s' % suffix)
        fig_pcolor.save('pcolor%s' % suffix)
        fig_site.save('graph-site%s' % suffix)
        fig_bond.save('graph-bond%s' % suffix)
        if fig_vs_ps:
            fig_vs_ps.ax.set_xlim(0,1)
            fig_vs_ps.ax.set_ylim(0,1)
            fig_vs_ps.legend_title = '$p_s$'
            fig_vs_ps.save('vs_ps')

            fig_fw_ps.ax.set_xlim(0,1)
            fig_fw_ps.ax.set_ylim(0,1)
            fig_fw_ps.legend_title = '$p_s$'
            fig_fw_ps.save('fw_ps')

            fig_vs_pb.ax.set_xlim(0,1)
            fig_vs_pb.ax.set_ylim(0,1)
            fig_vs_pb.legend_title = '$p_b$'
            fig_vs_pb.save('vs_pb')

            fig_fw_pb.ax.set_xlim(0,1)
            fig_fw_pb.ax.set_ylim(0,1)
            fig_fw_pb.legend_title = '$p_b$'
            fig_fw_pb.save('fw_pb')

def main():
    parser = argparse.ArgumentParser(description='Plot data from simulations')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--limit', default=1.0, type=float, help='reachability limit')
    parser.add_argument('--datapath', nargs='+', default=['./'], help='path to the data')
    parser.add_argument('--outdir', default='', help='path for the output')
    parser.add_argument('--mode', default='site', help='[site, bond_site, vs, simu_testbed]')
    parser.add_argument('--invert', '-i', nargs='?', const=True, default=False, help='1-pb')
    parser.add_argument('--noconfidence', nargs='?', const=True, default=False, help='Do not draw confidence intervals')
    parser.add_argument('--fit', '-f', default='', help='Fit function')
    parser.add_argument('--inset_loc', default=-1, type=int, help='Inset location')
    parser.add_argument('--nodes', default=25, type=int, help='Nodes')
    parser.add_argument('--inset_zoom', default=2, type=float, help='Inset zoom')
    parser.add_argument('--inset_xlim', nargs=2, default=[0, 1], type=float, help='Inset xlim')
    parser.add_argument('--inset_ylim', nargs=2, default=[0, 1], type=float, help='Inset ylim')
    parser.add_argument('--no_legend', nargs='?', default=False, const=True, help='Disable legend')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    options = get_options()
    options['fontsize'] = args.fontsize
    options['datapath'] = args.datapath
    options['limit'] = args.limit
    options['mode'] = args.mode
    options['fit'] = args.fit
    options['nodes'] = args.nodes
    options['no_legend'] = args.no_legend
    options['inset_loc'] = args.inset_loc
    options['inset_zoom'] = args.inset_zoom
    options['inset_xlim'] = args.inset_xlim
    options['inset_ylim'] = args.inset_ylim
    options['grayscale'] = args.grayscale

    if args.outdir != '':
        options['outdir'] = args.outdir
    else:
        options['outdir'] = args.datapath[0]
    options['invert'] = args.invert
    options['prefix'] = ''
    options['noconfidence'] = args.noconfidence

    if options['mode'] == 'site':
        plot_site(options)
    elif options['mode'] == 'bond_site':
        plot_bond_site(options)
    elif options['mode'] == 'vs':
        plot_event_vs_set(options)
    elif options['mode'] == 'simu_testbed':
        plot_simu_vs_testbed(options)
    else:
        print 'unsupported mode'

if __name__ == "__main__":
    main()


