#!/usr/bin/arch -i386 python2.7
#
# Author: Kevin Klues <klueska@cs.berkeley.edu>

import os
import re
import glob
import json
import matplotlib
import matplotlib as mpl
matplotlib.use('Agg')
from pylab import *
from collections import OrderedDict
import numpy as np

class BenchmarkData:
  def __init__(self, config):
    self.files = map(lambda x: os.path.join(config.input_folder, x), next(os.walk(config.input_folder))[2])
    self.data = {}
    map(lambda x: FileData(x, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    mainpat = re.compile('^main-output$')
    thumbpat = re.compile('^thumbnails-(?P<lib>.*)-(?P<num>.*)$')
    blastpat = re.compile('^blast-(?P<lib>.*)$')

    filename = os.path.basename(f)
    lines = map(lambda x: x.strip(), file(f).readlines())

    m = mainpat.match(filename)
    if m != None:
      bdata.data['main'] = ParseMainData(lines, bdata).data
      return
    m = thumbpat.match(filename)
    if m != None:
      lib = m.group('lib')
      td = ParseThroughputData(lines, bdata).data
      bdata.data.setdefault('thumbnails', {})
      bdata.data['thumbnails'].setdefault(lib, [])
      bdata.data['thumbnails'][lib].append(td)
      return
    m = blastpat.match(filename)
    if m != None:
      lib = m.group('lib')
      td = ParseThroughputData(lines, bdata).data
      bdata.data.setdefault('blast', {})
      bdata.data['blast'].setdefault(lib, [])
      bdata.data['blast'][lib].append(td)
      return

class ParseMainData:
  def __init__(self, lines, bdata):
    self.data = OrderedDict()
    for line in lines:
      if line == "":
        continue
      if line[0:15] == 'Configuration: ':
        config = line[15:]
        self.data[config] = {}
      else:
        name,time = map(lambda x: x.strip(), line.split(':'))
        a,t = map(lambda x: x.strip(), name.split('_'))
        self.data[config].setdefault(a, {})
        self.data[config][a].setdefault(t, [])
        self.data[config][a][t].append(float(time))
        if 'starttime' not in self.data[config]:
          self.data[config]['starttime'] = float(time)

class ParseThroughputData:
  def __init__(self, lines, bdata):
    self.data = []
    for i, line in enumerate(lines):
      sl = map(lambda x: x.strip(), line.split(','))
      self.data.append({
        'throughput' :     float(sl[0].split(' ')[0]),
        'avg latency' :    float(sl[1].split(' ')[0]),
        'std latency' :    float(sl[2].split(' ')[0]),
        'total requests' : float(sl[3].split(' ')[0]),
        'interval time' :  float(sl[4].split(' ')[0]),
        'total time' :     float(sl[5].split(' ')[0])
      })

def graph_throughput(bdata, config):
  fig, ax1 = plt.subplots()
  fig.subplots_adjust(hspace=0.3)
  colors = ["#396AB1", "#CC2529", "#3E9651", "#948B3D",
            "#DA7C30", "#535154", "#922428"]

  labels = OrderedDict([
    ('linux', 'Linux NPTL'),
    ('upthread', 'upthread'),
    ('upthread-lithe', 'upthread-lithe'),
    #('upthread-native-omp', 'upthread with NPTL-based OMP'),
    #('upthread-lithe-native-omp', 'upthread-lithe with NPTL-based OMP'),
  ])

  for i, lib in enumerate(labels.keys()):
    starttime = bdata.data['main'][lib]['starttime']
    for j, source in enumerate(['thumbnails', 'blast']):
      launches = bdata.data['main'][lib]['launch'][source]
      time = []
      throughput = []
      latency = []
      for k, data in enumerate(bdata.data[source][lib]):
        newtime = map(lambda x: x['total time'] + launches[k] - starttime, data)
        time += [newtime[0]] + newtime + [newtime[-1]]
        throughput += [0] + map(lambda x: x['throughput'], data) + [0]
        latency += [0] + map(lambda x: x['avg latency'], data) + [0]
      ax = subplot(4, 1, j + 1)
      plot(time, throughput, label=labels[lib], color=colors[i], linewidth=1.2)
      for label in ax.yaxis.get_ticklabels()[::2]:
        label.set_visible(False)
      xlim([0, 1600])
      ax = subplot(4, 1, 2 + j + 1)
      plot(time, latency, label=labels[lib], color=colors[i], linewidth=1.2)
      for label in ax.yaxis.get_ticklabels()[::2]:
        label.set_visible(False)
      xlim([0, 1600])

  suptitle('Kweb Throughput and Latency\nMixing File Requests and Thumbnail Generation', y=1.15, fontsize=20)
  ax = subplot(4, 1, 1)
  title('Thumbnail Request Throughput')
  ylabel('Throughput\n(Requests / Second)', labelpad=25, fontsize=16)
  ax.yaxis.set_label_coords(-0.13, -0.025)
  xx, locs = xticks()
  xticks(xx, [])
  leg = legend(loc='center', bbox_to_anchor=[0.5, 1.55, 0, 0], ncol=3)
  for legobj in leg.legendHandles:
    legobj.set_linewidth(10.0)

  subplot(4, 1, 2)
  title('File Request Throughput')
  xx, locs = xticks()
  xticks(xx, [])

  ax = subplot(4, 1, 3)
  title('Thumbnail Request Latency')
  ylabel('Latency (s)', fontsize=16)
  ax.yaxis.set_label_coords(-0.13, -0.025)
  xx, locs = xticks()
  xticks(xx, [])
  yy, locs = yticks()
  ll = ['%.2f' % a for a in yy]
  yticks(yy, ll)

  subplot(4, 1, 4)
  title('File Request Latency')
  xlabel('Time (s)', fontsize=16)

  figname = config.output_folder + "/thumbnails-throughput.png"
  savefig(figname, bbox_inches="tight")
  clf()

def thumbnails_graphs(parser, args):
  config = lambda:None
  if args.config_file:
    config.__dict__ = json.load(file(args.config_file), object_pairs_hook=OrderedDict)
  else:
    parser.print_help()
    exit(1)

  try:
    os.mkdir(config.output_folder)
  except:
    pass
  bdata = BenchmarkData(config)
  graph_throughput(bdata, config)

