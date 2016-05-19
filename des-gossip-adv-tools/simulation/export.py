#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse
import sys, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pylab
import scipy
import xml.etree.ElementTree as ET
import networkx as nx

def export_main():
    parser = argparse.ArgumentParser(description='Parse log files and store data in sqlite database')
    parser.add_argument('--db', required=True, help='name of the database where the parsed data shall be stored')
    parser.add_argument('--outdir', default='export', help='output directory for the plots')
    args = parser.parse_args()

    options = {}
    options['log_level'] = logging.INFO
    options['db'] = args.db
    options['outdir'] = args.outdir

    logging.basicConfig(level=options['log_level'])

    logging.info('connecting to database')
    db = options["db"]
    if not os.path.exists(db):
        logging.critical('Database file %s does not exist' % (db))
        sys.exit(2)

    options['db_conn'] = sqlite3.connect(db)

    try:
        os.mkdir(options['outdir'])
    except OSError:
        pass

#exportDBToCSV(options)
#   exportDBToXML(options)
    exportDBToNED(options)

ned_head = '''package scenarios;

simple Gossip
{
    parameters:
        int id;
        int gossipType;
        double p;
        double p2;
        int k;
        int n;
        int m;
        double pkgInterval;
        int jitter;
        double timeout;
        int tagKey;
        int numberOfPackets;
    gates:
        input in[];
    output out[];
}

simple Evaluation
{
    parameters:
        int flushDataThreshold;
        string dbName;
        string sourceSet;
        int runNumber;
}

network DESTestbed
{
    parameters:
        double packetDeliveryRatio;
    types:
        channel Channel extends ned.DatarateChannel{ per = 1-packetDeliveryRatio; }
    submodules:
        eval: Evaluation {}
'''

def exportDBToNED(options):
    def write_nodes(f):
        for i, h in enumerate(sorted(hosts)):
            f.write('      %s: Gossip { @display (\"t=%s\"); id=%d; } \n' % (h.replace('-', '_'), h, i))
        f.write('    connections:\n')

    logging.info('exporting as NED')
    c = options['db_conn'].cursor()
    hosts = list(pylab.flatten(c.execute('SELECT DISTINCT(host) FROM addr ORDER BY host').fetchall()))
    tag_ids = c.execute('SELECT key, helloSize FROM tag ORDER BY helloSize').fetchall()
    for tag, helloSize in tag_ids:
        undirected_unweighted = open('./%s/uu-%d.ned' % (options['outdir'], helloSize), 'w')
        directed_unweighted  = open('./%s/du-%d.ned' % (options['outdir'], helloSize), 'w')
        directed_weighted = open('./%s/dw-%d.ned' % (options['outdir'], helloSize), 'w')
        files = [undirected_unweighted, directed_unweighted, directed_weighted]

        for f in files:
            f.write('%s' % ned_head)
            write_nodes(f)

        c.execute('''SELECT src, host, pdr FROM eval_helloPDR WHERE tag_key=?''', (tag,))
        added = list()
        for src, host, pdr in c.fetchall():
            src = src.replace('-', '_')
            host = host.replace('-', '_')
            directed_weighted.write('        %s.out++ --> Channel { per = %.3f; } --> %s.in++;\n' % (src, 1-pdr, host))
            directed_unweighted.write('      %s.out++ --> Channel --> %s.in++;\n' % (src, host))
            if (src, host) not in added and (host, src) not in added:
                undirected_unweighted.write('        %s.out++ --> Channel --> %s.in++;\n' % (src, host))
                undirected_unweighted.write('        %s.in++ <-- Channel <-- %s.out++;\n' % (src, host))
                added.append((src, host))

        for f in files:
            f.write('}\n')

ned_mixim_head = '''package gossipsimulation80211;

import org.mixim.base.connectionManager.ConnectionManager;
import org.mixim.base.modules.BaseWorldUtility;

network GossipSimulation80211
{
    parameters:
        double packetDeliveryRatio;

        double playgroundSizeX @unit(m); // x size of the area the nodes are in (in meters)
        double playgroundSizeY @unit(m); // y size of the area the nodes are in (in meters)
        double playgroundSizeZ @unit(m); // z size of the area the nodes are in (in meters)

        @display("bgb=$playgroundSizeX,$playgroundSizeY,white;bgp=0,0");
    submodules:
        connectionManager: ConnectionManager {
            parameters:
                @display("p=150,0;b=42,42,rect,green,,;i=abstract/multicast");
        }
        world: BaseWorldUtility {
            parameters:
                playgroundSizeX = playgroundSizeX;
                playgroundSizeY = playgroundSizeY;
                playgroundSizeZ = playgroundSizeZ;
                @display("p=30,0;i=misc/globe");
        }
        eval: Evaluation {}
'''

ned_mixim_tail = '''
    connections allowunconnected:
}
'''

def graphToNEDMixim(options, prefix=''):
    def write_nodes(outfile, G):
        for i, h in enumerate(sorted(G.nodes())):
            outfile.write('      %s: Gossip { @display (\"t=%s\"); id=%d; } \n' % (h, h, i))
        outfile.write('    connections:\n')

    logging.info('exporting as NED')
    for i, G in enumerate(options['graph']):
        if 'steps' in options:
            i = i*options['steps']
        outfile = open('./%s/%sgraph%03d-mixim.ned' % (options['outdir'], prefix, i), 'w')
        outfile.write('// playgound size: 1000 * 1000\n')
        outfile.write('%s' % ned_mixim_head)

        for n in G.nodes():
            outfile.write('\t\tnode%d: GossipHost80211 {\n' % n)
            outfile.write('\t\t\tgossipSimLayer.id = %d;\n' % n)
            outfile.write('\t\t\tmobility.x = %f;\n' % ((G.node[n]['pos'][0])*1000))
            outfile.write('\t\t\tmobility.y = %f;\n' % ((G.node[n]['pos'][1])*1000))
            outfile.write('\t\t\tmobility.z = 0; // ignored when use2D\n')
            outfile.write('\t\t}\n')

        outfile.write('%s' % ned_mixim_tail)
        if 'layouts' in options:
            for name, func in options['layouts']:
                plt.clf()
                func(G)
                plt.savefig('./%s/graph%03d-%s.pdf' % (options['outdir'], i, name), format='pdf')

def graphToNED(options):
    def write_nodes(outfile, G):
        for i, h in enumerate(sorted(G.nodes())):
            outfile.write('      %s: Gossip { @display (\"t=%s\"); id=%d; } \n' % (h, h, i))
        outfile.write('    connections:\n')

    logging.info('exporting as NED')
    for i, G in enumerate(options['graph']):
        outfile = open('./%s/graph%03d.ned' % (options['outdir'], i), 'w')
        outfile.write('%s' % ned_head)
        write_nodes(outfile, G)

        added = dict()
        for a, b in G.edges():
            if (a, b) not in added and (b, a) not in added:
                outfile.write('        %s.out++ --> Channel --> %s.in++;\n' % (a, b))
                outfile.write('        %s.in++ <-- Channel <-- %s.out++;\n' % (a, b))
                added[(a, b)] = True

        outfile.write('}\n')
        for name, func in options['layouts']:
            plt.clf()
            func(G)
            plt.savefig('./%s/graph%03d-%s.pdf' % (options['outdir'], i, name), format='pdf')

def graphToCSV(options):
    def write_header():
        outfile.write('# mode           : %s\n' % options['type'])

        if options['type'] == 'DES-Testbed':
            outfile.write('# db             : %s\n' % options['db'])
            outfile.write('# helloSize      : %d\n' % options['size'])
        elif options['type'] ==  'grid':
            pass
        elif options['type'] == 'random geometric':
            outfile.write('# range          : %f\n' % options['range'])
        elif options['type'] == 'random regular':
            pass
        elif options['type'] == 'random':
            pass
        else:
            logging.critical("unknown type: aborting")
            assert(False)

        outfile.write('# nodes          : %s\n' % len(G.nodes()))

        if options['type'] == 'grid':
            outfile.write('# diameter       : %d\n' % (2*(int(scipy.sqrt(len(G.nodes())))-1)))
        else:
            if G.is_directed():
                components = nx.algorithms.strongly_connected_components(G)
            else:
                components = nx.algorithms.connected_components(G)
            c = components[0]
            info = ''
            if len(components) > 1:
                info = ' (partitioned!)'
            outfile.write('# mean diameter  : %d%s\n' % (nx.algorithms.distance_measures.diameter(nx.subgraph(G, c)), info))

        if 'degree' in options and options['degree'] != None:
            outfile.write('# degree         : %f\n' % options['degree'])
        outfile.write('# mean degree    : %f\n' % scipy.mean(G.degree().values()))
        outfile.write('\n# src, dest\n')

    logging.info('exporting as CSV')
    for i, G in enumerate(options['graph']):
        outfile = open('./%s/graph%03d.csv' % (options['outdir'], i), 'w')
        write_header()
        for a, b in G.edges():
            t = sorted([str(a), str(b)])
            outfile.write('%s; %s\n' % (t[0], t[1]))

def exportDBToXML(options):
  logging.info('exporting as XML')
  c = options['db_conn'].cursor()

  c.execute('''SELECT host FROM addr''')
  hosts = [h[0] for h in c.fetchall()]
  tag_ids = c.execute('SELECT key, helloSize FROM tag ORDER BY helloSize').fetchall()
  for tag, helloSize in tag_ids:
      root = ET.Element("topology")
      net = ET.SubElement(root, 'net', description='Testbed topology measured with %d byte HELLO packets' % (helloSize), name='vm-%s' % (helloSize))
      nodeTypes = ET.SubElement(net, 'nodeTypes')
      nodeType = ET.SubElement(nodeTypes, 'nodeType', name='meshrouter')
      interfaces = ET.SubElement(nodeType, 'interfaces')
      interface = ET.SubElement(interfaces, 'interface', name='wlan0', type='802.11bg')
      nodes = ET.SubElement(net, 'nodes')
      links = ET.SubElement(net, 'links')
      for host in hosts:
          ET.SubElement(nodes, 'node', name=host, type='meshrouter')
      c.execute('''SELECT src,host,pdr FROM eval_helloPDR WHERE tag_key=?''', (tag,))
      for src,host,pdr in c.fetchall():
          ET.SubElement(links, 'link', broadcast_loss=str(1.0-pdr), from_if='wlan0', from_node=src, loss=str(1.0-pdr), to_if='wlan0', to_node=host, uni='true')
      tree = ET.ElementTree(root)
      fname = './%s/vm%s.' % (options['outdir'], str(tag))
      tree.write(fname + 'tmp', encoding='UTF-8')
      os.system('xmllint -format -recover ' + fname + 'tmp > ' + fname + 'xml')
      os.remove(fname + 'tmp')

def exportDBToCSV(options):
    logging.info('exporting as CSV')
    c = options['db_conn'].cursor()
    tag_ids = c.execute('SELECT key, helloSize FROM tag ORDER BY helloSize').fetchall()

    def write_header(f, helloSize, options):
        c = options['db_conn'].cursor()
        hello_interval = list(set(pylab.flatten(c.execute('SELECT DISTINCT(hello_interval) FROM tag').fetchall())))
        channel = list(set(pylab.flatten(c.execute('SELECT DISTINCT(channel) FROM tag').fetchall())))
        assert(len(hello_interval) == 1)
        assert(len(channel) == 1)
        times = list(pylab.flatten(c.execute('SELECT time FROM tag').fetchall()))
        durations = [times[i] - times[i-1] for i in range(1, len(times))]
        avr_duration = scipy.mean(durations)
        f.write('# DES-Testbed topology as probed with broadcast packets\n\n')
        f.write('# %-15s: %s\n' % ('Database', options['db'].split('/')[-1]))
        f.write('# %-15s: %d\n' % ('Channel', channel[0]))
        f.write('# %-15s: %d byte\n' % ('Packet size', helloSize))
        f.write('# %-15s: %d ms\n' % ('Packet interval', hello_interval[0]))
        f.write('# %-15s: %d s\n' % ('Duration', (int(avr_duration)/60)*60))

        f.write('\n# column 0: Name of transmitting node\n')
        f.write('# column 1: Name of receiving node\n')
        f.write('# column 2: Calculated packet delivery ration (PDR) after duration\n')

        f.write('\n#%9s, %10s, %10s\n' % ('SRC', 'DEST', 'PDR'))

    for tag_key, helloSize in tag_ids:
        f = open('./%s/topo-%d.csv' % (options['outdir'], helloSize), 'w')
        write_header(f, helloSize, options)
        c.execute('''SELECT src, host, pdr FROM eval_helloPDR WHERE tag_key=?''', (tag_key,))
        for src, host, pdr in c.fetchall():
            f.write('%10s, %10s, %10f\n' % (src, host, pdr))

if __name__ == "__main__":
    export_main()
