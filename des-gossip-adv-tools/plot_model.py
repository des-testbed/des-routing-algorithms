#!/usr/bin/python
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import scipy
import pylab
import numpy
import argparse
from helpers import MyFig, get_options

#T  = 15   # deadline [s]
#qd = 0.7  # packet loss independent of tau
#m = 5     # threshold for the queue length l; l>m => faulty jobs
#ES = 0.05 # expexted network sojourn time
#N = 50    # nodes performing restart

def fakultaet(x):
    if x > 1:
        return x * fakultaet(x - 1)
    else:
        return 1

def p(tau, options):
    return options['nodes']/tau * options['ES']

def pi(i, tau, options):
    return float(scipy.e**(-p(tau, options)))/fakultaet(i) * p(tau, options)**i

def qo(tau, options):
    '''
    packet loss due to overload
    '''
    return 1-sum([pi(i, tau, options) for i in range(0, options['threshold'])])

def qr(tau, options):
    '''
    reachability after T with restart interval tau
    '''
    return (1-qo(tau, options))*(1-options['qd'])

def qs(tau, options):
    '''
    probability that at least one packet arrives
    '''
    return 1-(1-qr(tau, options))**(options['deadline']/tau)

def qe(tau, options):
    '''
    probability that exactly one packet arrives
    '''
    return options['deadline']/tau * qr(tau, options)*(1-qr(tau, options))**(options['deadline']/tau -1)

def plot(options):
    fig = MyFig(options, figsize=(10, 8), xlabel=r'Timeout $\tau$ [s]', ylabel=r'Reachability $\reachability$', aspect='auto', legend=True, grid=False)

    taus = numpy.arange(0, options['max_tau'], 0.01)

    Rqs = list()
    for tau in taus:
        Rqs.append(qs(tau, options))

    Rqe = list()
    for tau in taus:
        Rqe.append(qe(tau, options))

    fig.ax.plot(taus, Rqs, label='$q_s$ (at least one)', color='blue')
    fig.ax.plot(taus, Rqe, label='$q_e$ (exactly one)', color='red')
    fig.ax.set_ylim(0,1.1)
    fig.save('model-N=%d-qd=%.2f-ES=%.2f-m=%d-T=%.2f' % (options['nodes'], options['qd'], options['ES'], options['threshold'], options['deadline']))

def main():
    parser = argparse.ArgumentParser(description='Plot data stored in an sqlite database')
    parser.add_argument('--outdir', default='./', help='output directory for the plots')
    parser.add_argument('--fontsize', default=matplotlib.rcParams['font.size'], type=int, help='base font size of plots')
    parser.add_argument('--nodes', '-N', default=50, type=int, help='nodes thay apply restart')
    parser.add_argument('--qd', default=0.7, type=float, help='loss independent of tau')
    parser.add_argument('--ES', default=0.05, type=float, help='expexted network sojourn time')
    parser.add_argument('--deadline', '-T', default=15, type=float, help='deadline')
    parser.add_argument('--threshold', '-m', default=5, type=int, help='threshold for jobs in the queue')
    parser.add_argument('--max_tau', default=10, type=float, help='maximum tau (bounded by T)')
    args = parser.parse_args()

    options = get_options()
    options['prefix'] = 'restart'
    options['outdir'] = args.outdir
    options['fontsize'] = args.fontsize
    options['nodes'] = args.nodes
    options['qd'] = args.qd
    options['ES'] = args.ES
    options['deadline'] = args.deadline
    options['threshold'] = args.threshold
    options['max_tau'] = min(args.max_tau, args.deadline)

    plot(options)

if __name__ == '__main__':
    main()
