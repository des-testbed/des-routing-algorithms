#!/usr/bin/env python

import re
from helpers import *
import pylab
import numpy
import scipy
import argparse
import logging

import matplotlib
from matplotlib.patches import Polygon, Ellipse, Rectangle
from matplotlib.collections import PatchCollection

from concave_hull import get_concave

re_r = re.compile(r'R\(N=(\d+), (.+)\)=(\d+.\d+)(?:, (\d+.\d+), (\d+.\d+))?')
re_fit = re.compile(r'\[(.+)\], R\(F\) = (\d+.\d+)x \+ ([-]?\d+.\d+)')

dirs = [
('mcds', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mcds/0.92/'),
('mcds', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mcds/0.88/'),
('mcds', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mcds/0.59/'),
('mcds', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mcds/0.10/'),
('mpr-event', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mpr/gossip11-event'),
('mpr-periodic', '/home/blywis/svn/cst-phd/blywis/Text/graphics/mpr/gossip11-periodic'),
#'/home/blywis/svn/cst-phd/blywis/Text/graphics/gossip_parallel/gossip0/20110313-channel13-10-90-20-384_Byte-1_s-30_Pkts_results/',
('gossip0', '/home/blywis/svn/cst-phd/blywis/Text/graphics/gossip_parallel/gossip0/20110319-channel13-10-90-20-0_Byte-1_s-30_Pkts_results/'),
#'/home/blywis/svn/cst-phd/blywis/Text/graphics/gossip_parallel/gossip3/20110326-gossip3-channel13-10-90-20-384_Byte-1_s-30_Pkts_results/'
('gossip3', '/home/blywis/svn/cst-phd/blywis/Text/graphics/gossip_parallel/gossip3/20110326-gossip3-channel13-10-90-20-0_Byte-1_s-30_Pkts_results/'),
('gossip14', '/home/blywis/svn/cst-phd/blywis/Text/graphics/gossip14/20111211-gossip14-again.db_results/')]

def parse(options):
    r_data = list()
    fit_data = list()
    for t, d in dirs:
        r_file = open(d+'/reachability.data')
        for line in r_file:
            match = re_r.match(line)
            if match:
                N = int(match.group(1))
                params = match.group(2)
                params =  [s.strip().split('=') for s in params.split(',')]
                if t == 'gossip14':
                    size = [int(v) for p, v in params if p == 'pkg_size'][0]
                    if size != 0:
                        continue
                R = float(match.group(3))
                R_conf = match.group(4)
                F_conf = match.group(5)
                r_data.append((t, N, params, R, R_conf, F_conf))
            else:
                logging.warning('no match: %s', line)

        fit_file = open(d+'/fit.data')
        for line in fit_file:
            match = re_fit.match(line)
            if match:
                params = match.group(1)
                params =  [s.strip().split('=') for s in params.split(',')]
                if t == 'gossip14':
                    size = [int(v) for p, v in params if p == 'pkg_size'][0]
                    if size != 0:
                        continue
                a = float(match.group(2))
                b = float(match.group(3))
                fit_data.append((t, a, b, params))
            else:
                logging.warning('no match: %s', line)
    return r_data, fit_data

def plot(r_data, fit_data, options):
    for sources in [10, 30, 50, 70, 90]:
        def add_legend():
            proxies = []
            for key in sorted(points.keys()):
                r = Rectangle((0, 0), 1, 1, facecolor=name2color(key), edgecolor='gray', alpha=0.5)
                proxies.append((r, key))
            fig.ax.legend([proxy for proxy, label in proxies], [name2label(label) for proxy, label in proxies], loc='lower right', labelspacing=0.1)
            fig.legend = True

        fig = MyFig(options, figsize=(10,8), xlabel=r'Fraction of Forwarded Packets~$\forwarded$', ylabel=r'Reachability~$\reachability$', legend=False, grid=False, aspect='auto', legend_pos='best')
        ellipses = list()

        points = {
            'gossip0': list(),
            'gossip3': list(),
            'gossip14': list(),
            'mcds': list(),
            'mpr-event': list(),
            'mpr-periodic': list()
        }

        def name2label(name):
            mapping = {
                'gossip0': r'\emph{gossip0}',
                'gossip3': r'\emph{gossip3}',
                'gossip14': r'counter-based',
                'mcds': r'$\MCDS$',
                'mpr-event': r'$\MPR$ (event-based)',
                'mpr-periodic': r'$\MPR$ (periodic)'
            }
            try:
                return mapping[name]
            except KeyError:
                return name

        def name2color(name):
            names = ['gossip0', 'gossip3', 'gossip14', 'mcds', 'mpr-event', 'mpr-periodic']
            if options['grayscale']:
                colors = options['graycm'](pylab.linspace(0, 1, len(names)))
            else:
                colors = fu_colormap()(pylab.linspace(0, 1, len(names)))
            try:
                i = names.index(name)
                return colors[i]
            except ValueError:
                return 'black'

        for t, a, b, value in fit_data:
            data = [(_N, _R, _R_conf, _F_conf) for _t, _N, _value, _R, _R_conf, _F_conf in r_data if t == _t and value == _value and _N == sources]
            if len(data) == 0:
                logging.warning('skipping: t=%s, value=%s, N=%d', str(t), str(value), sources)
                continue
            N, R, R_conf, F_conf = data[0]
            x = pylab.linspace(0, 1, 1000)
            if options['plot_fits']:
                fig.ax.plot(x, numpy.polyval([a, b], x), linestyle='solid', color='gray', alpha=0.4)
            plist = points[t]
            color = name2color(t)
            FW = (R-b)/a
            if R_conf != None and F_conf != None:
                ellipse = Ellipse((FW, R), float(F_conf)*2, float(R_conf)*2, edgecolor=color, facecolor=color, alpha=0.5)
                ellipses.append(ellipse)
                plist.append((FW+float(F_conf), R+float(R_conf)))
                plist.append((FW-float(F_conf), R+float(R_conf)))
                plist.append((FW+float(F_conf), R-float(R_conf)))
                plist.append((FW-float(F_conf), R-float(R_conf)))
            if options['plot_points']:
                fig.ax.plot(FW, R, marker='o', color=color, label='%s, $%s$' % (t, value))
        if options['plot_ellipses']:
            patch_collection = PatchCollection(ellipses, match_original=True)
            fig.ax.add_collection(patch_collection)

        for key, value in points.iteritems():
            if len(value) == 0:
                continue
            concave, convex = get_concave(value, 0.1)
            color = name2color(key)

            if options['plot_convex']:
                poly = Polygon(convex, edgecolor='gray', facecolor=color, closed=True, alpha=0.5)
                patch_collection = PatchCollection([poly], match_original=True)
                patch_collection.zorder = -2
                fig.ax.add_collection(patch_collection)

            if options['plot_concave']:
                poly = Polygon(concave, edgecolor='gray', facecolor=color, closed=True, alpha=0.5)
                patch_collection = PatchCollection([poly], match_original=True)
                patch_collection.zorder = -2
                fig.ax.add_collection(patch_collection)
        fig.ax.axis((0, 1, 0, 1))
        add_legend()
        fig.save('%d_sources' % sources)


def main():
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('--outdir', default='./', help='output directory for the plots')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--plot_fits', nargs='?', const=True, default=False, help='plot fitted lines')
    parser.add_argument('--plot_convex', nargs='?', const=True, default=False, help='plot convex hull')
    parser.add_argument('--plot_concave', nargs='?', const=True, default=False, help='plot concave hull')
    parser.add_argument('--plot_ellipses', nargs='?', const=True, default=False, help='plot confidence interval ellipses')
    parser.add_argument('--plot_points', nargs='?', const=True, default=False, help='plot data points')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')

    options = get_options()
    options['outdir'] = args.outdir + '/'
    options['fontsize'] = args.fontsize
    options['prefix'] = 'comparison'
    options['plot_fits'] = args.plot_fits
    options['plot_convex'] = args.plot_convex
    options['plot_concave'] = args.plot_concave
    options['plot_ellipses'] = args.plot_ellipses
    options['plot_points'] = args.plot_points
    options['grayscale'] = args.grayscale
    r_data, fit_data = parse(options)
    plot(r_data, fit_data, options)

if __name__ == '__main__':
    main()