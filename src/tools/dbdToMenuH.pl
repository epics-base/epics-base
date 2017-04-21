#!/usr/bin/perl

#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Getopts;
use File::Basename;
use DBD;
use DBD::Parser;
use EPICS::macLib;
use EPICS::Readfile;

my $tool = 'dbdToMenuH.pl';

use vars qw($opt_D @opt_I $opt_o $opt_s);
getopts('DI@o:') or
    die "Usage: $tool: [-D] [-I dir] [-o menu.h] menu.dbd [menu.h]\n";

my @path = map { split /[:;]/ } @opt_I; # FIXME: Broken on Win32?
my $dbd = DBD->new();

my $infile = shift @ARGV;
$infile =~ m/\.dbd$/ or
    die "$tool: Input file '$infile' must have '.dbd' extension\n";
my $inbase = basename($infile);

my $outfile;
if ($opt_o) {
    $outfile = $opt_o;
} elsif (@ARGV) {
    $outfile = shift @ARGV;
} else {
    ($outfile = $infile) =~ s/\.dbd$/.h/;
    $outfile =~ s/^.*\///;
}
my $outbase = basename($outfile);

# Derive a name for the include guard
my $guard_name = "INC_$outbase";
$guard_name =~ tr/a-zA-Z0-9_/_/cs;
$guard_name =~ s/(_[hH])?$/_H/;

ParseDBD($dbd, Readfile($infile, 0, \@opt_I));

if ($opt_D) {
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$outfile: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
} else {
    open OUTFILE, ">$outfile" or die "$tool: Can't open $outfile: $!\n";
    print OUTFILE "/* $outbase generated from $inbase */\n\n",
        "#ifndef $guard_name\n",
        "#define $guard_name\n\n";
    my $menus = $dbd->menus;
    while (my ($name, $menu) = each %{$menus}) {
        print OUTFILE $menu->toDeclaration;
    }
# FIXME: Where to put metadata for widely used menus?
# In the generated menu.h file is wrong: can't create a list of menu.h files.
# Can only rely on registerRecordDeviceDriver output, so we must require that
# all such menus be named "menu...", and any other menus must be defined in
# the record.dbd file that needs them.
#    print OUTFILE "\n#ifdef GEN_MENU_METADATA\n\n";
#    while (($name, $menu) = each %{$menus}) {
#        print OUTFILE $menu->toDefinition;
#    }
#    print OUTFILE "\n#endif /* GEN_MENU_METADATA */\n";
    print OUTFILE "\n#endif /* $guard_name */\n";
    close OUTFILE;
}
