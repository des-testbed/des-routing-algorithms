#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sqlite3
import logging
import argparse

def main():
    parser = argparse.ArgumentParser(description='Create indexes in a sqlite database')
    parser.add_argument('--db', default='./sqlite.db', help='name of the database where the parsed data shall be stored')
    parser.add_argument('--add', '-a', nargs='?', const='True', default='False', help='add only missing indexes')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format='%(levelname)s [%(funcName)s] %(message)s')

    logging.info('connecting to database')
    conn = sqlite3.connect(args.db)
    create_indexes(conn, args.add)

def create_indexes(conn, add=False):
    print 'creating indexes'
    c = conn.cursor()
    logging.info('creating indexes')
    logging.info('(1/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_he_host_tag \
            ON he (host, tag) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(2/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_rh_host \
            ON rh (host) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(3/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_rh_src_seq \
            ON rh (src, seq) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(4/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_rh_prev_seq \
            ON rh (prev, seq) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(5/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_tx_host_tag \
            ON tx (host, tag) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(6/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_rx_host \
            ON rx (host) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise
    logging.info('(7/7)')
    try:
        c.execute(''' \
            CREATE INDEX IF NOT EXISTS id_rx_src_seq \
            ON rx (src, seq) \
        ''')
    except sqlite3.OperationalError:
        if not add:
            raise

    conn.commit()

if __name__ == "__main__":
  main()
