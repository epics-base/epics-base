#!/usr/bin/perl

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use Getopt::Std;
use CA;

our ($opt_0, $opt_a, $opt_C, $opt_d, $opt_e, $opt_f, $opt_g, $opt_h, $opt_n);
our ($opt_s, $opt_t);
our $opt_w = 1;

$Getopt::Std::OUTPUT_HELP_VERSION = 1;

HELP_MESSAGE() unless getopts('0:aC:d:e:f:g:hnstw:');
HELP_MESSAGE() if $opt_h;

$opt_d = "DBR_$opt_d" if $opt_d && $opt_d !~ m/^DBR_/;

die "No pv name specified. ('caget -h' gives help.)\n"
    unless @ARGV;

my @chans = map { CA->new($_); } @ARGV;

eval { CA->pend_io($opt_w); };
if ($@) {
    if ($@ =~ m/^ECA_TIMEOUT/) {
        my $err = (@chans > 1) ? 'some PV(s)' : "'" . $chans[0]->name . "'";
        print "Channel connect timed out: $err not found.\n";
        @chans = grep { $_->is_connected } @chans;
    } else {
        die $@;
    }
}

my %rtype;

map {
    my $type;
    if ($opt_d) {
        $type = $opt_d;
    } else {
        $type = $_->field_type;
        $type = 'DBR_STRING'
            if $opt_s && $type =~ m/ ^DBR_FLOAT$ | ^DBR_DOUBLE$ /x;
        $type = 'DBR_LONG'
            if $opt_n && $type eq 'DBR_ENUM';
        $type =~ s/^DBR_/DBR_TIME_/
            if $opt_a;
    }
    $rtype{$_} = $type;
    my $count = $_->element_count;
    $count = +$opt_C if $opt_C && $opt_C <= $count;
    $_->get_callback(\&get_callback, $type, $count);
} @chans;

my $incomplete = @chans;
CA->pend_event(0.1) while $incomplete;


sub get_callback {
    my ($chan, $status, $data) = @_;
    die $status if $status;
    display($chan, $rtype{$chan}, $data);
    $incomplete--;
}

sub format_number {
    my ($data, $type) = @_;
    if ($type =~ m/_DOUBLE$/) {
        return sprintf "%.${opt_e}e", $data if $opt_e;
        return sprintf "%.${opt_f}f", $data if $opt_f;
        return sprintf "%.${opt_g}g", $data if $opt_g;
    }
    if ($type =~ m/_LONG$/) {
        return sprintf "%lx", $data if $opt_0 eq 'x';
        return sprintf "%lo", $data if $opt_0 eq 'o';
        if ($opt_0 eq 'b') {
            my $bin = unpack "B*", pack "l", $data;
            $bin =~ s/^0*//;
            return $bin;
        }
    }
    return $data;
}

sub display {
    my ($chan, $type, $data) = @_;
    if (ref $data eq 'ARRAY') {
        display($chan, $type, join(' ', scalar @{$data}, @{$data}));
    } elsif (ref $data eq 'HASH') {
        $type = $data->{TYPE};  # Can differ from request
        my $value = $data->{value};
        if (ref $value eq 'ARRAY') {
            $value = join(' ', $data->{COUNT},
                map { format_number($_, $type); } @{$value});
        } else {
            $value = format_number($value, $type);
        }
        my $stamp;
        if (exists $data->{stamp}) {
            my @t = localtime $data->{stamp};
            splice @t, 6;
            $t[5] += 1900;
            $t[0] += $data->{stamp_fraction};
            $stamp = sprintf "%4d-%02d-%02d %02d:%02d:%09.6f", reverse @t;
        }
        printf "%-30s %s %s %s %s\n", $chan->name,
            $stamp, $value, $data->{status}, $data->{severity};
    } else {
        my $value = format_number($data, $type);
        if ($opt_t) {
            print "$value\n";
        } else {
            printf "%-30s %s\n", $chan->name, $value;
        }
    }
}

sub HELP_MESSAGE {
    print STDERR "\nUsage: caget [options] <PV name> ...\n",
        "\n",
        "  -h: Help: Print this message\n",
        "Channel Access options:\n",
        "  -w <sec>:  Wait time, specifies longer CA timeout, default is $opt_w second\n",
        "Format options:\n",
        "  -t: Terse mode - print only value, without name\n",
        "  -a: Wide mode \"name timestamp value stat sevr\" (read PVs as DBR_TIME_xxx)\n",
        "  -d <type>: Request specific dbr type from one of the following:\n",
        "        DBR_STRING       DBR_LONG       DBR_DOUBLE\n",
        "        DBR_STS_STRING   DBR_STS_LONG   DBR_STS_DOUBLE\n",
        "        DBR_TIME_STRING  DBR_TIME_LONG  DBR_TIME_DOUBLE\n",
        "        DBR_GR_STRING    DBR_GR_LONG    DBR_GR_DOUBLE    DBR_GR_ENUM\n",
        "        DBR_CTRL_STRING  DBR_CTRL_LONG  DBR_CTRL_DOUBLE  DBR_CTRL_ENUM\n",
        "        DBR_CLASS_NAME   DBR_STSACK_STRING\n",
        "Arrays: Value format: print number of values, then list of values\n",
        "  Default:    Print all values\n",
        "  -C <count>: Print first <count> elements of an array\n",
        "Enum format:\n",
        "  -n: Print DBF_ENUM value as number (default is enum string)\n",
        "Floating point type format:\n",
        "  Default: Use %g format\n",
        "  -e <nr>: Use %e format, with a precision of <nr> digits\n",
        "  -f <nr>: Use %f format, with a precision of <nr> digits\n",
        "  -g <nr>: Use %g format, with a precision of <nr> digits\n",
        "  -s:      Get value as string (may honour server-side precision)\n",
        "Integer number format:\n",
        "  Default: Print as decimal number\n",
        "  -0x: Print as hex number\n",
        "  -0o: Print as octal number\n",
        "  -0b: Print as binary number\n",
        "\n";
    exit 1;
}

