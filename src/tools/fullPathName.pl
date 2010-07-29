eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if 0;
#*************************************************************************
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

# Determines an absolute pathname for its argument,
# which may be either a relative or absolute path and
# might have trailing directory names that don't exist yet.

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use Getopt::Std;
use EPICS::Path;

our ($opt_h);

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
&HELP_MESSAGE if !getopts('h') || $opt_h || @ARGV != 1;

my $path = AbsPath(shift);

print "$path\n";


sub HELP_MESSAGE {
    print STDERR "Usage: fullPathName.pl [-h] pathname\n";
    exit 2;
}
