#!/usr/bin/env python3

import logging
import sys
import re
from io import StringIO

_log = logging.getLogger(__name__)

def main(args):
    logging.basicConfig(level=args.level)

    if args.dev is True:
        actions=[
            ('DEVELOPMENT_FLAG', '1'),
            ('DEV_SNAPSHOT', '-DEV'),
        ]

    elif args.dev is False:
        actions=[
            ('DEVELOPMENT_FLAG', '0'),
            ('DEV_SNAPSHOT', ''),
        ]

    elif args.version:
        M=re.match(r'R?(\d+).(\d+).(\d+)(?:.(\d+))?(-.*)?', args.version)
        actions=[
            ('SITE_VERSION', None),
            ('SHORT_VERSION', None),

            ('MINOR_VERSION', M[2]),
            ('REVISION', M[2]),

            ('MODIFICATION', M[3]),
            ('MAINTENANCE_VERSION', M[3]),

            ('PATCH_LEVEL', M[4] or '0'),

            ('DEVELOPMENT_FLAG', '1' if (M[5] or '').upper().endswith('-DEV') else '0'),
            ('DEV_SNAPSHOT', M[5] or ''),

            ('MAJOR_VERSION', M[1]),
            ('VERSION', M[1]), # plain _VERSION must be last to resolve ambiguity
        ]

    elif args.dry_run:
        _log.debug('Print existing')
        for fname in args.conf:
            print('# ', fname)
            with open(fname, 'r') as F:
                sys.stdout.write(F.read())
        return

    else:
        print('One of --version, --release, --dev, or --dry-run is required')
        sys.exit(1)

    for name, val in actions:
        _log.debug('Pattern "%s" -> "%s"', name, val)

    for fname in args.conf:
        OUT=StringIO()

        with open(fname, 'r') as F:
            for line in F:
                _log.debug('Line: %s', repr(line))
                for name, val in actions:
                    M = re.match(r'(\s*[A-Z_]+' + name + r'\s*=[\t ]*)(\S*)(\s*)', line)
                    if M and val is None:
                        _log.debug('Ignore')
                        OUT.write(line)
                        break
                    elif M:
                        _log.debug('  Match %s -> %s', M.re.pattern, M.groups())
                        OUT.write(M[1]+val+M[3])
                        break
                else:
                    _log.debug('No match')
                    OUT.write(line)

        if args.dry_run:
            print('# ', fname)
            print(OUT.getvalue())
        else:
            with open(fname, 'w') as F:
                F.write(OUT.getvalue())

def getargs():
    from argparse import ArgumentParser
    P = ArgumentParser()
    P.add_argument('-n','--dry-run', action='store_true', default=False)
    P.add_argument('-d','--debug', dest='level', action='store_const',
                   const=logging.DEBUG, default=logging.INFO)
    P.add_argument('-V', '--version', help='A version in R1.2.3-xyz or 1.2.3 form')
    P.add_argument('-D', '--dev', action='store_true', default=None)
    P.add_argument('-R', '--release', dest='dev', action='store_false')
    P.add_argument('conf', nargs='+',
                   help='A configure/CONFIG_*_VERSION file name')
    return P

if __name__=='__main__':
    main(getargs().parse_args())
