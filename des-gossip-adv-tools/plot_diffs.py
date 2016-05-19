#!/usr/bin/env python

import matplotlib
import argparse
import pylab
import scipy
import os, sys
import numpy
from helpers import *
import re

def fix_labels(name, options):
    re_grid = re.compile(r'grid(\d+)x(\d+)')
    re_rr = re.compile(r'rr-(\d+)n-(\d+)d-(\d+)g')
    re_geo = re.compile(r'geo-(\d+)n-250m-30g')
    re_testbed = re.compile(r'testbed-(\d+)-uu')

    if options['suffix'] == 'grid':
        match = re_grid.match(name)
        if match:
            return r'$%d \times %d$' % (int(match.group(1)), int(match.group(2)))
    elif options['suffix'] == 'geo':
        match = re_geo.match(name)
        if match:
            return r'$%d$' % (int(match.group(1)),)
    elif options['suffix'] == 'rr':
        match = re_rr.match(name)
        if match:
            return r'%d' % (int(match.group(2)))
    elif options['suffix'] == 'testbed':
        match = re_testbed.match(name)
        if match:
            return r'DES-Testbed'

    return name

def plot_diffs(data, options):
    fig_eval = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_b$', ylabel=r'$\Delta p_s$', aspect='auto', legend=True, grid=False)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1, len(data)))
    else:
        colors = options['color'](pylab.linspace(0, 1, len(data)))
    ls = options['linestyles']
    for i, (name, X, Y) in enumerate(data):
        name = fix_labels(name, options)
        fig_eval.ax.plot(Y, X, label=name, color=colors[i], ls=ls[i])
    #fig_eval.ax.set_ylim(0,1)
    suffix = ''
    if options['suffix'] != '':
        suffix = '-%s' % options['suffix']

    if options['suffix'] == 'grid':
        fig_eval.legend_title = 'Nodes'
    elif options['suffix'] == 'rr':
        fig_eval.legend_title = 'Node degree'
    elif options['suffix'] == 'geo':
        fig_eval.legend_title = 'Nodes'
    fig_eval.save('diffs%s' % suffix)

    #fig_eval_surface = MyFig(options, figsize=(10, 10), xlabel=r'Probability $p_b$', ylabel=fig_eval.legend_title, zlabel=r'$\Delta p_s$', aspect='auto', legend=True, grid=False, ThreeD=True)
    #minx = min([min(X) for (name, X, Y) in data])
    #maxx = max([max(X) for (name, X, Y) in data])
    #factor = 1000
    #l = abs(int(maxx*factor) - int(minx*factor))
    #X, Y = pylab.meshgrid(range(0, l), range(0, l))
    #Z = numpy.zeros(l)
    #for i, (name, x, y) in enumerate(data):
        #z = numpy.zeros(l)
        #_minx = int(min(x)*factor)
        #print z.shape, numpy.array(y).shape
        #print abs(minx-_minx), abs(minx-_minx)+len(y), len(y)
        #z[abs(minx-_minx):abs(minx-_minx)+len(y)] = numpy.array(y)

    #fig_eval_surface.plot_surface(X, Y, Z, cmap=options['color'], rstride=1, cstride=1, linewidth=0, antialiased=True, shade=True, vmin=-r, vmax=r)
    #fig_eval_surface.save('diffs%s-surface' % suffix)

def parse_diffs(options):
    data = list()
    f = open(options['diffs'], 'r')
    for line in f:
        name, X, Y = [t.strip() for t in line.split(';')]
        X = [float(x) for x in X.split(',')]
        Y = [float(y) for y in Y.split(',')]
        data.append((name, X, Y))
    return data

def main():
    parser = argparse.ArgumentParser(description='Plot data from simulations')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--outdir', default='', help='path for the output')
    parser.add_argument('--diffs', required=True, help='file to store all diffs')
    parser.add_argument('--suffix', default='', help='suffix of file name')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    options = get_options()
    options['fontsize'] = args.fontsize
    options['diffs'] = args.diffs
    if args.outdir != '':
        options['outdir'] = args.outdir
    else:
        options['outdir'] = args.outdir
    options['prefix'] = 'gossip-versus-percolation'
    options['suffix'] = args.suffix
    options['grayscale'] = args.grayscale

    data = parse_diffs(options)
    plot_diffs(data, options)

if __name__ == "__main__":
    main()
