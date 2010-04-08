#!/usr/bin/perl

# This version of dbExpand does not syntax check its input files or remove
# duplicate definitions, unlike the dbStaticLib version.  It does do the
# include file expansion and macro substitution though.

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Getopts;
use Readfile;
use macLib;

getopts('DI@S@o:') or
    die "Usage: dbExpand [-D] [-I dir] [-S macro=val] [-o out.dbd] in.dbd ...";

my @path = map { split /[:;]/ } @opt_I;
my @output;

my $macros = macLib->new(@opt_S);

while (@ARGV) {
    my @file = &Readfile(shift @ARGV, $macros, \@opt_I);
    # Strip the stuff that Readfile() added:
    push @output, grep !/^\#\#!/, @file
}

if ($opt_D) {   # Output dependencies, not the expanded data
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
} elsif ($opt_o) {
    open OUTFILE, ">$opt_o" or die "Can't create $opt_o: $!\n";
    print OUTFILE @output;
    close OUTFILE;
} else {
    print @output;
}
