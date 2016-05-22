#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Determines an absolute pathname for its argument,
# which may be either a relative or absolute path and
# might have trailing directory names that don't exist yet.

use strict;

use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use Getopt::Std;
use EPICS::Path;

our ($opt_h);

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
&HELP_MESSAGE if !getopts('h') || $opt_h || @ARGV != 1;

my $path = AbsPath(shift);

# Escape shell special characters unless on Windows, which doesn't allow them.
$path =~ s/([!"\$&'\(\)*,:;<=>?\[\\\]^`{|}])/\\$1/g unless $^O eq 'MSWin32';

print "$path\n";


sub HELP_MESSAGE {
    print STDERR "Usage: fullPathName.pl [-h] pathname\n";
    exit 2;
}
