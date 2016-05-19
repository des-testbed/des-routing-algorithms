#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt
import matplotlib.cm as cm

import numpy, math, random
import pylab, scipy
from scipy import stats
from helpers import *

def _plot_distribution(fractionOfNodesList, options):
    logging.debug('')
    steps = 0.05
    probabilities = numpy.arange(0, 1.0+steps, steps)

    fig_cdf_data = MyFig(options, xlabel='Fraction of Nodes', ylabel='CDF', grid=True, legend=True)
    fig_cdf_data.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

    ########################################
    fig_qq_normal = MyFig(options, xlabel='Fraction of Nodes', ylabel='Normal Distribution', grid=True, legend=True)
    fig_qq_normal.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
    fig_qq_normal.ax.set_xlim(0.0, 1.0)
    fig_qq_normal.ax.set_ylim(0.0, 1.0)

    fig_cdf_normal = MyFig(options, xlabel='x', ylabel='CDF', grid=True, legend=True)
    fig_cdf_normal.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

    ########################################
    fig_qq_gamma = MyFig(options, xlabel='Fraction of Nodes', ylabel='Gamma Distribution', grid=True, legend=True)
    fig_qq_gamma.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
    fig_qq_gamma.ax.set_xlim(0.0, 1.0)
    fig_qq_gamma.ax.set_ylim(0.0, 1.0)

    fig_cdf_gamma = MyFig(options, xlabel='x', ylabel='CDF', grid=True, legend=True, aspect='auto')
    fig_cdf_gamma.ax.set_xlim(0.0, 1.0)

    ########################################
    fig_qq_2normal = MyFig(options, xlabel='Fraction of Nodes', ylabel='Normal Distribution', grid=True, legend=True)
    fig_qq_2normal.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
    fig_qq_2normal.ax.set_xlim(0.0, 1.0)
    fig_qq_2normal.ax.set_ylim(0.0, 1.0)

    fig_cdf_2normal = MyFig(options, xlabel='x', ylabel='CDF', grid=True, legend=True)
    fig_cdf_2normal.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

    ########################################
    fig_qq_2chi2 = MyFig(options, xlabel='Fraction of Nodes', ylabel='2x Chi Square Distribution', grid=True, legend=True)
    fig_qq_2chi2.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
    fig_qq_2chi2.ax.set_xlim(0.0, 1.0)
    fig_qq_2chi2.ax.set_ylim(0.0, 1.0)

    fig_cdf_2chi2 = MyFig(options, xlabel='x', ylabel='CDF', grid=True, legend=True)
    fig_cdf_2chi2.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

    colors = cm.hsv(pylab.linspace(0, 0.8, len(fractionOfNodesList)))
    for j, (p, data, label) in enumerate(fractionOfNodesList):
        avr = scipy.average(data)
        sigma = scipy.std(data)
        quantiles_data = stats.mstats.mquantiles(data, prob=probabilities)
        fig_cdf_data.ax.plot(quantiles_data, probabilities, 'o', color=colors[j], linestyle='-', label=label)
        ###########################################################################
        # Normal Distribution
        ###########################################################################
        quantiles_normal = stats.norm.ppf(probabilities, loc=avr, scale=sigma)
        fig_cdf_normal.ax.plot(quantiles_normal, probabilities, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)
        fig_qq_normal.ax.plot(quantiles_data, quantiles_normal, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)
        ###########################################################################
        # Gamma Distribution
        ###########################################################################
        quantiles_gamma = stats.gamma.ppf(probabilities, 0.4, loc=avr, scale=sigma)
        _quantiles_gamma = []
        for x in quantiles_gamma:
            if x != numpy.infty:
                _quantiles_gamma.append(min(1.0,x))
            else:
                _quantiles_gamma.append(x)
        quantiles_gamma = numpy.array(_quantiles_gamma)
        fig_cdf_gamma.ax.plot(quantiles_gamma, probabilities, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)
        fig_qq_gamma.ax.plot(quantiles_data, quantiles_gamma, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)
        ###########################################################################
        # 2x Chi Square Distribution
        ###########################################################################
        data_2chi2 = []
        for i in range(0, 5000):
            sigma_2chi2 = p*0.5
            dv = 0.5
            if random.normalvariate(0.6, 0.1) < p:
            #if random.uniform(0.0, 1.0) < p:
                exp_2chi2 = 1.0
                d = stats.chi2.rvs(dv, loc=exp_2chi2, scale=sigma_2chi2, size=1)
                d = exp_2chi2-abs(exp_2chi2-d)
            else:
                exp_2chi2 = 0.05
                d = stats.chi2.rvs(dv, loc=exp_2chi2, scale=sigma_2chi2, size=1)
            data_2chi2.append(max(min(d[0], 1.0), 0.0))
        quantiles_data_2chi2 = stats.mstats.mquantiles(data_2chi2, prob=probabilities)
        fig_cdf_2chi2.ax.plot(quantiles_data_2chi2, probabilities, 'o', linestyle='-', label='p=%.2f' % p, color=colors[j])
        fig_qq_2chi2.ax.plot(quantiles_data, quantiles_data_2chi2, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)

        ###########################################################################
        # 2x Normal Distribution
        ###########################################################################
        data_2normal = []
        for i in range(0, 2000):
            if random.uniform(0.0, 1.0) < p:
                exp_2normal = 1.0
                sigma_2normal = 0.05
                d = stats.norm.rvs(loc=exp_2normal, scale=sigma_2normal, size=1)
                d = exp_2normal-abs(exp_2normal-d)
            else:
                exp_2normal = 0.05
                sigma_2normal = 0.05
                d = stats.norm.rvs(loc=exp_2normal, scale=sigma_2normal, size=1)
            data_2normal.append(max(min(d[0], 1.0), 0.0))
        quantiles_data_2normal = stats.mstats.mquantiles(data_2normal, prob=probabilities)
        fig_cdf_2normal.ax.plot(quantiles_data_2normal, probabilities, 'o', linestyle='-', label='p=%.2f' % p, color=colors[j])
        fig_qq_2normal.ax.plot(quantiles_data, quantiles_data_2normal, 'o', color=colors[j], linestyle='-', label='p=%.2f' % p)

    fig_qq_normal.save('distribution-qq_normal')
    fig_qq_2normal.save('distribution-qq_2normal')
    fig_qq_gamma.save('distribution-qq_gamma')
    fig_qq_2chi2.save('distribution-qq_2chi2')

    fig_cdf_gamma.save('distribution-cdf_gamma')
    fig_cdf_2normal.save('distribution-cdf_2normal')
    fig_cdf_2chi2.save('distribution-cdf_2chi2')
    fig_cdf_data.save('distribution-cdf_data')
    fig_cdf_normal.save('distribution-cdf_normal')

