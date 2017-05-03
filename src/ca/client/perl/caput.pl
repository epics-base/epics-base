#!/usr/bin/env perl

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use Getopt::Std;
use CA;

our ($opt_0, $opt_c, $opt_e, $opt_f, $opt_g, $opt_h, $opt_l,
    $opt_n, $opt_s, $opt_S, $opt_t);
our $opt_w = 1;

$Getopt::Std::OUTPUT_HELP_VERSION = 1;

HELP_MESSAGE() unless getopts('achlnsStw:');
HELP_MESSAGE() if $opt_h;

die "No pv name specified. ('caput.pl -h' gives help.)\n"
    unless @ARGV;
my $pv = shift;
die "caput.pl: Empty pv name given.\n"
    unless $pv ne '';

die "No value specified. ('caput.pl -h' gives help.)\n"
    unless @ARGV;

my $chan = CA->new($pv);
eval {
    CA->pend_io($opt_w);
};
if ($@) {
    if ($@ =~ m/^ECA_TIMEOUT/) {
        print "Channel connect timed out: '$pv' not found.\n";
        exit 2;
    } else {
        die $@;
    }
}

die "Write access denied for '$pv'.\n" unless $chan->write_access;

my $n = $chan->element_count();
die "Too many values given, '$pv' limit is $n\n"
    unless $n >= @ARGV;

my $type = $chan->field_type;
$type = 'DBR_STRING'
    if $opt_s && $type =~ m/ ^ DBR_ (ENUM | FLOAT | DOUBLE) $ /x;
$type = 'DBR_LONG'
    if ($opt_n && $type eq 'DBR_ENUM')
    || (!$opt_S && $type eq 'DBR_CHAR');
$type =~ s/^DBR_/DBR_TIME_/
    if $opt_l;

my @values;
if ($type !~ m/ ^ DBR_ (STRING | ENUM | CHAR) $ /x) {
    # Make @ARGV strings numeric
    @values = map { +$_; } @ARGV;
} else {
    # Use strings
    @values = @ARGV;
}

my $done = 0;
if ($opt_t) {
    do_put();
} else {
    $chan->get_callback(\&old_callback, $type);
}
CA->pend_event(0.1) until $done;


sub old_callback {
    my ($chan, $status, $data) = @_;
    die $status if $status;
    display($chan, $type, $data, 'Old');
    do_put();
}

sub do_put {
    if ($opt_c) {
        $chan->put_callback(\&put_callback, @values);
    } else {
        $chan->put(@values);
        $chan->get_callback(\&new_callback, $type);
    }
}

sub put_callback {
    my ($chan, $status) = @_;
    die $status if $status;
    $chan->get_callback(\&new_callback, $type);
}

sub new_callback {
    my ($chan, $status, $data) = @_;
    die $status if $status;
    display($chan, $type, $data, 'New');
    $done = 1;
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
    my ($chan, $type, $data, $prefix) = @_;
    if (ref $data eq 'ARRAY') {
        display($chan, $type, join(' ', @{$data}), $prefix);
    } elsif (ref $data eq 'HASH') {
        $type = $data->{TYPE};  # Can differ from request
        my $value = $data->{value};
        if (ref $value eq 'ARRAY') {
            $value = join(' ', map { format_number($_, $type); } @{$value});
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
            printf "$prefix : %-30s %s\n", $chan->name, $value;
        }
    }
}

sub HELP_MESSAGE {
    print STDERR "\nUsage: caput.pl [options] <PV name> <PV value> ...\n",
        "\n",
        "  -h: Help: Print this message\n",
        "Channel Access options:\n",
        "  -w <sec>:  Wait time, specifies CA timeout, default is $opt_w second\n",
        "  -c: Use put_callback to wait for completion\n",
        "Format options:\n",
        "  -t: Terse mode - print only sucessfully written value, without name\n",
        "  -l: Long mode \"name timestamp value stat sevr\" (read PVs as DBR_TIME_xxx)\n",
        "  -S: Put string as an array of char (long string)\n",
        "Enum format:\n",
        "  Default: Auto - try value as ENUM string, then as index number\n",
        "  -n: Force interpretation of values as numbers\n",
        "  -s: Force interpretation of values as strings\n",
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
        "\n",
        "Examples:\n",
        "    caput my_channel 1.2\n",
        "    caput my_waveform 1.2 2.4 3.6 4.8 6.0\n",
        "\n",
        "Base version: ", CA->version, "\n";
    exit 1;
}

