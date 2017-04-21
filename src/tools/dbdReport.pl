#!/usr/bin/perl

#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use EPICS::Getopts;
use EPICS::macLib;
use EPICS::Readfile;
use Text::Wrap;

#$EPICS::Readfile::debug = 1;
#$DBD::Parser::debug = 1;

getopts('I@S@') or die usage();

sub usage() {
    "Usage: dbdReport [-I dir:dir2] [-S macro=val,...] file.dbd ...";
}

my @path = map { split /[:;]/ } @opt_I; # FIXME: Broken on Win32?
my $macros = EPICS::macLib->new(@opt_S);
my $dbd = DBD->new();

ParseDBD($dbd, Readfile(shift @ARGV, $macros, \@opt_I)) while @ARGV;

$Text::Wrap::columns = 75;

my @menus = sort keys %{$dbd->menus};
print wrap("Menus:\t", "\t", join(', ', @menus)), "\n"
        if @menus;
my @drivers = sort keys %{$dbd->drivers};
print wrap("Drivers: ", "\t", join(', ', @drivers)), "\n"
        if @drivers;
my @variables = sort keys %{$dbd->variables};
print wrap("Variables: ", "\t", join(', ', @variables)), "\n"
        if @variables;
my @registrars = sort keys %{$dbd->registrars};
print wrap("Registrars: ", "\t", join(', ', @registrars)), "\n"
        if @registrars;
my @breaktables = sort keys %{$dbd->breaktables};
print wrap("Breaktables: ", "\t", join(', ', @breaktables)), "\n"
        if @breaktables;
my %recordtypes = %{$dbd->recordtypes};
if (%recordtypes) {
    @rtypes = sort keys %recordtypes;
    print wrap("Recordtypes: ", "\t", join(', ', @rtypes)), "\n";
    foreach my $rtyp (@rtypes) {
        my @devices = $recordtypes{$rtyp}->devices;
        print wrap("Devices($rtyp): ", "\t",
                   join(', ', map {$_->choice} @devices)), "\n"
            if @devices;
    }
}
my @records = sort keys %{$dbd->records};
print wrap("Records: ", "\t", join(', ', @records)), "\n"
        if @records;

