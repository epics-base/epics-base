#!/usr/bin/env python

from __future__ import print_function

import sys
import os
from collections import OrderedDict # used as OrderedSet

from argparse import ArgumentParser

if os.environ.get('EPICS_DEBUG_RPATH','')=='YES':
    sys.stderr.write('%s'%sys.argv)

P = ArgumentParser(description='''Compute and output -rpath entries for each of the given paths.
  Paths under --root will be computed as relative to --final .''',
epilog='''
eg. A library to be placed in /build/lib and linked against libraries in
'/build/lib', '/build/module/lib', and '/other/lib' would pass:

 "makeRPath.py -F /build/lib -R /build /build/lib /build/module/lib /other/lib"
which prints "-Wl,-rpath,$ORIGIN/. -Wl,-rpath,$ORIGIN/../module/lib -Wl,-rpath,/other/lib"
''')
P.add_argument('-F','--final',default=os.getcwd(), help='Final install location for ELF file')
P.add_argument('-R','--root',default='', help='Root(s) of relocatable tree.  Separate with :')
P.add_argument('-O', '--origin', default='$ORIGIN')
P.add_argument('path', nargs='*')
args = P.parse_args()

# eg.
# target to be installed as: /build/bin/blah
#
# post-install will copy as:  /install/bin/blah
#
# Need to link against:
#   /install/lib/libA.so
#   /build/lib/libB.so
#   /other/lib/libC.so
#
# Want final result to be:
#  -rpath $ORIGIN/../lib -rpath /other/lib \
#  -rpath-link /build/lib -rpath-link /install/lib

fdir = os.path.abspath(args.final)
roots = [os.path.abspath(root) for root in args.root.split(':') if len(root)]

# find the root which contains the final location
froot = None
for root in roots:
    frel = os.path.relpath(fdir, root)
    if not frel.startswith('..'):
        # final dir is under this root
        froot = root
        break

if froot is None:
    sys.stderr.write("makeRPath: Final location %s\nNot under any of: %s\n"%(fdir, roots))
    sys.exit(1)

output = OrderedDict()
for path in args.path:
    path = os.path.abspath(path)

    for root in roots:
        rrel = os.path.relpath(path, root)
        if not rrel.startswith('..'):
            # path is under this root

            # some older binutils don't seem to handle $ORIGIN correctly
            # when locating dependencies of libraries.  So also provide
            # the absolute path for internal use by 'ld' only.
            output['-Wl,-rpath-link,'+path] = True

            # frel is final location relative to enclosing root
            # rrel is target location relative to enclosing root
            path = os.path.relpath(rrel, frel)
            break

    output['-Wl,-rpath,'+os.path.join(args.origin, path)] = True

print(' '.join(output))
