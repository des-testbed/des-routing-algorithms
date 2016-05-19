#!/usr/bin/env python
# -*- coding: utf-8 -*-

import scipy
import pylab
import argparse
import numpy
import networkx as nx
import os
from multiprocessing import Pool
from helpers import MyFig, fu_colormap, confidence, conf2poly
import export
from matplotlib.patches import Polygon
from matplotlib.collections import PatchCollection

def draw(options):
    files = [f for f in os.listdir(options['outdir']) if f.endswith('.data')]

    degrees = list()
    diameters = list()
    velocities = list()
    for f in files:
        fin = open(options['outdir']+'/'+f, 'r')
        ts = -1
        for line in fin:
            if line.startswith('#'):
                continue
            time, degree, diameter, velocity = [t.strip() for t in line.split(',')]
            time = int(time)
            assert(ts == time-1)
            ts = time
            try:
                degrees[time].append(float(degree))
                diameters[time].append(int(diameter))
                velocities[time].append(float(velocity))
            except IndexError:
                degrees.append([float(degree)])
                diameters.append([int(diameter)])
                velocities.append([float(velocity)])

    polies = list()
    times = range(len(degrees))
    times2 = times + times[::-1]

    degrees_conf_upper = [confidence(d)[0] for d in degrees]
    degrees_conf_lower = [confidence(d)[1] for d in degrees]
    polies.append(conf2poly(times, degrees_conf_upper, degrees_conf_lower, color='blue'))

    diameters_conf_upper = [confidence(d)[0] for d in diameters]
    diameters_conf_lower = [confidence(d)[1] for d in diameters]
    polies.append(conf2poly(times, diameters_conf_upper, diameters_conf_lower, color='blue'))

    velocities_conf_upper = [confidence(d)[0] for d in velocities]
    velocities_conf_lower = [confidence(d)[1] for d in velocities]
    polies.append(conf2poly(times, velocities_conf_upper, velocities_conf_lower, color='green'))

    velocities = [scipy.mean(d) for d in velocities]
    diameters = [scipy.mean(d) for d in diameters]
    degrees = [scipy.mean(d) for d in degrees]

    fig = MyFig(options, figsize=(10, 8), xlabel='Time [s]', ylabel='Metric', grid=False, legend=True, aspect='auto', legend_pos='upper right')

    patch_collection = PatchCollection(polies, match_original=True)
    patch_collection.set_alpha(0.3)
    patch_collection.set_linestyle('dashed')
    fig.ax.add_collection(patch_collection)

    fig.ax.plot(times, degrees, label='Mean degree', color='blue')
    fig.ax.plot(times, diameters, label='Diameter', color='red')
    fig.ax.plot(times, velocities, label='Mean velocity $[m/s]$', color='green')

    fig.ax.set_xlim(0, options['duration'])
    y_max = max(max(degrees), max(diameters), max(velocities))
    fig.ax.set_ylim(0, y_max+10)
    fig.save('metrics', fileformat='pdf')

def main():
    parser = argparse.ArgumentParser(description='Run random waypoint model')
    parser.add_argument('--outdir', default='./', help='output directory for the results')
    parser.add_argument('--nodes', '-n', type=int, default=40, help='number of nodes')
    parser.add_argument('--fontsize', '-f', type=int, default=24, help='fontsize')
    parser.add_argument('--speed', '-s', nargs='+', type=int, default=[1, 20], help='speed [m/s]')
    parser.add_argument('--duration', '-d', type=int, default=600, help='duration [s]')
    parser.add_argument('--range', '-r', type=int, default=250, help='radio range [m]')
    parser.add_argument('--steps', type=int, default=1, help='steps [s]')
    parser.add_argument('--size', nargs='+', type=int, default=[1000, 1000], help='steps [s]')
    parser.add_argument('--movie', '-m', nargs='?', const=True, default=False, help='Create movie')
    parser.add_argument('--graphs', '-g', type=int, default=1, help='number of graphs/replications')
    parser.add_argument('--processes', '-p', type=int, default=1, help='number of parallel processes')
    parser.add_argument('--export', '-e', nargs='?', const=True, default=False, help='export graphs to ned files (default: false)')
    args = parser.parse_args()

    options = dict()
    options['outdir'] = args.outdir
    options['nodes'] = args.nodes
    options['fontsize'] = args.fontsize
    options['speed'] = args.speed
    options['duration'] = args.duration
    options['steps'] = args.steps
    options['size'] = args.size
    options['range'] = args.range
    options['prefix'] = 'random_waypoint'
    options['color'] = fu_colormap()
    options['movie'] = args.movie
    options['graphs'] = args.graphs
    options['processes'] = args.processes
    options['export'] = args.export

    pool = Pool(processes=options['processes'])
    data = [(options, i) for i in range(options['graphs'])]
    pool.map(run, data)
    draw(options)

class Node:
    pos = None
    velocity = None
    max_velocity = None
    destination = None
    name = None

    def __init__(self, max_velocity, name):
        self.pos = self.get_pos()
        self.max_velocity = max_velocity
        self.destination = self.get_pos()
        self.velocity = self.get_velocity()
        self.name = name

    def get_pos(self):
        return numpy.array([scipy.random.uniform()*1000, scipy.random.uniform()*1000])

    def get_velocity(self):
        return max(scipy.random.uniform()*self.max_velocity, 1)

    def cur_pos(self):
        return self.pos

    def cur_velocity(self):
        return self.velocity

    def walk(self, duration):
        distance_to_dest = scipy.sqrt(sum((self.pos - self.destination)**2))
        distance = self.velocity*duration
        if distance < distance_to_dest:
            gamma = self.destination - self.pos
            angle = numpy.arctan2(gamma[1], gamma[0])
            delta_x = numpy.cos(angle) * distance
            delta_y = numpy.sin(angle) * distance
            self.pos += [delta_x, delta_y]
            #print self.pos, self.velocity, self.destination, angle
            assert(self.pos[0] < 1000 and self.pos[1] < 1000)
        else:
            self.pos = self.destination
            time_left = duration - distance_to_dest/self.velocity
            if(time_left > duration):
                print time_left, duration
                assert(False)
            #print '#### arrived ####'
            self.destination = self.get_pos()
            self.velocity = self.get_velocity()
            self.walk(time_left)

def run((options, num)):
    def connect_graph(G, range):
        for n in G.nodes():
            for m in G.nodes():
                if m <= n:
                    continue
                distance = scipy.sqrt(sum((nodes[n].cur_pos() - nodes[m].cur_pos())**2))
                if distance <= range:
                    G.add_edge(n, m)

    def eval_graph(G, l):
        components = nx.connected_components(G)
        diameter = nx.algorithms.diameter(G.subgraph(components[0]))
        mean_velocity = scipy.mean([n.cur_velocity() for n in nodes])
        mean_degree = scipy.mean(G.degree().values())
        l.append((time, mean_degree, diameter, mean_velocity))

    def draw_graph(G, time):
        if not options['movie']:
            return
        fig = MyFig(options)
        for (n1, n2) in G.edges():
            fig.ax.plot([nodes[n1].cur_pos()[0], nodes[n2].cur_pos()[0]], [nodes[n1].cur_pos()[1], nodes[n2].cur_pos()[1]], color='black', linestyle='solid', alpha=0.2)
        for i,n in enumerate(nodes):
            fig.ax.plot(n.cur_pos()[0], n.cur_pos()[1], marker='o', color=colors[i])
        fig.ax.set_xlim(0, 1000)
        fig.ax.set_ylim(0, 1000)
        fig.save('graph-%04d-%04d' % (num, time), fileformat='png')

    scipy.random.seed()
    nodes = [Node(options['speed'][1], i) for i in range(0, options['nodes'])]
    colors = options['color'](pylab.linspace(0, 1, options['nodes']))
    time = 0
    values = list()
    metrics_out = open('%s/%04d-metrics.data' % (options['outdir'], num), 'w')
    metrics_out.write('# Metrics of a network where nodes move as defined by the random waypoint model\n')
    metrics_out.write('#\n')
    metrics_out.write('# %-11s: %8d\n' % ('nodes', options['nodes']))
    metrics_out.write('# %-11s: %8d\n' % ('min speed', options['speed'][0]))
    metrics_out.write('# %-11s: %8d\n' % ('max speed', options['speed'][1]))
    metrics_out.write('# %-11s: %8d\n' % ('radio range', options['range']))
    metrics_out.write('# %-11s: %8d\n' % ('duration', options['duration']))
    metrics_out.write('# %-11s: %8d\n' % ('steps', options['steps']))
    metrics_out.write('#\n')
    metrics_out.write('# column 0: time since start of movement [s]\n')
    metrics_out.write('# column 1: computed mean node degree\n')
    metrics_out.write('# column 2: computed network diameter [hops] (of largest component if partitioned)\n')
    metrics_out.write('# column 3: computed mean velocity [m/s]\n')
    metrics_out.write('#\n')
    metrics_out.write('# time, degree, diameter, velocity\n')

    G = nx.Graph()
    G.add_nodes_from([n.name for n in nodes])
    connect_graph(G, options['range'])
    eval_graph(G, values)
    draw_graph(G, 0)

    graphs = list()
    while (time +  options['steps']) <= options['duration']:
        time += options['steps']
        for n in nodes:
            n.walk(options['steps'])
        G = nx.Graph()
        G.add_nodes_from([n.name for n in nodes])
        if options['export']:
            for n in nodes:
                npos = n.cur_pos()
                G.node[n.name]['pos'] = npos
        connect_graph(G, options['range'])
        eval_graph(G, values)
        draw_graph(G, time)
        if options['export']:
            graphs.append(G)
    if options['export']:
        print 'exporting'
        options['graph'] = graphs
        export.graphToNEDMixim(options, '%04d-' % num)

    times = [t for (t, m, d, v) in values]
    degrees = [m for (t, m, d, v) in values]
    diameters = [d for (t, m, d, v) in values]
    velocities = [v for (t, m, d, v) in values]
    y_max = max([max(degrees), max(diameters), max(velocities)])
    for i, t in enumerate(times):
        metrics_out.write('%4d, %f, %2d, %f\n' % (t, degrees[i], diameters[i], velocities[i]))

        if options['movie']:
            fig = MyFig(options, figsize=(10, 8), xlabel='Time [s]', ylabel='Metric', grid=False, legend=True, aspect='auto', legend_pos='upper right')
            fig.ax.plot(times[0:i+1], degrees[0:i+1], label='Mean degree')
            fig.ax.plot(times[0:i+1], diameters[0:i+1], label='Diameter')
            fig.ax.plot(times[0:i+1], velocities[0:i+1], label='Mean velocity $[m/s]$')
            fig.ax.set_xlim(0, options['duration'])
            fig.ax.set_ylim(0, y_max+10)
            fig.save('%s/metrics-%04d' % (options['outdir'], t), fileformat='png')
            os.system('montage -geometry 1280x720 -tile 2x1 -background white -depth 8 -mode Concatenate %s/%s-graph-%04d.png %s/%s-metrics-%04d.png %s/%s-%04d.png' % (options['outdir'],  options['prefix'], i, options['outdir'],  options['prefix'], i, options['outdir'], options['prefix'], i))

    if options['movie']:
        cmd = 'ffmpeg -y -qscale 5 -r 10 -b 9600 -i %s/%s-%04d-%%04d.png %s/%04d-movie.mp4' % (options['outdir'], options['prefix'], num, options['outdir'], num)
        print cmd
        os.system(cmd)
        os.system('rm %s/random_waypoint-*.png' % options['outdir'])

    print 'finished: %d' % (num)

if __name__ == '__main__':
    main()