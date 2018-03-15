#!/usr/bin/env python

from __future__ import print_function

import sys
import os

from argparse import ArgumentParser

if os.environ.get('EPICS_DEBUG_RPATH','')=='YES':
    sys.stderr.write('%s'%sys.argv)

P = ArgumentParser()
P.add_argument('-F','--final',default=os.getcwd(), help='Final install location for ELF file')
P.add_argument('-R','--root',default='/')
P.add_argument('-O', '--origin', default='$ORIGIN')
P.add_argument('path', nargs='*')
args = P.parse_args()

fdir = os.path.abspath(args.final)

output = []
for path in args.path:
    path = os.path.abspath(path)

    if args.root and os.path.relpath(path, args.root).startswith('../'):
        pass # absolute rpath
    else:
        path = os.path.relpath(path, fdir)

    output.append('-Wl,-rpath,'+os.path.join(args.origin, path))

print(' '.join(output))
