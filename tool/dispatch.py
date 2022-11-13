#!/usr/bin/env python3

import os
import sys

DISPATCH = '''\
    /*000*/ OpAlubAdd,
    /*001*/ OpAluw,
    /*002*/ OpAlubFlipAdd,
etc...
'''.split('\n')

COUNT = [0 for _ in range(0x500)]

for arg in sys.argv[1:]:
  with open(arg) as f:
    for line in f:
      line = line.strip()
      if not line: continue
      count, op = line.split()
      count = int(count)
      op = int(op)
      COUNT[op] += count

toto = sum(COUNT)
S = []
for op in range(len(DISPATCH)):
  S.append((COUNT[op], op))
S.sort(reverse=True)
RANK = {}
for i, (_, op) in enumerate(S):
  RANK[op] = i + 1
REPORT = []

for op in range(len(DISPATCH)):
  if COUNT[op]:
    print("%-40s // #%-4d (%.6f%%)" % (DISPATCH[op], RANK[op], COUNT[op] / float(toto) * 100))
  else:
    print("%-40s //" % (DISPATCH[op]))
