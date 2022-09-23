#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2005 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Tool to expand @VAR@ variables while copying a file.
# The output file will *not* be written to if it already
# exists and the expansion would leave the file unchanged.
#
# Author: Andrew Johnson <anj@aps.anl.gov>
# Date: 10 February 2005
#

use strict;

use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl");

use EPICS::Getopts;
use EPICS::Path;
use EPICS::Release;

# Process command line options
our ($opt_a, $opt_d, @opt_D, $opt_h, $opt_q, $opt_t);
getopts('a:dD@hqt:')
    or HELP_MESSAGE();

# Handle the -h command
HELP_MESSAGE() if $opt_h;

die "Path to TOP not set, use -t option\n"
    unless $opt_t;

# Check filename arguments
my $infile = shift
    or die "No input filename argument\n";
my $outfile = shift
    or die "No output filename argument\n";

# Where are we?
my $top = AbsPath($opt_t);

# Read RELEASE file into vars
my %vars = (TOP => $top);
my @apps = ('TOP');
readReleaseFiles("$top/configure/RELEASE", \%vars, \@apps, $opt_a);
expandRelease(\%vars);

$vars{'ARCH'} = $opt_a if $opt_a;

while ($_ = shift @opt_D) {
    m/^ (\w+) \s* = \s* (.*) $/x;
    $vars{$1} = $2;
}

print "Variables defined:\n",
    map "  $_ = $vars{$_}\n", sort keys %vars
    if $opt_d;

# Generate the expanded output
open(my $SRC, '<', $infile)
    or die "$! reading $infile\n";

my $vf=0;
my %nf;
my $output = join '', map {
    # Substitute any @VARS@ in the text
    s{@([A-Za-z0-9_]+)@}
     {exists $vars{$1} ? (++$vf, $vars{$1}) : (++$nf{$1}, "\@$1\@")}eg;
    $_
    } <$SRC>;
close $SRC;

my $vn = scalar %nf;
print "Expanded $infile => $outfile with $vf successes, $vn failures\n",
    map {"  \@$_\@ - " . $nf{$_} . " instance(s)\n"} keys %nf
    if $opt_d;

# Check if the output file matches
my $DST;
if (open($DST, '+<', $outfile)) {

    my $actual = join('', <$DST>);

    if ($actual eq $output) {
        close $DST;
        print "expandVars.pl: Keeping existing output file $outfile\n"
            unless $opt_q;
        exit 0;
    }

    seek $DST, 0, 0;
    truncate $DST, 0;
} else {
    open($DST, '>', $outfile)
        or die "Can't create $outfile: $!\n";
}

print $DST $output;
close $DST;
exit 0;

##### Subroutines only below here

sub HELP_MESSAGE {
    print STDERR <<EOF;
Usage:
    expandVars.pl -h
        Display this Usage message
    expandVars.pl -t /path/to/top [-a arch] -D var=val ... [-q] infile outfile
        Expand vars in infile to generate outfile
EOF
    exit $opt_h ? 0 : 1;
}
