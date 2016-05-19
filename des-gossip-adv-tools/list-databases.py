#!/usr/bin/python

import os
import sqlite3
import logging

logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')
databases = os.listdir('./db/')
databases = sorted(['./db/'+db for db in databases if db.endswith('.db')])

def eval_scenarios(options):
    """
    Returns information in a dictionary that is important for some plotting functions.
    All tags in the tag table are evaluated.
    """
    cursor = options['db_conn'].cursor()
    parameters = cursor.execute('''
        PRAGMA TABLE_INFO(view_configurations)
    ''').fetchall()
    parameters = [name for _num, name, _type, _a, _b, _c in parameters]
    values = {}
    for param in parameters:
        values[param] = set()

    configurations = cursor.execute('''
        SELECT DISTINCT * \
        FROM view_configurations
    ''').fetchall()

    tx_sources = cursor.execute('''
        SELECT DISTINCT host \
        FROM tx;
    ''').fetchall()

    num_tags, = cursor.execute('''
        SELECT COUNT(key) \
        FROM tag;
    ''').fetchone()

    num_nodes, = cursor.execute('''
        SELECT COUNT(host) \
        FROM addr;
    ''').fetchone()

    tx_start, tx_end = cursor.execute('''
        SELECT MIN(time), MAX(time) \
        FROM tx;
    ''').fetchone()

    if not tx_start:
        tx_start, tx_end = cursor.execute('''
        SELECT MIN(time), MAX(time) \
        FROM he;
    ''').fetchone()

    for config in configurations:
        for i, value in enumerate(config):
            values[parameters[i]].add(value)

    options['configurations'] = values

    logging.info('experiment started %s and ended %s', tx_start, tx_end)
    logging.info('%d different configurations found in tags table', len(configurations))
    logging.info('%d tag keys in tags table', num_tags)
    if len(configurations)*len(tx_sources) != num_tags \
        and len(tx_sources) and not 2 in values['gossip']:
        logging.warning('Inspect database for legacy or incomplete information!!!')
    logging.info('Values of the parameters:')
    logging.info('\t\tnodes:          %d', num_nodes)
    logging.info('\t\tsources:        %s', [str(src) for src, in tx_sources])
    for key in sorted(values.keys()):
        if len(values[key]) > 1 or key == 'gossip':
            tabs = ''.join(' ' for x in range(0, 15-len(key)))
            if key in ['p', 'pkg_interval']:
                logging.info('\t\t%s:%s[%s]', key, tabs, ', '.join('%.2f' % v for v in sorted(values[key])))
            else:
                logging.info('\t\t%s:%s%s', key, tabs, sorted([v for v in values[key]]))


for db in databases:
    logging.info('database %s', db)
    conn = sqlite3.connect(db)
    options = {}
    options['db_conn'] = conn
    eval_scenarios(options)
    print('')