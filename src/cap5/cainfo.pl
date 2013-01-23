#!/usr/bin/env perl

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use Getopt::Std;
use CA;

our $opt_w = 1;
our $opt_h;

$Getopt::Std::OUTPUT_HELP_VERSION = 1;

HELP_MESSAGE() unless getopts('hw:');
HELP_MESSAGE() if $opt_h;

die "No pv name specified. ('cainfo -h' gives help.)\n"
    unless @ARGV;

my @chans = map { CA->new($_); } @ARGV;

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
    print STDERR "\nUsage: cainfo [options] <PV name> ...\n",
        "\n",
        "  -h: Help: Print this message\n",
        "Channel Access options:\n",
        "  -w <sec>:   Wait time, specifies CA timeout, default is $opt_w second\n",
        "\n",
        "Example: cainfo my_channel another_channel\n",
        "\n";
    exit 1;
}
