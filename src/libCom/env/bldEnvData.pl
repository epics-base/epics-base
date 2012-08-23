#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
#
# Author: Kay-Uwe Kasemir
# Date: 1-30-97

use strict;

# This program is never installed, so it can't use FindBin to get
# the path to the lib/perl directory.  However it can load the
# EPICS:: modules directly from the src/tools directory instead:
use lib '../../tools';

use Getopt::Std;
use File::Basename;
use EPICS::Path;
use EPICS::Release;
use Text::Wrap;

my $tool = basename($0);

our ($opt_h, $opt_q);
our $opt_o = 'envData.c';

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
$Text::Wrap::columns = 75;

&HELP_MESSAGE unless getopts('ho:q') && @ARGV == 1;
&HELP_MESSAGE if $opt_h;

my $config   = AbsPath(shift);
my $env_defs = AbsPath('../env/envDefs.h');

# Parse the ENV_PARAM declarations in envDefs.h
# to get the param names we are interested in
#
open SRC, '<', $env_defs
    or die "$tool: Cannot open $env_defs: $!\n";

my @vars;
while (<SRC>) {
    if (m/epicsShareExtern\s+const\s+ENV_PARAM\s+([A-Za-z_]\w*)\s*;/) {
        push @vars, $1;
    }
}
close SRC;

# Read the values from the CONFIG_ENV and CONFIG_SITE_ENV files
#
my $config_env      = "$config/CONFIG_ENV";
my $config_site_env = "$config/CONFIG_SITE_ENV";

my %values;
readReleaseFiles($config_env, \%values);
readReleaseFiles($config_site_env, \%values);

# Warn about any vars with no value
#
my @undefs = grep {!exists $values{$_}} @vars;
warn "$tool: No value given for $_\n" foreach @undefs;

print "Generating $opt_o\n" unless $opt_q;

# Start creating the output
#
open OUT, '>', $opt_o
    or die "$tool: Cannot create $opt_o: $!\n";

print OUT << "END";
/* Generated file $opt_o
 *
 * Created from
 *   $env_defs
 *   $config_env
 *   $config_site_env
 */

#include <stddef.h>
#define epicsExportSharedSymbols
#include "envDefs.h"

END

# Define all parameters, giving variable name and default value
#
foreach my $var (@vars) {
    my $default = $values{$var} || '';
    $default =~ s/^"//;
    $default =~ s/"$//;

    print OUT "epicsShareDef const ENV_PARAM $var =\n",
              "    {\"$var\", \"$default\"};\n";
}

# Now create a list of all those parameters
#
print OUT "\n",
    "epicsShareDef const ENV_PARAM* env_param_list[] = {\n",
    wrap('    ', '    ', join(', ', map("&$_", @vars), 'NULL')),
    "\n};\n";
close OUT;

sub HELP_MESSAGE {
    print STDERR "Usage: $tool [options] configure\n",
        "  -h       Help: Print this message\n",
        "  -q       Quiet: Only print errors\n",
        "  -o file  Output filename, default is $opt_o\n",
        "\n";

    exit 1;
}
