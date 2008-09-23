eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if 0;
#*************************************************************************
# Copyright (c) 2008 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

# Determines an absolute pathname for its argument,
# which may be either a relative or absolute path and
# might have trailing parts that don't exist yet.

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Path;

print AbsPath(shift), "\n";

