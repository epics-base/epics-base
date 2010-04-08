#!/usr/bin/perl

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use DBD::Output;
use EPICS::Getopts;
use Readfile;
use macLib;

getopts('DI@S@o:') or
    die "Usage: dbdExpand [-D] [-I dir] [-S macro=val] [-o out.dbd] in.dbd ...";

my @path = map { split /[:;]/ } @opt_I;
my $macros = macLib->new(@opt_S);
my $dbd = DBD->new();

while (@ARGV) {
    &ParseDBD($dbd, &Readfile(shift @ARGV, $macros, \@opt_I));
}

if ($opt_D) {   # Output dependencies only
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
    exit 0;
}

my $out;
if ($opt_o) {
    open $out, '>', $opt_o or die "Can't create $opt_o: $!\n";
} else {
    $out = STDOUT;
}

&OutputDBD($out, $dbd);

if ($opt_o) {
    close $out or die "Closing $opt_o failed: $!\n";
}
exit 0;
