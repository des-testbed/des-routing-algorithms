#!/usr/bin/env python

import matplotlib
import argparse
import pylab
import scipy
import os
import numpy
from helpers import *
from matplotlib.collections import PatchCollection
from plot_simu import calc_metrics
import re
from collections import Counter

epsilon = numpy.finfo(numpy.float).eps

def parse(data_file):
    merge = dict()
    for line in data_file:
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
    return calc_metrics(merge)

def k_subsets_i(n, k):
    '''
    Yield each subset of size k from the set of intergers 0 .. n - 1
    n -- an integer > 0
    k -- an integer > 0

    Source: http://code.activestate.com/recipes/500268/
    '''
    # Validate args
    if n < 0:
        raise ValueError('n must be > 0, got n=%d' % n)
    if k < 0:
        raise ValueError('k must be > 0, got k=%d' % k)
    # check base cases
    if k == 0 or n < k:
        yield set()
    elif n == k:
        yield set(range(n))

    else:
        # Use recursive formula based on binomial coeffecients:
        # choose(n, k) = choose(n - 1, k - 1) + choose(n - 1, k)
        for s in k_subsets_i(n - 1, k - 1):
            s.add(n - 1)
            yield s
        for s in k_subsets_i(n - 1, k):
            yield s

def k_subsets(s, k):
    '''
    Yield all subsets of size k from set (or list) s
    s -- a set or list (any iterable will suffice)
    k -- an integer > 0

    Source: http://code.activestate.com/recipes/500268/
    '''
    s = list(s)
    n = len(s)
    for k_set in k_subsets_i(n, k):
        yield ([s[i] for i in k_set])

def eval_critical(options, tuples_for_pb, ps, cepsilon=0.01):
    fig_abs = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'abs', aspect='auto', legend=True, grid=False)
    fig_mean = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'mean', aspect='auto', legend=True, grid=False)
    fig_majority = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Majority', aspect='auto', legend=True, grid=False)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, len(tuples_for_pb)))
    else:
        colors = options['color'](pylab.linspace(0, 1.0, len(tuples_for_pb)))
    crit_ranges = options['crit_range']
    p_c = list()

    for i, pb in enumerate(sorted(tuples_for_pb.keys())):
        if '%.2f' % pb not in crit_ranges:
            continue
        minps = crit_ranges['%.2f' % pb][0]
        maxps = crit_ranges['%.2f' % pb][1]

        rs = tuples_for_pb[pb]
        tuples = zip(*rs)
        if len(tuples) == 0:
            continue
        crit_abs = [max(t)-min(t) for t in tuples]
        crit_abs_min = min(crit_abs)
        crit_abs_ps = [pos for pos, c in enumerate(crit_abs) if abs(c - crit_abs_min) < cepsilon]
        fig_abs.ax.plot(ps, crit_abs, label='%.2f' % pb, color=colors[i])

        crit_mean = [scipy.mean(t) for t in tuples]
        crit_mean_min = min(crit_mean)
        crit_mean_ps = [pos for pos, c in enumerate(crit_mean) if abs(c - crit_mean_min) < cepsilon]
        fig_mean.ax.plot(ps, crit_mean, label='%.2f' % pb, color=colors[i])

        majority = list()
        for t in tuples:
            all_pairs = list(k_subsets(t, 2))
            c = [abs(a-b) for a,b in all_pairs]
            counter = Counter(c)
            times = set(counter.values())
            y = scipy.mean([val for (val, occ) in counter.iteritems() if occ == max(times)])
            #y = scipy.mean(c)
            majority.append(y)
        fig_majority.ax.plot(ps, majority, label='%.2f' % pb, color=colors[i])
        start = list(ps).index(minps)
        stop = min(list(ps).index(maxps)+1, len(ps))
        metric = majority
        pc = ps[list(metric).index(min(metric[start:stop]))]
        p_c.append((pb, pc))

    fig_abs.legend_title = '$p_b$'
    fig_abs.save('value-eval-abs')
    fig_mean.legend_title = '$p_b$'
    fig_mean.save('value-eval-pairs')
    fig_majority.legend_title = '$p_b$'
    fig_majority.save('value-eval-pairs')
    return p_c

def plot(options):
    def legendTitle(s):
        if s.find('grid') >= 0:
            return 'Nodes'
        elif s.find('rr') >= 0:
            return 'Nodes'
        elif s.find('geo') >= 0:
            return 'Nodes'
        else:
            return s

    def cleanLabel(s):
        s = s.replace('gossip-', '')
        s = s.replace('percolation-', '')
        for t in ['grid', 'geo', 'rr']:
            s = s.replace(t, '')
        for t in ['-100r', '-30r-30s', '-30g', '-4d', '-250m']:
            s = s.replace(t, '')
        s = s.replace('x', r' \times ')
        if s.endswith('n'):
            s = s[1:-1]
        return '$%s$' % s

    bins = 11
    # ugly code
    path = '/'.join(options['datapath'].split('/')[0:-1])
    pattern = options['datapath'].split('/')[-1]
    ntype = legendTitle(pattern)

    dirList = [p for p in os.listdir(path) if p.find(pattern) >= 0]
    convert = lambda text: int(text) if text.isdigit() else text
    alphanum_key = lambda key: [convert(i) for i in re.split('([0-9]+)', key)]
    dirList = sorted(dirList, key=alphanum_key)

    if pattern.find('gossip') >= 0:
        data_files = [(cleanLabel(p), open('%s/%s/results-mean' % (path, p))) for p in dirList]
        ylabel = r'Reachability~$\reachability$'
    elif pattern.find('percolation') >= 0:
        data_files = [(cleanLabel(p), open('%s/%s/results-max' % (path, p))) for p in dirList]
        ylabel = r'Fractional Size of Largest Cluster~${\reachability}_{c}$'
    else:
        assert(False)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, len(data_files)))
    else:
        colors = options['color'](pylab.linspace(0, 1.0, len(data_files)))

    tuples_for_pb = dict()
    figs = dict()
    for bin in pylab.linspace(0, 1, bins):
        figs[bin] = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=ylabel, aspect='auto', legend=True, grid=False)
    data_for_pb = dict()

    for j, (p, d) in enumerate(data_files):
        print p
        data = parse(d)
        nodes = 105.0
        for i, bin in enumerate(pylab.linspace(0, 1, bins)):
            fig = figs[bin]
            r_pb1       = numpy.array([r for _ps, pb, r, _std, _conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])
            #fw_pb1      = numpy.array([fw_mean for _ps, pb, _r, _std, _conf, _n, fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])/nodes
            #fw_conf_pb1 = numpy.array([fw_conf for _ps, pb, _r, _std, _conf, _n, fw_mean, _fw_std, fw_conf in data if abs(pb - bin) < epsilon*2])/nodes
            conf_pb1    = numpy.array([r_conf for _ps, pb, _r, _std, r_conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])
            p_pb1       = numpy.array([ps for ps, pb, _r, _std, _conf, _n, _fw_mean, _fw_std, _fw_conf in data if abs(pb - bin) < epsilon*2])

            if len(r_pb1) == 0:
                continue
            #fig.ax.errorbar(p_pb1, r_pb1, yerr=conf_pb1, color=colors[j], label=p)
            fig.ax.plot(p_pb1, r_pb1, color=colors[j], label=p)
            try:
                tuples_for_pb[bin].append(r_pb1)
            except KeyError:
                tuples_for_pb[bin] = [r_pb1]

    p_c = eval_critical(options, tuples_for_pb, p_pb1)

    for pb, fig in figs.iteritems():
        pc = [b for (a, b) in p_c if abs(a-pb) < 0.0001]
        if len(pc):
            pc = pc[0]
            if not options['no_fit']:
                fig.ax.plot([pc, pc], [0, 1], color='black', linestyle='dashed')
                if pc+0.025 < 0.8:
                    fig.ax.text(pc+0.025, 0.05, '$p_{s_c} = %.2f$' % pc)
                else:
                    fig.ax.text(pc-0.2, 0.9, '$p_{s_c} = %.2f$' % pc)
        fig.legend_title = ntype
        fig.save('value-pb_%.2f' % pb, grid='y')

def main():
    parser = argparse.ArgumentParser(description='Plot data from simulations')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--datapath', required=True, help='path pattern for the data')
    parser.add_argument('--outdir', default='', required=True, help='path for the output')
    parser.add_argument('--crit_range', nargs=3, default=[], type=float, help='range to search for critical probability', action='append')
    parser.add_argument('--no_fit', default=False, nargs='?', const=True, help='omit fitting')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    crit_ranges = dict()
    for pb, minps, maxps in args.crit_range:
        crit_ranges['%.2f' % pb] = (minps, maxps)
    options = get_options()
    options['fontsize'] = args.fontsize
    options['datapath'] = args.datapath
    options['outdir'] = args.outdir
    options['crit_range'] = crit_ranges
    options['no_fit'] = args.no_fit
    options['prefix'] = 'critical'
    options['grayscale'] = args.grayscale

    plot(options)

if __name__ == '__main__':
    main()