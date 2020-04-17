#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

# To find the EPICS::PodHtml module used below we need to add our lib/perl to
# the lib search path. If the script is running from the src/tools directory
# before everything has been installed though, the search path must include
# our source directory (i.e. $Bin), so we add both here.
use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use Getopt::Std;
use EPICS::PodHtml;

our ($opt_o);

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
&HELP_MESSAGE if !getopts('o:') || @ARGV != 1;

my $infile = shift @ARGV;

if (!$opt_o) {
    ($opt_o = $infile) =~ s/\. \w+ $/.html/x;
    $opt_o =~ s/^.*\///;
}

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

my $podHtml = EPICS::PodHtml->new();

$podHtml->html_css('style.css');
$podHtml->set_source($infile);
$podHtml->output_string(\my $html);
$podHtml->run;

print $out $html;
close $out;

sub HELP_MESSAGE {
    print STDERR "Usage: podToHtml.pl [-o file.html] file.pod\n";
    exit 2;
}
