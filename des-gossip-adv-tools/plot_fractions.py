#!/usr/bin/env python
# -*- coding: utf-8 -*-

import logging
import matplotlib.cm as cm
from matplotlib.patches import Polygon, Rectangle, Ellipse
from matplotlib.collections import PatchCollection
import numpy, pylab, scipy, random
from scipy import stats
import scipy.stats.mstats_extras
from helpers import drawBuildingContours, MyFig, confidence, _cname_to_latex
import exceptions

class DataFilter(object):
    """
    Class used to store, compare etc filter(-s)
    """
    def __init__(self, filters):
        self.filters = filters

    def sql_filter(self):
        if self.filters:
            s = str(self)
            if not len(s):
                return s
            return ' AND ' + s
        return str()

    def __str__(self):
        if self.filters:
            s = ' AND '.join('%s=%s' % (param, val) for param, val in self.filters if val != None)
            return s
        return str()

    def __nonzero__(self):
        if self.filters == None:
            return False
        return True

    def __len__(self):
        if self.filters == None:
            return 0
        return len(self.filters)

    def __eq__(self, other):
        if len(self) != len(other):
            return False
        if not len(self):
            return True
        for fltr in self.filters:
            if fltr not in other.filters:
                return False
        return True

class FracsData(object):
    def __init__(self, src, data_filter, options):
        self.options = options
        self.src = src
        self.tag_keys = None
        self.data_filter = DataFilter(data_filter)
        self.configurations_per_variant = None
        self.fraction_of_nodes = None
        self.dimension = None
        self.configurations = None
        self.figures = {}
        self._get_data()
        self._get_info()

    def _get_data(self):
        cursor = self.options['db_conn'].cursor()
        #######################################################################
        # create dynamic query based on the parameters that have changed
        # in different tags
        #######################################################################
        parameters = []
        for param in self.options['configurations']:
            if param not in ['p', 'gossip'] and len(self.options['configurations'][param]) > 1:
                parameters.append(param)
        parameters_str = str()
        if len(parameters):
            parameters_str = ', T.' + ', T.'.join(str(s) for s in parameters)
        query = '''
            SELECT DISTINCT(E.tag_key), T.p, T.gossip %s
            FROM eval_rx_fracs_for_tag AS E JOIN tag as T
            ON E.tag_key = T.key
            WHERE src=\'%s\' %s
            ORDER BY T.p, T.gossip %s
        ''' % (parameters_str, self.src, self.data_filter.sql_filter(), parameters_str)
        tags = cursor.execute(query).fetchall()
        assert(len(tags))

        #######################################################################
        # get the fraction of nodes the received the packets from the source
        # for each tag
        #######################################################################
        self.fraction_of_nodes = []
        self.label_info = []
        for tag in tags:
            tag_key = tag[0]
            fracs_for_p = cursor.execute('''
                SELECT frac
                FROM eval_rx_fracs_for_tag
                WHERE src=? AND tag_key=?
            ''', (self.src, tag_key)).fetchall()
            label_info = zip(parameters, tag[3::])
            self.fraction_of_nodes.append(list(pylab.flatten(fracs_for_p)))
            self.label_info.append(label_info)
        self.tag_keys = [tag[0] for tag in tags]

    def _get_info(self):
        cursor = self.options['db_conn'].cursor()

        parameters = cursor.execute('''
            PRAGMA TABLE_INFO(view_configurations)
        ''').fetchall()
        parameters = [name for _num, name, _type, _a, _b, _c in parameters]
        self.configurations = {}
        for param in parameters:
            self.configurations[param] = set()

        key_in = '('+','.join([str(s) for s in self.tag_keys])+')'
        query = '''
            SELECT DISTINCT *
            FROM tag
            WHERE key in %s
        ''' % (key_in)
        configurations = cursor.execute(query).fetchall()

        for config in configurations:
            for i, value in enumerate(config):
                if i > 2:
                    self.configurations[parameters[i-3]].add(value)

        # convert sets to sorted lists
        for key in self.configurations:
            self.configurations[key] = sorted(list(self.configurations[key]))

        self.configurations_per_p = 1
        self.dimension = 0
        for key, value in self.configurations.iteritems():
            if key != 'p':
                self.configurations_per_p *= len(value)
                if len(value) > 1:
                    self.dimension += 1
        self.configurations_per_variant = self.configurations_per_p / len(self.configurations['gossip'])

    def _data_to_array(self):
        logging.debug('')
        sample_size = len(self.fraction_of_nodes[0])
        bins = self.options['bins']

        array = numpy.zeros((bins, max(len(self.configurations['p']), 10)))
        for i, fracs in enumerate(self.fraction_of_nodes):
            ### TODO add binning on the probability axis
            hist, _bin_edges = numpy.histogram(fracs, bins=bins, range=(0.0, 1.0), normed=False)
            hist_norm = hist/float(sample_size)
            array[:, i] = hist_norm
        return array

    def plot(self):
        """
        Call all plotting functions
        """
        for func in [func for func in dir(self) if func.startswith('plot_')]:
            getattr(self, func)()

    def plot_wire_surface_pcolor(self):
        """
        Plot the fraction of executions as a function of the fraction of nodes for
        each source. Three plots are created: wireframe, surface, and pseudocolor.

        """
        logging.debug('')
        if self.dimension > 0:
            return
        array = self._data_to_array()

        x = pylab.linspace(0.0, 1.0, len(array[0])+1)
        y = pylab.linspace(0.0, 1.0, len(array)+1)
        X, Y = pylab.meshgrid(x, y)

        #fig_wire = MyFig(self.options, xlabel='Probability p', ylabel='Fraction of Nodes', ThreeD=True)
        #fig_surf = MyFig(self.options, xlabel='Probability p', ylabel='Fraction of Nodes', ThreeD=True)
        fig_pcol = MyFig(self.options, xlabel='Probability p', ylabel='Fraction of Nodes')

        #fig_wire.ax.plot_wireframe(X, Y, array)
        #fig_surf.ax.plot_surface(X, Y, array, rstride=1, cstride=1, linewidth=1, antialiased=True)
        pcolor = fig_pcol.ax.pcolor(X, Y, array, cmap=cm.jet, vmin=0.0, vmax=1.0)
        cbar = fig_pcol.fig.colorbar(pcolor, shrink=0.8, aspect=10)
        cbar.ax.set_yticklabels(pylab.linspace(0.0, 1.0, 11), fontsize=0.8*self.options['fontsize'])

        #for ax in [fig_wire.ax, fig_surf.ax]:
            #ax.set_zlim3d(0.0, 1.01)
            #ax.set_xlim3d(0.0, 1.01)
            #ax.set_ylim3d(0.0, 1.01)

        #self.figures['wireframe'] = fig_wire.save('wireframe_' + str(self.data_filter))
        #self.figures['surface'] = fig_surf.save('surface_' + str(self.data_filter))
        self.figures['pcolor'] = fig_pcol.save('pcolor_' + str(self.data_filter))

    def plot_box(self):
        """
        Plots the fraction of nodes that received a particular packet from the source
        as a box-and-whisker with the probability p on the x-axis.
        """
        logging.debug('')

        configurations_per_variant = self.configurations_per_variant
        gossip_variants_count = len(self.configurations['gossip'])
        colors = self.options['color'](pylab.linspace(0, 0.8, configurations_per_variant))

        labels = []
        for li_of_frac in self.label_info:
            s = str()
            for i, (param, value) in enumerate(li_of_frac):
                if i > 0:
                    s += '\n'
                s += '%s=%s' % (_cname_to_latex(param), value)
            labels.append(s)
        labels *= len(self.configurations['p'])
        ps = list(pylab.flatten(self.configurations_per_p*[p] for p in self.configurations['p']))

        #################################################################
        # box plot
        #################################################################
        array = numpy.zeros([len(self.fraction_of_nodes[0]), self.length()])
        for i, fracs in enumerate(self.fraction_of_nodes):
            array[:, i] = fracs

        fig = MyFig(self.options, rect=[0.1, 0.2, 0.8, 0.75], figsize=(max(self.length(), 10), 10), xlabel='Probability p', ylabel='Fraction of Nodes', aspect='auto')
        fig.ax.set_ylim(0, 1)
        box_dict = fig.ax.boxplot(array, notch=1, sym='rx', vert=1)
        #box_dict = fig.ax.boxplot(array, notch=1, sym='rx', vert=1, patch_artist=False)
        for j, box in enumerate(box_dict['boxes']):
            j = (j % self.configurations_per_p)
            box.set_color(colors[j])
        for _flier in box_dict['fliers']:
            _flier.set_color('lightgrey')
        fig.ax.set_xticklabels(ps, fontsize=self.options['fontsize']*0.6)
        # draw vertical line to visually mark different probabilities
        for x in range(0, self.length(), self.configurations_per_p):
            fig.ax.plot([x+0.5, x+0.5], [0.0, 1.0], linestyle='dotted', color='red', alpha=0.8)
        #################################################################
        # create some dummy elements for the legend
        #################################################################
        if configurations_per_variant > 1:
            proxies = []
            for i in range(0, configurations_per_variant):
                r = Rectangle((0, 0), 1, 1, edgecolor=colors[i % configurations_per_variant], facecolor='white')
                proxies.append((r, labels[i]))
            fig.ax.legend([proxy for proxy, label in proxies], [label for proxy, label in proxies], loc='lower right')
        self.figures['boxplot'] = fig.save('boxplot_' + str(self.data_filter))

    def plot_histogram(self):
        configurations_per_variant = self.configurations_per_variant
        colors = self.options['color'](pylab.linspace(0, 0.8, configurations_per_variant))

        labels = []
        for li_of_frac in self.label_info:
            s = str()
            for i, (param, value) in enumerate(li_of_frac):
                if i > 0:
                    s += '\n'
                s += '%s=%s' % (_cname_to_latex(param), value)
            labels.append(s)
        labels *= len(self.configurations['p'])
        ps = list(pylab.flatten(self.configurations_per_p*[p] for p in self.configurations['p']))

        #################################################################
        # histogram plot
        #################################################################
        fig2 = MyFig(self.options, rect=[0.1, 0.2, 0.8, 0.75], figsize=(max(self.length(), 10), 10), xlabel='Probability p', ylabel='Fraction of Nodes', aspect='auto')
        patches = []
        for i, fracs in enumerate(self.fraction_of_nodes):
            hist, bin_edges = numpy.histogram(fracs, bins=pylab.linspace(0, 1, self.options['bins']))
            hist_norm = hist/float(len(fracs))*0.4
            yvals = list(bin_edges.repeat(2))[1:-1]
            yvals.extend(list(bin_edges.repeat(2))[1:-1][::-1])
            xvals = list((i+1+hist_norm).repeat(2))
            xvals.extend(list((i+1-hist_norm).repeat(2))[::-1])
            i = (i % self.configurations_per_p)
            poly = Polygon(zip(xvals, yvals), edgecolor='black', facecolor=colors[i], closed=True)
            patches.append(poly)
        patch_collection = PatchCollection(patches, match_original=True)
        fig2.ax.add_collection(patch_collection)
        fig2.ax.set_xticks(range(1, self.length() + 1))
        fig2.ax.set_xticklabels(ps, fontsize=self.options['fontsize']*0.6)
        for x in range(0, self.length(), self.configurations_per_p):
            fig2.ax.plot([x+0.5, x+0.5], [0.0, 1.0], linestyle='dotted', color='red', alpha=0.8)
        fig2.ax.set_ylim(0, 1)
        #################################################################
        # create some dummy elements for the legend
        #################################################################
        if configurations_per_variant > 1:
            proxies = []
            #for i in range(0, len(self.configurations['gossip'])):
            for i in range(0, configurations_per_variant):
                r = Rectangle((0, 0), 1, 1, facecolor=colors[i % configurations_per_variant], edgecolor='black')
                proxies.append((r, labels[i]))
            fig2.ax.legend([proxy for proxy, label in proxies], [label for proxy, label in proxies], loc='lower right')
        self.figures['histogram'] = fig2.save('histogram_' + str(self.data_filter))

    def _plot_bar3d(self):
        logging.debug('')
        if self.dimension > 0:
            return
        array = self._data_to_array()

        # width, depth, and height of the bars (array == height values == dz)
        dx = list(numpy.array([1.0/len(array[0]) for x in range(0, array.shape[1])]))
        dy = list(numpy.array([1.0/len(array) for x in range(0, array.shape[0])]))
        dx *= len(array)
        dy *= len(array[0])
        dz = array.flatten()+0.00000001 # dirty hack to cirumvent ValueError

        # x,y,z position of each bar
        x = pylab.linspace(0.0, 1.0, len(array[0]), endpoint=False)
        y = pylab.linspace(0.0, 1.0, len(array), endpoint=False)
        xpos, ypos = pylab.meshgrid(x, y)
        xpos = xpos.flatten()
        ypos = ypos.flatten()
        zpos = numpy.zeros(array.shape).flatten()

        fig = MyFig(self.options, xlabel='Probability p', ylabel='Fraction of Nodes', zlabel='Fraction of Executions', ThreeD=True)
        fig.ax.set_zlim3d(0.0, 1.01)
        fig.ax.set_xlim3d(0.0, 1.01)
        fig.ax.set_ylim3d(0.0, 1.01)
        fig.ax.set_autoscale_on(False)

        assert(len(dx) == len(dy) == len(array.flatten()) == len(xpos) == len(ypos) == len(zpos))
        fig.ax.bar3d(xpos, ypos, zpos, dx, dy, dz, color=['#CCBBDD'])
        try:
            self.figures['bar3d'] = fig.save('bar3d' + str(self.data_filter))
        except ValueError, err:
            logging.warning('%s', err)

    def plot_distribution(self):
        logging.debug('')
        if self.dimension > 0:
            return

        probabilities = pylab.linspace(0, 1, 20)
        distributions = {}
        for name, func, extra_args in [('data', None, []), ('normal', stats.norm, [])]:
        #('chi2', stats.chi2), ('gamma', stats.gamma)]:
            fig_cdf = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='CDF', grid=True, legend=True)
            fig_cdf.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
            fig_qq = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='%s Distribution' % name, grid=True, legend=True)
            fig_qq.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
            fig_qq.ax.set_xlim(0.0, 1.0)
            fig_qq.ax.set_ylim(0.0, 1.0)
            distributions[name] = (func, fig_cdf, fig_qq, extra_args)

        #fig_cdf_data = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='CDF', grid=True, legend=True)
        #fig_cdf_data.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

        #########################################
        #fig_qq_normal = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='Normal Distribution', grid=True, legend=True)
        #fig_qq_normal.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
        #fig_qq_normal.ax.set_xlim(0.0, 1.0)
        #fig_qq_normal.ax.set_ylim(0.0, 1.0)

        #fig_cdf_normal = MyFig(self.options, xlabel='x', ylabel='CDF', grid=True, legend=True)
        #fig_cdf_normal.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

        #########################################
        #fig_qq_gamma = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='Gamma Distribution', grid=True, legend=True)
        #fig_qq_gamma.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
        #fig_qq_gamma.ax.set_xlim(0.0, 1.0)
        #fig_qq_gamma.ax.set_ylim(0.0, 1.0)

        #fig_cdf_gamma = MyFig(self.options, xlabel='x', ylabel='CDF', grid=True, legend=True, aspect='auto')
        #fig_cdf_gamma.ax.set_xlim(0.0, 1.0)

        #########################################
        #fig_qq_2normal = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='Normal Distribution', grid=True, legend=True)
        #fig_qq_2normal.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
        #fig_qq_2normal.ax.set_xlim(0.0, 1.0)
        #fig_qq_2normal.ax.set_ylim(0.0, 1.0)

        #fig_cdf_2normal = MyFig(self.options, xlabel='x', ylabel='CDF', grid=True, legend=True)
        #fig_cdf_2normal.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

        #########################################
        #fig_qq_2chi2 = MyFig(self.options, xlabel='Fraction of Nodes', ylabel='2x Chi Square Distribution', grid=True, legend=True)
        #fig_qq_2chi2.ax.plot([0.0, 1.0], [0.0, 1.0], color='black', linestyle='solid')
        #fig_qq_2chi2.ax.set_xlim(0.0, 1.0)
        #fig_qq_2chi2.ax.set_ylim(0.0, 1.0)

        #fig_cdf_2chi2 = MyFig(self.options, xlabel='x', ylabel='CDF', grid=True, legend=True)
        #fig_cdf_2chi2.ax.plot([0.0, 1.2], [0.0, 1.2], color='black', linestyle='solid')

        colors = self.options['color'](pylab.linspace(0, 0.8, self.length()))
        markers = self.options['markers']
        for j, data in enumerate(self.fraction_of_nodes):
            label = 'p=%.2f' % self.configurations['p'][j]
            avr = scipy.average(data)
            sigma = scipy.std(data)
            quantiles_data = stats.mstats.mquantiles(data, prob=probabilities)

            for name in distributions:
                func, fig_cdf, fig_qq, extra_args = distributions[name]
                if func:
                    quantiles_stat = func.ppf(probabilities, *extra_args, loc=avr, scale=sigma)
                    fig_qq.ax.plot(quantiles_data, quantiles_stat, 'o', color=colors[j], linestyle='-', label=label, marker=markers[j])
                else:
                    quantiles_stat = quantiles_data
                fig_cdf.ax.plot(quantiles_stat, probabilities, 'o', color=colors[j], linestyle='-', label=label, marker=markers[j])

            #fig_cdf_data.ax.plot(quantiles_data, probabilities, 'o', color=colors[j], linestyle='-', label=label)
            ###########################################################################
            # Normal Distribution
            ###########################################################################
            #quantiles_normal = stats.norm.ppf(probabilities, loc=avr, scale=sigma)
            #fig_cdf_normal.ax.plot(quantiles_normal, probabilities, 'o', color=colors[j], linestyle='-', label=label)
            #fig_qq_normal.ax.plot(quantiles_data, quantiles_normal, 'o', color=colors[j], linestyle='-', label=label)
            ###########################################################################
            # Gamma Distribution
            ###########################################################################
            #quantiles_gamma = stats.gamma.ppf(probabilities, 0.4, loc=avr, scale=sigma)
            #_quantiles_gamma = []
            #for x in quantiles_gamma:
                #if x != numpy.infty:
                    #_quantiles_gamma.append(min(1.0,x))
                #else:
                    #_quantiles_gamma.append(x)
            #quantiles_gamma = numpy.array(_quantiles_gamma)
            #fig_cdf_gamma.ax.plot(quantiles_gamma, probabilities, 'o', color=colors[j], linestyle='-', label=label)
            #fig_qq_gamma.ax.plot(quantiles_data, quantiles_gamma, 'o', color=colors[j], linestyle='-', label=label)
            ###########################################################################
            # 2x Chi Square Distribution
            ###########################################################################
            #data_2chi2 = []
            #for i in range(0, 5000):
                #sigma_2chi2 = p*0.5
                #dv = 0.5
                #if random.normalvariate(0.6, 0.1) < p:
                    #exp_2chi2 = 1.0
                    #d = stats.chi2.rvs(dv, loc=exp_2chi2, scale=sigma_2chi2, size=1)
                    #d = exp_2chi2-abs(exp_2chi2-d)
                #else:
                    #exp_2chi2 = 0.05
                    #d = stats.chi2.rvs(dv, loc=exp_2chi2, scale=sigma_2chi2, size=1)
                #data_2chi2.append(max(min(d[0], 1.0), 0.0))
            #quantiles_data_2chi2 = stats.mstats.mquantiles(data_2chi2, prob=probabilities)
            #fig_cdf_2chi2.ax.plot(quantiles_data_2chi2, probabilities, 'o', linestyle='-', label='p=%.2f' % p, color=colors[j])
            #fig_qq_2chi2.ax.plot(quantiles_data, quantiles_data_2chi2, 'o', color=colors[j], linestyle='-', label=label)

            ###########################################################################
            # 2x Normal Distribution
            ###########################################################################
            #data_2normal = []
            #for i in range(0, 2000):
                #if random.uniform(0.0, 1.0) < p:
                    #exp_2normal = 1.0
                    #sigma_2normal = 0.05
                    #d = stats.norm.rvs(loc=exp_2normal, scale=sigma_2normal, size=1)
                    #d = exp_2normal-abs(exp_2normal-d)
                #else:
                    #exp_2normal = 0.05
                    #sigma_2normal = 0.05
                    #d = stats.norm.rvs(loc=exp_2normal, scale=sigma_2normal, size=1)
                #data_2normal.append(max(min(d[0], 1.0), 0.0))
            #quantiles_data_2normal = stats.mstats.mquantiles(data_2normal, prob=probabilities)
            #fig_cdf_2normal.ax.plot(quantiles_data_2normal, probabilities, 'o', linestyle='-', label='p=%.2f' % p, color=colors[j])
            #fig_qq_2normal.ax.plot(quantiles_data, quantiles_data_2normal, 'o', color=colors[j], linestyle='-', label=label)

        for name in distributions:
            func, fig_cdf, fig_qq, extra_args = distributions[name]
            if func:
                fig_qq.save('distribution-qq_%s_%s' % (name, str(self.data_filter)))
            fig_cdf.save('distribution-cdf_%s_%s' % (name, str(self.data_filter)))

    def plot_redundancy(self):
        """
        Plot the fraction of (unique) packets received from the source (0.0 - 1.0) over the
        total number of packets received as fraction (0.0 - infty).
        A cubic curve is fit to the data points.
        """
        if self.dimension > 0:
            return
        cursor = self.options['db_conn'].cursor()
        max_total, = cursor.execute('''
            SELECT MAX(total)
            FROM eval_fracsOfHosts
        ''').fetchone()
        fig_all = MyFig(self.options, xlabel='Fraction of rx Packets (incl. duplicates)', ylabel='Fraction of rx Packets sent by %s' % self.src, legend=True, aspect='auto')
        fig_all.ax.plot([1, 1], [0, 1], linestyle='dashed', color='grey')
        fig_all_median = MyFig(self.options, xlabel='Fraction of rx Packets (incl. duplicates)', ylabel='Fraction of rx Packets sent by %s' % self.src, legend=True, aspect='auto')
        fig_all_median.ax.plot([1, 1], [0, 1], linestyle='dashed', color='grey')
        colors = self.options['color'](pylab.linspace(0, 0.8, len(self.tag_keys)))
        markers = self.options['markers']
        max_x = 0

        ellipses = []
        for j, tag_key in enumerate(self.tag_keys):
            fig = MyFig(self.options, xlabel='Fraction of rx Packets (incl. duplicates)', ylabel='Fraction of rx Packets sent by %s' % self.src, aspect='auto')
            results = cursor.execute('''
                SELECT total, frac
                FROM eval_fracsOfHosts
                WHERE src=? AND tag_key=?
            ''', (self.src, tag_key)).fetchall()
            assert(len(results))
            fig.ax.plot([1, 1], [0, 1], linestyle='-', color='grey')
            xvals = [x[0] for x in results]
            yvals = [y[1] for y in results]
            label = 'p=%.2f' % self.configurations['p'][j]
            fig.ax.scatter(xvals, yvals, s=15, color=colors[j])
            fig.ax.set_xlim((0, max_total))
            max_x = max(max_x, max_total)
            fig.ax.set_ylim((0, 1))
            z = numpy.polyfit(xvals, yvals, 3)
            poly = numpy.poly1d(z)

            median_y = scipy.median(yvals)
            mean_y = scipy.mean(yvals)
            ci_y = confidence(yvals)
            median_x = scipy.median(xvals)
            mean_x = scipy.mean(xvals)
            ci_x = confidence(xvals)
            ellipse = Ellipse((mean_x, mean_y), ci_x[1]-ci_x[0], ci_y[1]-ci_y[0], edgecolor=colors[j], facecolor=colors[j], alpha=0.5)
            ellipses.append(ellipse)

            selected_xvals = numpy.arange(min(xvals), max(xvals), 0.4)

            fig.ax.plot(selected_xvals, poly(selected_xvals),"-", color=colors[j])
            fig.ax.plot([0.0, 10], [median_y, median_y], linestyle="dashed", color=colors[j])

            fig_all.ax.plot(selected_xvals, poly(selected_xvals),"-", color=colors[j], label=label, marker=markers[j])
            fig_all.ax.plot([0.0, 10], [median_y, median_y], linestyle="dashed", color=colors[j], alpha=0.6, marker=markers[j])

            fig_all_median.ax.plot(ci_x, [mean_y, mean_y], color=colors[j], label=label,  marker=markers[j])
            fig_all_median.ax.plot([mean_x, mean_x], ci_y, color=colors[j],  marker=markers[j])

            fig.save('redundancy_%s_%s' % (label, str(self.data_filter)))
        fig_all.ax.axis((0, max(max_x, 1), 0, 1))
        fig_all_median.ax.axis((0, max(max_x, 1), 0, 1))
        patch_collection = PatchCollection(ellipses, match_original=True)
        fig_all_median.ax.add_collection(patch_collection)
        fig_all.save('redundancy_%s' % str(self.data_filter))
        fig_all_median.save('redundancy_median_%s' % str(self.data_filter))


    def latex_figure(self, plot, subfig=True):
        file_name, fig_label = self.figures[plot]
        caption = self.src
        if subfig:
            subfig = '\\subfloat[%s]{\\label{fig:%s}\\includegraphics[width=_WIDTH_]{%s}}' % (caption, fig_label, file_name)
            return subfig
        else:
            figure = '\\begin{figure}\n'
            figure += '\t\\includegraphics[width=\linewidth]{%s}\n' % (file_name)
            figure += '\t\\caption{%s}\n' % (caption)
            figure += '\t\\label{fig:%s}\n' % (fig_label)
            figure += '\\end{figure}\n\n'
            return figure

    def length(self):
        return len(self.fraction_of_nodes)

    def __str__(self):
        s = 'FracsData: gossip=%s, probs=%d, dim=%d, filter=\"%s\"' % (self.configurations['gossip'], len(self.configurations['p']), self.dimension, str(self.data_filter))
        return s

    def __sub__(self, other):
        # TODO allow operation if dimensions are > 0 but equal/same parameters
        if self.dimension or other.dimension:
            raise DimensionError
        if self.configurations['p'] != other.configurations['p']:
            raise ProbabilityError
        new_fracs = []
        for _frac1, _frac2 in zip(self.fraction_of_nodes, other.fraction_of_nodes):
            new_fracs.append(0) # TODO

    class DimensionError(exceptions.Exception):
        def __init__(self):
            return

        def __str__(self):
            print "","At least one of the data sets has dimension > 0"

    class ProbabilityError(exceptions.Exception):
        def __init__(self):
            return

        def __str__(self):
            print "","The data sets are for incompatible probability values"

def _generate_filter_tuples(options):
    """
    TODO recursive generation should be replaced by permutation
    """
    values_dict = options['configurations']
    values_tuples = []
    for param in values_dict:
        if len(values_dict[param]) == 1 or param == 'p':
            continue
        values_tuples.append((param, sorted(list(values_dict[param]))))
    return _generate_filter_recursive([None], list(), values_tuples)

def _generate_filter_recursive(filters, cur_filter, values_tuples):
    if len(values_tuples):
        if len(cur_filter):
            filters.append(cur_filter)
        vt = list(values_tuples)
        param, values = vt.pop()
        for val in values:
            cf = list(cur_filter)
            cf.append((param, val))
            _generate_filter_recursive(filters, cf, vt)
    else:
        if len(cur_filter):
            filters.append(cur_filter)
    return filters

def _generate_filter_tuples2(options):
    """
    TODO recursive generation should be replaced by permutation
    """
    values_dict = options['configurations']
    values_tuples = []
    for param in values_dict:
        if len(values_dict[param]) == 1 or param == 'p':
            continue
        lst = sorted(list(values_dict[param]))
        lst.insert(0, None)
        values_tuples.append((param, lst))
    return _generate_filter_recursive2([], list(), values_tuples)

def _generate_filter_recursive2(filters, cur_filter, values_tuples):
    if len(values_tuples):
        vt = list(values_tuples)
        param, values = vt.pop()
        for val in values:
            cf = list(cur_filter)
            cf.append((param, val))
            _generate_filter_recursive2(filters, cf, vt)
    else:
        if len(cur_filter):
            filters.append(cur_filter)
    return filters

def _print_filter_tuples(filter_tuples):
    logging.info('All Filter Tuples:')
    for _filter in filter_tuples:
            if _filter == None:
                print('FOOBAR: None')
                continue
            s = ', '.join('%s=%s' % (p,v) for p,v in _filter)
            logging.info('\t'+s)

def plot_single(options):
    """
    Creates appropriate filters based on the variable parameters of the
    experiment and creates objects to query and plot the data.
    """
    filter_tuples = _generate_filter_tuples2(options)
    if not len(filter_tuples):
        filter_tuples = [(None)]
    _print_filter_tuples(filter_tuples)
    for i, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, i+1, len(options['src']))
        options['prefix'] = src
        data_set = []
        for _filter in filter_tuples:
            data = FracsData(src, _filter, options)
            logging.info('\t%s', data)
            data.plot()
            data_set.append(data)

        options['latex_file'].write('\\subsection{Results for Source %s}\n' % src)
        options['latex_file'].write('\\subsubsection{Overview}\n')

        for i, data in enumerate([d for d in data_set if d.dimension > 0]):
            fig_box = data.latex_figure('boxplot', subfig=True).replace('_WIDTH_', '\linewidth')
            fig_hist = data.latex_figure('histogram', subfig=True).replace('_WIDTH_', '\linewidth')
            options['latex_file'].write('\\begin{figure}\n')
            options['latex_file'].write(fig_box + ' \\\\\n' + fig_hist + '\n')
            options['latex_file'].write('\\end{figure}\n')

            if (i + 1) % 16 == 0:
                options['latex_file'].write('\\clearpage\n')

        options['latex_file'].write('\\clearpage\n')
        options['latex_file'].write('\\subsubsection{Detailed View}\n')
        for data in [d for d in data_set if d.dimension == 0]:
            # for all scenarios with at least one free variable
            #options['latex_file'].write(data.latex_figure(subfig=True))
            pass
        options['latex_file'].write('\\clearpage\n')

def _plot_propagation(options):
    """
    Plot the fraction of packets each router received from its neighbors.
    This can be used to (visually) detect routers that depend on a particular
    neighbors.

    tags: Can be used with any tag/configuration. One plot is generated for each!
    """
    locs = options['locs']
    cursor = options['db_conn'].cursor()
    colors = options['color2'](pylab.linspace(0, 1, 101))
    units2meter = options['units2meter']

    #################################################################################
    ## Get mapping of hosts and interface addresses
    #################################################################################
    cursor.execute('''
        SELECT DISTINCT(host), rx_if
        FROM rx
    ''')
    addr2host = {}
    for host, rx_if in cursor.fetchall():
        addr2host[rx_if] = host
    ################################################################################
    # Evaluate for all sources
    ################################################################################
    for i, src in enumerate(options['src']):
        logging.info('src=%s (%d/%d)', src, i+1, len(options['src']))
        options['prefix'] = src
        #################################################################################
        ## Get all hostnames
        #################################################################################
        #cursor.execute('SELECT host FROM addr')
        #dsts = sorted([str(d[0]) for d in cursor.fetchall()])
        ################################################################################
        # Evaluate received packets for each tag for each node
        ################################################################################
        tags = cursor.execute('''
            SELECT key, id
            FROM tag
        ''').fetchall()
        for j, (tag_key, tag_id) in enumerate(tags):
            logging.info('\ttag=%s (%d/%d)', tag_id, j+1, len(tags))
            results = cursor.execute('''
                SELECT host, total, frac
                FROM eval_fracsOfHosts
                WHERE src=? AND tag_key=?
            ''', (src, tag_key)).fetchall()
            ################################################################################
            # Draw figure for current tag
            ################################################################################
            fig = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate')
            fig3d = MyFig(options, rect=[0.1, 0.1, 0.8, 0.7], xlabel='x Coordinate', ylabel='y Coordinate', zlabel='z Coordinate', ThreeD=True)
            fig.ax.set_autoscalex_on(False)
            fig.ax.set_autoscaley_on(False)
            min_x = min_y = numpy.infty
            max_x = max_y = max_z = 0
            circ_max = 5
            line_max = 10
            floor_factor = 2
            floor_skew = -0.25
            line_min = 1
            # first draw the links....
            for host, _total, _frac in results:
                try:
                    xpos, ypos, zpos = locs[host]
                except KeyError:
                    logging.warning('no position found for node %s', host)
                    continue
                xpos = xpos*units2meter
                ypos = ypos*units2meter
                zpos = zpos*units2meter
                prevs =  cursor.execute('''
                    SELECT prev, frac
                    FROM eval_prevHopFraction
                    WHERE src=? AND tag_key=? and cur=?
                ''', (src, tag_key, host)).fetchall()
                for prev, frac in prevs:
                    try:
                        prev_xpos, prev_ypos, prev_zpos = locs[addr2host[prev]]
                    except KeyError:
                        logging.warning('no position found for node %s', prev)
                        continue
                    prev_xpos = prev_xpos*units2meter
                    prev_ypos = prev_ypos*units2meter
                    prev_zpos = prev_zpos*units2meter
                    fig.ax.plot(
                        [xpos+zpos*floor_skew*floor_factor, prev_xpos+prev_zpos*floor_skew*floor_factor],
                        [ypos+zpos*floor_factor, prev_ypos+prev_zpos*floor_factor],
                        linestyle='-', color=colors[frac*100], linewidth=max(line_max*frac, line_min), alpha=0.3)
                    fig3d.ax.plot(
                        [xpos, prev_xpos],
                        [ypos, prev_ypos],
                        [zpos, prev_zpos],
                        linestyle='-', color=colors[frac*100], linewidth=max(line_max*frac, line_min), alpha=0.3)
            # ...then draw the nodes
            for host, _total, frac in results:
                try:
                    xpos, ypos, zpos = locs[host]
                except KeyError:
                    logging.warning('no position found for node %s', host)
                    continue
                xpos = xpos*units2meter
                ypos = ypos*units2meter
                zpos = zpos*units2meter
                max_x = max(xpos, max_x)
                max_y = max(ypos, max_y)
                min_x = min(xpos, min_x)
                min_y = min(ypos, min_y)
                max_z = max(zpos, max_z)
                fig.ax.plot(
                    xpos+zpos*floor_skew*floor_factor,
                    ypos+zpos*floor_factor,
                    'o', color=colors[int(frac*100)], ms=max(frac*circ_max, 1))
                fig3d.ax.plot(
                    [xpos],
                    [ypos],
                    [zpos],
                    'o', color=colors[int(frac*100)], ms=max(frac*circ_max, 1))
            drawBuildingContours(fig3d.ax, options)
            fig.ax.axis((min_x-10, max_x+10, min_y-10, max_y+10+max_z*floor_factor+10))
            colorbar_ax = fig.fig.add_axes([0.1, 0.875, 0.8, 0.025])
            colorbar_ax3d = fig3d.fig.add_axes([0.1, 0.875, 0.8, 0.025])
            alinspace = numpy.linspace(0, 1, 100)
            alinspace = numpy.vstack((alinspace, alinspace))
            for tax in [colorbar_ax, colorbar_ax3d]:
                tax.imshow(alinspace, aspect='auto', cmap=options['color2'])
                tax.set_xticks(range(0, 101, 25))
                tax.set_xticklabels(numpy.arange(0.0, 101.0, 0.25), fontsize=0.8*options['fontsize'])
                tax.set_yticks([])
                tax.set_title(tag_id, size=options['fontsize'])
            fig.save('propagation2d_%s' % tag_id)
            fig3d.save('propagation3d_%s' % tag_id)
