#!/usr/bin/env perl

# SPDX-FileCopyrightText: 2008 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

use strict;

# This construct sets @INC to search lib/perl of all RELEASE entries
use FindBin qw($Bin);
use lib ($Bin, "$Bin/../../lib/perl");
use _APPNAME_ModuleDirs;
no lib $Bin;

use Getopt::Std;
use CA;

our $opt_w = 1;
our $opt_h;

$Getopt::Std::OUTPUT_HELP_VERSION = 1;

HELP_MESSAGE() unless getopts('hw:');
HELP_MESSAGE() if $opt_h;

die "No pv name specified. ('cainfo.pl -h' gives help.)\n"
    unless @ARGV;

my @chans = map { CA->new($_); } grep { $_ ne '' } @ARGV;

die "cainfo.pl: Please provide at least one non-empty pv name\n"
    unless @chans;

eval {
    CA->pend_io($opt_w);
};
if ($@) {
    if ($@ =~ m/^ECA_TIMEOUT/) {
        my $err = (@chans > 1) ? 'some PV(s)' : "'" . $chans[0]->name . "'";
        print "Channel connect timed out: $err not found.\n";
    } else {
        die $@;
    }
}

map { display($_); } @chans;


sub display {
    my $chan = shift;

    printf "%s\n", $chan->name;
    printf "    State:         %s\n", $chan->state;
    printf "    Host:          %s\n", $chan->host_name;

    my @access = ('no ', '');
    printf "    Access:        %sread, %swrite\n",
        $access[$chan->read_access], $access[$chan->write_access];

    printf "    Data type:     %s\n", $chan->field_type;
    printf "    Element count: %d\n", $chan->element_count;
}

sub HELP_MESSAGE {
    print STDERR "\nUsage: cainfo.pl [options] <PV name> ...\n",
        "\n",
        "  -h: Help: Print this message\n",
        "Channel Access options:\n",
        "  -w <sec>:   Wait time, specifies CA timeout, default is $opt_w second\n",
        "\n",
        "Example: cainfo my_channel another_channel\n",
        "\n",
        "Base version: ", CA->version, "\n";
    exit 1;
}
