#!/usr/bin/python

from helpers import MyFig, get_options, convex_hull
import random
import math
import numpy
from matplotlib.patches import Polygon
from matplotlib.collections import PatchCollection

def point_inside_polygon((x,y), poly):
    '''
    determine if a point is inside a given polygon or not
    Polygon is a list of (x,y) pairs.

    source: http://www.ariel.com.au/a/python-point-int-poly.html
    '''
    n = len(poly)
    inside =False

    p1x,p1y = poly[0]
    for i in range(n+1):
        p2x,p2y = poly[i % n]
        if y > min(p1y,p2y):
            if y <= max(p1y,p2y):
                if x <= max(p1x,p2x):
                    if p1y != p2y:
                        xinters = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x
                    if p1x == p2x or x <= xinters:
                        inside = not inside
        p1x,p1y = p2x,p2y

    return inside

def distance(p1, p2):
    return math.sqrt((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2)

def get_mindist_point(H, N):
    min_p = None
    for n in N:
        dists = sorted([(distance(n, h), n) for h in H])
        if not min_p or dists[0][0] < min_p[0]:
            min_p = dists[0]
    return min_p[1]

def get_mindist_point2(p, points):
    dists = sorted([(distance(p, p2), p2) for p2 in points])
    return dists[0][1]

def get_points(num = 80):
    #return zip([random.random() for x in xrange(num)], [random.random() for x in xrange(num)])
    return zip([random.normalvariate(0.5, 0.1) for x in xrange(num)], [random.normalvariate(0.5, 0.1) for x in xrange(num)])

#def get_distance2line(((x1, y1), (x2, y2)), (x3, y3)):
    #px = x2-x1
    #py = y2-y1

    #a = px*px + py*py
    #u =  ((x3 - x1) * px + (y3 - y1) * py) / a

    #if u > 1:
        #u = 1
    #elif u < 0:
        #u = 0

    #x = x1 + u * px
    #y = y1 + u * py

    #return distance((x3, y3), (x, y))

def get_distance2line(((x1, y1), (x2, y2)), (x3, y3)):
    a = (x1, y1)
    b = (x2, y2)
    p = (x3, y3)
    dX = b[0] - a[0]
    dY = b[1] - a[1]
    segmentLen = math.sqrt(dX * dX + dY * dY)
    halfLen = segmentLen/2

    pX = p[0] - a[0]
    pY = p[1] - a[1]
    if segmentLen < 1E-6:
        return math.sqrt( pX * pX + pY * pY )

    newX = abs((pX * dX + pY * dY)/segmentLen - halfLen)
    newY = abs(-pX * dY + pY * dX)/segmentLen

    if newX > halfLen:
        newX = newX - halfLen
        return math.sqrt( newX * newX + newY * newY)
    else:
        return newY

def get_min_dist_line(p, edges):
    dists = sorted([(get_distance2line(e, p), e) for e in edges])
    return dists[0][1]

def main():
    points = get_points()
    options = get_options()
    options['outdir'] = '/tmp/'
    options['prefix'] = 'concave'
    concave, convex = get_concave(points)
    draw(options, concave, points, convex)

def get_concave(points):
    convex = list(convex_hull(numpy.array(points).transpose()))
    convex = [(x, y) for x, y in convex]
    concave = calc_concave2(points, convex)
    return concave, convex

def edges2list(edges):
    concave = list()
    cur_edge = None
    while len(edges):
        if not cur_edge:
            cur_edge = edges.pop()
        else:
            matches = [(_p1, _p2) for _p1, _p2 in edges if _p1 == cur_edge[1]]
            try:
                cur_edge = matches[0]
            except IndexError:
                print cur_edge
                print sorted(edges)
                raise
            edges.remove(cur_edge)
        concave.append(cur_edge[0])
        concave.append(cur_edge[1])
    return concave

def calc_concave2(points, convex, threshold=2):
    N = [p for p in points if p not in convex]
    concave = list(convex)
    assert(len(concave) + len(N) == len(points))
    edges = [(p1, concave[(i+1)%len(concave)]) for i, p1 in enumerate(concave)]
    assert(len(edges) == len(concave))
    for j, (p1, p2) in enumerate(edges):
        if len(N) <= 0:
            print 'N'
            break
        dists1 = sorted([(get_distance2line((p1, p2), p), p) for p in N])

        if True:
            dists = ([(d, p) for d, p in dists1 if get_min_dist_line(p, edges) == (p1, p2)])
            if len(dists) == 0:
                continue
            dist, p0 = dists[0]
        else:
            p0 = None
            for d, p in dists1:
                dists2 = sorted([(get_distance2line((p1e, p2e), p), p) for p1e, p2e in edges if p1e != p1 and p2e != p2])
                if d < dists2[0][0]:
                    p0 = p
                    break
            if not p0:
                continue
        len_edge = distance(p1, p2)
        decision = min(distance(p1, p0), distance(p2, p0))
        if len_edge/float(decision) > threshold:
            N.remove(p0)
            edges.append((p1, p0))
            edges.append((p0, p2))
            edges.remove((p1, p2))
    return edges2list(edges)

def draw(options, H, points, convex=None):
    fig = MyFig(options, figsize=(10,8), xlabel='', ylabel='', legend=False, grid=False, aspect='auto')
    poly = Polygon(H, edgecolor='red', facecolor='red', closed=True, alpha=0.3)
    patch_collection = PatchCollection([poly], match_original=True)
    patch_collection.zorder = -2
    fig.ax.add_collection(patch_collection)

    for x, y in H:
        fig.ax.plot(x, y, marker='o', color='red')

    for x, y in [p for p in points if p not in H]:
        fig.ax.plot(x, y, marker='o', color='black')

    if convex != None:
        poly = Polygon(convex, edgecolor='blue', facecolor='none', closed=True, alpha=0.3)
        patch_collection = PatchCollection([poly], match_original=True)
        patch_collection.zorder = -2
        fig.ax.add_collection(patch_collection)

    fig.ax.axis((0, 1, 0, 1))
    fig.save('test')

if __name__ == '__main__':
    main()