#!/usr/bin/env python

import matplotlib
import argparse
import pylab
import scipy
import os, sys
import numpy
from helpers import *
from matplotlib.collections import PatchCollection
import fileinput

def calc_metrics(merge):
    data = list()
    for (ps, pb) in merge.keys():
        results = merge[(ps, pb)]
        reachability = [r for _ps, _pb, r, std, _conf, _n in results]
        sample_size = list(set([n for _ps, _pb, _r, _std, _conf, n in results]))
        assert(len(sample_size) == 1)
        sample_size = sample_size[0]
        reachability_mean = scipy.mean(reachability)
        if len(reachability) > 1:
            reachability_std  = scipy.std(reachability)
        else:
            reachability_std   = std
        m = sum([n for _ps, _pb, _r, _std, _conf, n in results])
        reachability_conf = 1.96*reachability_std/scipy.sqrt(m)
        data.append((ps, pb, reachability_mean, reachability_std, reachability_conf, sample_size))
    return sorted(data)

def fit_data(X, Y, options):
    def isConcave(X, Y):
        start = (X[0], Y[0])
        end   = (X[-1], Y[-1])
        inclination = (end[1]-start[1])/float(end[0]-start[0])
        interception = end[1] - inclination * end[0]
        linear = lambda a,b,x: a*x+b
        deviation = [y - linear(inclination, interception, x) for x, y in zip(X,Y)]
        #print deviation
        print inclination, interception, start, end
        deviation = sum(deviation)
        if deviation <= 0:
            print 'convex', deviation
            return False
        else:
            print 'concave', deviation
            return True

    init_params_convex = [1.0, 1.0, 1.0, 1.0]
    init_params_concave = [1.0, -3.0, 2.0, 1.0]

    if isConcave(X, Y):
        init_params = init_params_concave
    else:
        init_params = init_params_convex
    hyperbolic2 = lambda (a,b,c,d), x: 1/(map(lambda _x: a*_x, x)+b)    # f(x) = 1/(ax+b)
    hyperbolic4 = lambda (a,b,c,d), x: d/(map(lambda _x: a*_x, x)+b)+c  # f(x) = d/(ax+b)+c
    #exponential = lambda (a,b,c,d), x: scipy.e**(a*(x+b))+c             # f(x) = e^(-a(x+b))+c
    fitfunc = locals()['hyperbolic4']
    errfunc = lambda (a,b,c,d), x, y: y-fitfunc((a,b,c,d), x)
    return scipy.optimize.leastsq(errfunc, init_params, args=(X, Y), maxfev=10**6), fitfunc

def parseData(path, limits):
    dirList = os.listdir(path)
    files = [f for f in dirList if f.startswith('results')]
    data_files = [(open('%s/%s' % (path, f), 'r'), f.replace('results', '')) for f in files]
    results = dict()
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
            line = line.split(':')[1].split(',')
            # ps, pb, reachability, std, conf, replications
            t = (float(line[0]), float(line[1]), float(line[2]), float(line[3]), float(line[4]), int(line[5]))
            try:
                merge[(float(line[0]), float(line[1]))].append(t)
            except KeyError:
                merge[(float(line[0]), float(line[1]))] = [t]

        if bond_site:
            if not max_cluster:
                return None, None
            dtype = 'percolation'
        else:
            dtype = 'gossip'

        merged = calc_metrics(merge)
        for l in limits:
            data = [(ps, pb) for ps, pb, r, _std, _conf, _n in merged if r >= l]
            contour = list()
            pss = sorted(list(set([ps for ps, pb in data])))
            for foo_ps in pss:
                foo_pb = min([pb for ps, pb in data if ps == foo_ps])
                contour.append((foo_ps, foo_pb))

            pbs = sorted(list(set([pb for ps, pb in data])))
            for foo_pb in pbs:
                foo_ps = min([ps for ps, pb in data if pb == foo_pb])
                contour.append((foo_ps, foo_pb))
            contour = sorted(list(set(contour)))
            results[l] = contour
        return results, dtype

def parseData_matrix(path):
    dirList = os.listdir(path)
    files = [f for f in dirList if f.startswith('results')]
    data_files = [(open('%s/%s' % (path, f), 'r'), f.replace('results', '')) for f in files]
    results = dict()
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
            line = line.split(':')[1].split(',')
            # ps, pb, reachability, std, conf, replications
            t = (float(line[0]), float(line[1]), float(line[2]), float(line[3]), float(line[4]), int(line[5]))
            try:
                merge[(float(line[0]), float(line[1]))].append(t)
            except KeyError:
                merge[(float(line[0]), float(line[1]))] = [t]

        if bond_site:
            if not max_cluster:
                return None, None
            dtype = 'percolation'
        else:
            dtype = 'gossip'

        merged = calc_metrics(merge)
        ps_len = len(set([ps for ps, _pb, _r, _std, _conf, _n in merged]))
        pb_len = len(set([pb for _ps, pb, _r, _std, _conf, _n in merged]))
        matrix = numpy.zeros((ps_len, pb_len))
        for ps, pb, r, _std, _conf, _n in merged:
            #print ps, ps*ps_len, pb, pb*pb_len, matrix.size
            matrix[int(ps*ps_len+0.0001)-1][int(pb*pb_len+0.0001)-1] = r
        return matrix, dtype

def plot_vs_pcolor(options):
    gossip = None
    percolation = None
    for dpath in options['datapath']:
        print dpath
        data, dtype = parseData_matrix(dpath)
        if data == None or dtype == None:
            continue
        if dtype == 'gossip':
            gossip = data
        elif dtype == 'percolation':
            percolation = data
        else:
            assert(False)
    diff = gossip - percolation

    fig = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Probability $p_b$', aspect='auto', legend=False, grid=False)
    fig_surface = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Probability $p_b$', aspect='auto', legend=False, grid=False, ThreeD=True)
    r = max(abs(diff.min()), abs(diff.max()))
    if options['grayscale']:
        pcolor = fig.ax.pcolorfast(diff.transpose(), cmap=options['graycm'], vmin=-r, vmax=r)
    else:
        pcolor = fig.ax.pcolorfast(diff.transpose(), cmap=options['color'], vmin=-r, vmax=r)
    cbar = fig.colorbar(pcolor)
    X, Y = pylab.meshgrid(range(0, diff.shape[1]), range(0, diff.shape[0]))
    Z = numpy.array(diff.transpose())
    Z = blur_image(Z, 3)
    if options['grayscale']:
        surf = fig_surface.ax.plot_surface(X, Y, Z, cmap=options['graycm'], rstride=1, cstride=1, linewidth=0, antialiased=True, shade=True, vmin=-r, vmax=r)
    else:
        surf = fig_surface.ax.plot_surface(X, Y, Z, cmap=options['color'], rstride=1, cstride=1, linewidth=0, antialiased=True, shade=True, vmin=-r, vmax=r)
    cbar = fig_surface.colorbar(surf)
    fig_surface.ax.plot_wireframe(X, Y, Z+0.002, color='black', rstride=5, cstride=5, linewidth=0.75, linestyles='solid', antialiased=True)

    for f in [fig, ]:
        f.ax.set_xticks(pylab.linspace(0, 100, 6))
        f.ax.set_xticklabels(['$%.1f$' % l for l in pylab.linspace(0, 1, 6)])
        f.ax.set_yticks(pylab.linspace(0, 100, 6))
        f.ax.set_yticklabels(['$%.1f$' % l for l in pylab.linspace(0, 1, 6)])
    for f in [fig_surface, ]:
        for wax in [fig_surface.ax.w_xaxis, fig_surface.ax.w_yaxis]:
            #wax.set_ticks(pylab.linspace(0, 100, 6))
            wax.set_ticklabels(pylab.linspace(0, 1, 6))

    fig.save(options['dname'])
    fig_surface.ax.view_init(45)
    fig_surface.save(options['dname']+'_surface')

def plot_vs(options):
    def eval_min_max(fitfunc, fitted_params, X):
        min_x = min(X)
        while fitfunc(fitted_params, [min_x]) <= 1.0 and min_x >= 0.0:
            min_x -= 0.01
        max_x = max(X)
        while fitfunc(fitted_params, [max_x]) >= 0.0 and max_x <= 1.0:
            max_x += 0.01
        return pylab.linspace(min_x, max_x, 100)

    def write_diffs(options, X, Y):
        if options['diffs'] != '':
            replaceExp = '%s; %s; %s\n' % (options['dname'], ','.join([str(f) for f in X]), ','.join([str(f) for f in Y]))
            f = open(options['diffs'], 'a')
            f.write(replaceExp)
            f.close()

    gossip = None
    percolation = None
    for dpath in options['datapath']:
        print dpath
        data, dtype = parseData(dpath, options['contours']) # numpy.arange(0.8, 1.01, 0.05))
        if data == None or dtype == None:
            continue
        if dtype == 'gossip':
            gossip = data
        elif dtype == 'percolation':
            percolation = data
        else:
            assert(False)

    fig_contour = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_s$', ylabel=r'Probability $p_b$', aspect='auto', legend=True, grid=False)
    fig_eval = MyFig(options, figsize=(10, 8), xlabel=r'Probability $p_b$', ylabel=r'$\Delta p_s$', aspect='auto', legend=True, grid=False)
    if options['grayscale']:
        colors = options['graycm'](pylab.linspace(0, 1.0, len(gossip)))
    else:
        colors = options['color'](pylab.linspace(0, 1.0, len(gossip)))
    ls = options['linestyles']

    hyperbolic2_inv = lambda (a,b,c,d), Y: map(lambda y: (1-(y*b))/float(a*y), Y)       # f(x) = 1-b/(ay)
    hyperbolic4_inv = lambda (a,b,c,d), Y: map(lambda y: (d-b*(y-c))/float(a*(y-c)), Y) # f(x) = d-b(y-c)/a(y-c)


    for i, l in enumerate(sorted(gossip.keys())):
        Xg, Yg = [x for x, y in gossip[l]], [y for x, y in gossip[l]]
        Xp, Yp = [x for x, y in percolation[l]], [y for x, y in percolation[l]]
        if min([len(Xg), len(Xp)]) < 4:
            continue

        fig_contour.ax.scatter(Xg, Yg, color=colors[i], marker='x')
        (fitted_paramsg, successg), fitfuncg = fit_data(Xg, Yg, options)
        print fitted_paramsg, successg
        xfitg = eval_min_max(fitfuncg, fitted_paramsg, Xg)
        fig_contour.ax.plot(xfitg, fitfuncg(fitted_paramsg, xfitg), color=colors[i], label='%.2f' % l)

        ps_gossip = hyperbolic4_inv(fitted_paramsg, pylab.linspace(1,0,1000))
        ps_gossip = [r for r in ps_gossip if r <=1 and r >=0]

        fig_contour.ax.scatter(Xp, Yp, color=colors[i], marker='o')
        (fitted_paramsp, successp), fitfuncp = fit_data(Xp, Yp, options)
        print fitted_paramsp, successp
        xfitp = eval_min_max(fitfuncp, fitted_paramsp, Xp)
        fig_contour.ax.plot(xfitp, fitfuncp(fitted_paramsp, xfitp), color=colors[i])

        ps_percolation = hyperbolic4_inv(fitted_paramsp, pylab.linspace(1,0,1000))
        ps_percolation = [r for r in ps_percolation if r <=1 and r >=0]
        size = min(len(ps_gossip), len(ps_percolation))
        print l, size, len(ps_gossip), len(ps_percolation)
        ps_percolation = ps_percolation[0:size]
        ps_gossip = ps_gossip[0:size]

        delta_ps = numpy.array(ps_gossip)-numpy.array(ps_percolation)
        #size = min(len(Xp), len(Xg))
        #delta_ps = numpy.array(Xg[0:size]) - numpy.array(Xp[0:size])
        delta_pb = pylab.linspace(1,0,1000)
        fig_eval.ax.plot(delta_pb[0:len(delta_ps)], delta_ps, color=colors[i], label='%.2f' % l, ls=ls[i])

        write_diffs(options, delta_ps, pylab.linspace(1,0,1000)[0:len(delta_ps)])

    fig_contour.ax.axis((0,1, 0,1))
    fig_contour.legend_title = 'Contours'
    fig_contour.save('limits', grid='xy')
    fig_eval.legend_title = 'Contours'
    fig_eval.save('diff')

def main():
    parser = argparse.ArgumentParser(description='Plot data from simulations')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--datapath', nargs=2, required=True, help='path to the data')
    parser.add_argument('--outdir', default='', help='path for the output')
    parser.add_argument('--contours', nargs='+', default=[0.9], type=float, help='contours to evaluate')
    parser.add_argument('--diffs', default='', help='file to store all diffs')
    parser.add_argument('--dname', default='', help='name for diffs')
    parser.add_argument('--grayscale', nargs='?', const=True, default=False, help='Create plots with grayscale colormap')
    args = parser.parse_args()

    options = get_options()
    options['contours'] = args.contours
    options['fontsize'] = args.fontsize
    options['datapath'] = args.datapath
    options['diffs'] = args.diffs
    options['dname'] = args.dname
    if args.outdir != '':
        options['outdir'] = args.outdir
    else:
        options['outdir'] = args.outdir
    options['prefix'] = 'gossip-versus-percolation'
    options['grayscale'] = args.grayscale

    plot_vs_pcolor(options)
    plot_vs(options)

if __name__ == "__main__":
    main()


