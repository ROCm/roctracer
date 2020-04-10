#!/usr/bin/python

#Copyright (c) 2015-present Advanced Micro Devices, Inc. All rights reserved.
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.

import sys, os, re
import filecmp
import argparse

events_count = {}
events_order = {}
trace2info = {}
trace2info_filename = 'test/tests_trace_cmp_levels.txt'

def parse_trace_levels(filename):
    f = open(filename)
    trace2info = {}
    for line in f:
        count_once = 0
        lis = line.split(' ')
        trace_name = lis[0]
        comp_level = lis[1]
        no_events_cnt = ''
        events2ignore = ''
        for l in lis:
          if no_events_cnt == ' ':
            no_events_cnt = l 
          if events2ignore == ' ':
            events2ignore = l 
          if l == '-no-event-count':
            no_events_cnt = ' '
          if l == '-ignore-event':
            events2ignore = ' '
          if l.rstrip('\n') == '-count_once':
            count_once = 1
        trace2info[trace_name] = (eval(comp_level),no_events_cnt,events2ignore,count_once)
    return trace2info

# check trace againt golden reference and returns 0 for match, 1 for mismatch
def check_trace_status(tracename,verbose):
  trace2info = parse_trace_levels(trace2info_filename)

  trace = 'test/' + tracename + '.txt'
  rtrace = tracename + '.txt'
  if os.path.basename(tracename) in trace2info.keys():
    (trace_level,no_events_cnt,events2ignore,count_once) = trace2info[os.path.basename(tracename)]
    print 'Trace comparison for ' + os.path.basename(tracename) + ' at level ' + str(trace_level) + ' with no_events_cnt regex ' + no_events_cnt + ' and events_to_ignore list ' + events2ignore + ' and count_once ' + str(count_once)
  else:
    print 'Trace ' + os.path.basename(tracename) + ' not found in ' + trace2info_filename + ', defaulting to level 0 i.e. no trace comparison'
    return 1

  if trace_level == 1:
    cnt_r = gen_events_info(rtrace,'cnt',no_events_cnt,events2ignore,verbose,count_once)
    cnt = gen_events_info(trace,'cnt',no_events_cnt,events2ignore,verbose,count_once)
    if cnt_r == cnt:
      if verbose:
        print 'PASSED!'
      return 0
    else:
      if verbose:
        print 'FAILED!'
      return 1
  elif trace_level == 2:
    cnt_r = gen_events_info(rtrace,'or',no_events_cnt,events2ignore,verbose,count_once)
    cnt = gen_events_info(trace,'or',no_events_cnt,events2ignore,verbose,count_once)
    if cnt_r == cnt:
      if verbose:
        print 'PASSED!'
      return 0
    else:
      if verbose:
        print 'FAILED!'
      return 1
  elif trace_level == 3:
    if filecmp.cmp(trace,rtrace):
      if verbose:
        print 'PASSED!'
      return 0
    else:
      print 'FAILED!'
      os.system('/usr/bin/diff --brief ' + trace + ' ' + rtrace)
      return 1

#Parses roctracer trace file for regression purpose
#and generates events count per event (when cnt is on) or events order per tid (when order is on)
def gen_events_info(tracefile, metric,no_events_cnt,events2ignore,verbose,count_once):
  events_count = {}
  events_order = {}
  res=''
  with open(tracefile) as f:
    for line in f:
      event_pattern_s = re.compile(r'# START \((\d+)\) #############################')
      ms = event_pattern_s.match(line)
      if ms:
        start_id = ms.group(1)
        continue
      event_pattern = re.compile(r'.*<(\w+)\s+id\(\d+\)\s+.*tid\((\d+)\)>')
      # event_pattern extracts event(grp1) and tid (grp2) from a line like this:
      # <hsaKmtGetVersion id(2) correlation_id(0) on-enter pid(26224) tid(26224)>
      m = event_pattern.match(line)
      if m:
        event = m.group(1)
        tid = m.group(2)
      event_pattern2 = re.compile(r'\d+:\d+\s+\d+:(\d+)\s+(\w+)')
      # event_pattern2 extracts tid (grp1) and event (grp2) from a line like this:
      # 1822810364769411:1822810364771941 116477:116477 hsa_agent_get_info(<agent 0x8990e0>, 17, 0x7ffeac015fec) = 0
      m2 = event_pattern2.match(line)
      if m2:
        event = m2.group(2)
        tid = m2.group(1)
      event_pattern3 = re.compile(r'<rocTX "(.*)">')
      # event_pattern2 extracts rocTX event like:
      # <rocTX "before hipLaunchKernel">
      # <rocTX "hipLaunchKernel">
      m3 = event_pattern3.match(line)
      if m3:
        event = m3.group(1)
        tid = start_id
      if metric == 'cnt' and (m or m2 or m3) and event not in events2ignore:
        if event in events_count:
          events_count[event] = events_count[event] + 1
        else:
          events_count[event] = 1
      if metric == 'or' and (m or m2 or m3) and event not in events2ignore:
        if tid in events_order.keys():
          if count_once == 1:
            if event != events_order[tid][-1]: #Add event only if it is not last event in the list
              events_order[tid].append(event)
          else:
            events_order[tid].append(event)
        else:
          events_order[tid] = [event]
  if metric == 'cnt':
    for event,count in events_count.items():
      re_genre = r'{}'.format(no_events_cnt)
      if re.search(re_genre,event): # or no_events_cnt == "N/A": #'hsa_agent.*|hsa_amd.*|hsa_signal.*|hsa_sys.*'
        res = res + event + '\n'
      else:
        res = res + event + " : count " + str(count) + '\n'
  if metric == 'or':
    for tid in sorted (events_order.keys()) :
      res = res + str(events_order[tid])
  if verbose:
    print res
  return res


parser = argparse.ArgumentParser(description='check_trace.py: check a trace aainst golden ref. Returns 0 for success, 1 for failure')
requiredNamed = parser.add_argument_group('Required arguments')
requiredNamed.add_argument('-in', metavar='file', help='Name of trace to be checked', required=True)
requiredNamed.add_argument('-v', action='store_true', help='debug info', required=False)
args = vars(parser.parse_args())

if __name__ == '__main__':
    sys.exit(check_trace_status(args['in'],args['v']))

