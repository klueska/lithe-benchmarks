#!/usr/bin/arch -i386 python2.7
#
# Author: Kevin Klues <klueska@cs.berkeley.edu>

import os
import sys
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
    self.files = glob.glob(config.input_folder + '/*-results-*')
    self.data = {}
    map(lambda f: FileData(f, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    m = re.match('(?P<label>.*)-results-(?P<iteration>.*)', os.path.basename(f))
    label = m.group('label')
    iteration = int(m.group('iteration'))

    i = 0
    lines = map(lambda x: x.strip(), file(f).readlines())
    comment_line = '#################################################################'
    while (i < len(lines)):
      while (i < len(lines) and lines[i] == comment_line):
        i += 1
      if (i >= len(lines)):
        break
      if (lines[i][:8] == 'matrices'):
        data = TestData(lines[i:i+12])
        bdata.data.setdefault(data.matrix, {})
        bdata.data[data.matrix].setdefault(label, {})
        bdata.data[data.matrix][label].setdefault(data.omp_threads, {})
        bdata.data[data.matrix][label][data.omp_threads].setdefault(data.tbb_threads, {})
        bdata.data[data.matrix][label][data.omp_threads][data.tbb_threads].setdefault(iteration, {})
        bdata.data[data.matrix][label][data.omp_threads][data.tbb_threads][iteration] = data.real_time
      i += 1

class TestData:
  def __init__(self, lines):
    self.matrix = lines[0].split('/')[2][:-4]
    self.omp_threads = int(lines[1].split(':')[1])
    self.tbb_threads = int(lines[2].split(':')[1])
    self.real_time = self.getSeconds(lines[9].split('\t')[1])
    self.user_time = self.getSeconds(lines[10].split('\t')[1])
    self.sys_time = self.getSeconds(lines[11].split('\t')[1])

  def getSeconds(self, time):
    m, s = time[:-1].split('m')
    return (60 * float(m)) + float(s)

def specific_results(mdata, omp, tbb):
  results = {}
  for lib in mdata.keys():
    if (omp in mdata[lib] and tbb in mdata[lib][omp]):
      avg = np.mean(mdata[lib][omp][tbb].values())
      var = np.std(mdata[lib][omp][tbb].values())
      results[lib] = [omp, tbb, avg, var]
  return results

def best_results(mdata):
  results = {}
  for lib in mdata.keys():
    best = [0, 0, sys.float_info.max]
    for omp in mdata[lib].keys():
      for tbb in mdata[lib][omp].keys():
        avg = np.mean(mdata[lib][omp][tbb].values())
        var = np.std(mdata[lib][omp][tbb].values())
        if avg < best[2]:
          best = [omp, tbb, avg, var]
    results[lib] = best
  return results

def graph_results(bdata, config):
  margin = 1
  width = 2
  colors = ['#396AB1', '#CC2529', '#3E9651', 'yellow']
  matrices = ['landmark', 'deltaX', 'ESOC', 'Rucci1']

  results = OrderedDict()
  for m in matrices:
    results.setdefault(m, {})
    results[m]['Manually Tuned'] = best_results(bdata.data[m])
    results[m]['Serial'] = specific_results(bdata.data[m], 1, 1)
    results[m]['Out of the Box'] = specific_results(bdata.data[m], 16, 16)

  gbs = []
  glabels = []
  for i, m in enumerate(results.keys()):
    bs = []
    labels = []
    subplot(1, len(results.keys()), i + 1)

    j = 0
    r = results[m]['Out of the Box']['native']
    labels.append( "Out of the Box - native")
    b = bar(margin + j*width, r[2], width, label=labels[-1], color=colors[j])
    e = plt.errorbar(margin + j*width + width/2, r[2], yerr=r[3], fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
    bs.append(b)

    j += 1
    r = results[m]['Manually Tuned']['native']
    labels.append( "Manually Tuned - native")
    top_label = "OMP=%d, TBB=%d" % (r[0], r[1])
    b = bar(margin + j*width, r[2], width, label=labels[-1], color=colors[j])
    e = plt.errorbar(margin + j*width + width/2, r[2], yerr=r[3], fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
    bs.append(b)

    j += 1
    r = results[m]['Out of the Box']['upthread']
    labels.append( "Out of the Box - upthread")
    top_label = "OMP=%d, TBB=%d" % (r[0], r[1])
    b = bar(margin + j*width, r[2], width, label=labels[-1], color=colors[j])
    e = plt.errorbar(margin + j*width + width/2, r[2], yerr=r[3], fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
    bs.append(b)

    j += 1
    r = results[m]['Manually Tuned']['lithe']
    labels.append("Lithe")
    b = bar(margin + j*width, r[2], width, label=labels[-1], color=colors[j])
    e = plt.errorbar(margin + j*width + width/2, r[2], yerr=r[3], fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
    bs.append(b)

    if i == 0:
      gbs = bs
      glabels = labels
      ylabel('Time (sec)', fontsize=16)

    plotwidth = (j + 1)*width + 2*margin
    xlim([0, plotwidth])
    xticks([plotwidth/2.0], [m], fontsize=16)

  figlegend(bs, labels, loc='center', bbox_to_anchor=(0.023, 0.60, 1, 1), ncol=1, labelspacing=0.5)
  suptitle("Performance of SPQR with Lithe", y=1.30, x=0.523, fontsize=20)

  tight_layout(pad=0.8)
  figname = config.output_folder + "/spqr-results.png"
  savefig(figname, bbox_inches="tight")
  clf()

def spqr_graphs(parser, args):
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
  graph_results(bdata, config)

