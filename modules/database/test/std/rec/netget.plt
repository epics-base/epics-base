#!/usr/bin/env perl

use strict;
use warnings;

use lib '@TOP@/lib/perl';

use Test::More tests => 3;
use EPICS::IOC;

$ENV{HARNESS_ACTIVE} = 1 if scalar @ARGV && shift eq '-tap';

# Keep traffic local and avoid duplicates over multiple interfaces
$ENV{EPICS_CA_AUTO_ADDR_LIST} = 'NO';
$ENV{EPICS_CA_ADDR_LIST} = 'localhost';
$ENV{EPICS_CA_SERVER_PORT} = 55064;
$ENV{EPICS_CAS_BEACON_PORT} = 55065;
$ENV{EPICS_CAS_INTF_ADDR_LIST} = 'localhost';

$ENV{EPICS_PVA_AUTO_ADDR_LIST} = 'NO';
$ENV{EPICS_PVA_ADDR_LIST} = 'localhost';
$ENV{EPICS_PVAS_SERVER_PORT} = 55075;
$ENV{EPICS_PVA_BROADCAST_PORT} = 55076;
$ENV{EPICS_PVAS_INTF_ADDR_LIST} = 'localhost';

my $bin = "@TOP@/bin/@ARCH@";
my $exe = ($^O =~ m/^(MSWin32|cygwin)$/x) ? '.exe' : '';
my $prefix = "test-$$";

my $ioc = EPICS::IOC->new();
#$ioc->debug(1);

$SIG{__DIE__} = $SIG{INT} = $SIG{QUIT} = sub {
    $ioc->kill;
    BAIL_OUT('Caught signal');
};


# Watchdog utilities

sub kill_bail {
    my $doing = shift;
    return sub {
        $ioc->kill;
        BAIL_OUT("Timeout $doing");
    }
}

sub watchdog (&$$) {
    my ($do, $timeout, $abort) = @_;
    $SIG{ALRM} = $abort;
    alarm $timeout;
    &$do;
    alarm 0;
}


# Start the IOC

my $softIoc = "$bin/softIocPVA$exe";
$softIoc = "$bin/softIoc$exe"
    unless -x $softIoc;
BAIL_OUT("Can't find a softIoc executable")
    unless -x $softIoc;

watchdog {
    $ioc->start($softIoc, '-x', $prefix);
    $ioc->cmd;  # Wait for command prompt
} 10, kill_bail('starting softIoc');


# Get Base Version number from PV

my $pv = "$prefix:BaseVersion";

watchdog {
    my @pvs = $ioc->dbl('stringin');
    grep(m/^ $pv $/x, @pvs)
        or BAIL_OUT('No BaseVersion record found');
} 10, kill_bail('running dbl');

my $version;
watchdog {
    $version = $ioc->dbgf("$pv");
} 10, kill_bail('getting BaseVersion');
like($version, qr/^ \d+ \. \d+ \. \d+ /x,
    "Got BaseVersion '$version' from iocsh");


# Channel Access

SKIP: {
    my $caget = "$bin/caget$exe";
    skip "caget not available", 1
        unless -x $caget;

    # CA Server Diagnostics

    watchdog {
        note("CA server configuration:\n",
            map("  $_\n", $ioc->cmd('casr', 1)));
    } 10, kill_bail('running casr');

    # CA Client test

    watchdog {
        my $caVersion = `$caget -w5 $pv`;
        like($caVersion, qr/^ $pv \s+ \Q$version\E $/x,
            'Got same BaseVersion from caget');
    } 10, kill_bail('doing caget');
}


# PV Access

SKIP: {
    my $pvget = "$bin/pvget$exe";
    skip "softIocPVA not available", 1
        if $softIoc eq "$bin/softIoc$exe";

    # PVA Server Diagnostics

    watchdog {
        note("PVA server configuration:\n",
            map("  $_\n", $ioc->cmd('pvasr')));
    } 10, kill_bail('running pvasr');

    skip "pvget not available", 1
        unless -x $pvget;

    # PVA Client test

    watchdog {
        my $pvaVersion = `$pvget -w5 $pv`;
        like($pvaVersion, qr/^ $pv \s .* \Q$version\E \s* $/x,
            'Got same BaseVersion from pvget');
    } 10, kill_bail('doing pvget');
}

$ioc->kill;
