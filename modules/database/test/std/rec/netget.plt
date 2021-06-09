#!/usr/bin/env perl

use strict;
use warnings;

use if $^O eq 'MSWin32', "Win32::Process";
use if $^O eq 'MSWin32', "Win32";

use lib '@TOP@/lib/perl';

use Test::More tests => 3;
use EPICS::IOC;

# Set to 1 to echo all IOC and client communications
my $debug = 0;

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

my $bin = '@TOP@/bin/@ARCH@';
my $exe = ($^O =~ m/^(MSWin32|cygwin)$/x) ? '.exe' : '';
my $prefix = "test-$$";

my $ioc = EPICS::IOC->new();
$ioc->debug($debug);

$SIG{__DIE__} = $SIG{INT} = $SIG{QUIT} = sub {
    $ioc->exit;
    BAIL_OUT("Caught signal: $_[0]");
};


# Watchdog utilities

sub kill_bail {
    my $doing = shift;
    return sub {
        $ioc->exit;
        BAIL_OUT("Timeout $doing");
    }
}

sub watchdog (&$$) {
    my ($code, $timeout, $fail) = @_;
    my $bark = "Woof $$\n";
    my $result;
    eval {
        local $SIG{__DIE__};
        local $SIG{ALRM} = sub { die $bark };
        alarm $timeout;
        $result = &$code;
        alarm 0;
    };
    if ($@) {
        die if $@ ne $bark;
        $result = &$fail;
    }
    return $result;
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

    my $caVersion = qx_timeout(15, "$caget -w5 $pv");
    like($caVersion, qr/^ $pv \s+ \Q$version\E $/x,
        'Got same BaseVersion from caget');
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

    my $pvaVersion = qx_timeout(15, "$pvget -w5 $pv");
    like($pvaVersion, qr/^ $pv \s .* \Q$version\E \s* $/x,
        'Got same BaseVersion from pvget');
}

$ioc->exit;


# Process timeout utilities

sub system_timeout {
    my ($timeout, $cmdline) = @_;
    my $status;
    if ($^O eq 'MSWin32') {
        my $proc;
        (my $app) = split ' ', $cmdline;
        if (! Win32::Process::Create($proc, $app, $cmdline,
            1, &Win32::Process::NORMAL_PRIORITY_CLASS, '.')) {
            my $err = Win32::FormatMessage(Win32::GetLastError());
            die "Can't create Process for '$cmdline': $err\n";
        }
        if (! $proc->Wait(1000 * $timeout)) {
            $proc->Kill(1);
            note("Timed out '$cmdline' after $timeout seconds\n");
        }
        my $status;
        $proc->GetExitCode($status);
        return $status;
    }
    else {
        my $pid;
        $status = watchdog {
            $pid = fork();
            die "Can't fork: $!\n"
                unless defined $pid;
            exec $cmdline
                or die "Can't exec: $!\n"
                unless $pid;
            waitpid $pid, 0;
            return $? >> 8;
        } $timeout, sub {
            kill 9, $pid if $pid;
            note("Timed out '$cmdline' after $timeout seconds\n");
            return -2;
        };
    }
    return $status;
}

sub qx_timeout {
    my ($timeout, $cmdline) = @_;
    open(my $stdout, '>&STDOUT')
        or die "Can't save STDOUT: $!\n";
    my $outfile = "stdout-$$.txt";
    unlink $outfile;
    open STDOUT, '>', $outfile;
    my $text;
    if (system_timeout($timeout, $cmdline) == 0 && -r $outfile) {
        open(my $file, '<', $outfile)
            or die "Can't open $outfile: $!\n";
        $text = join '', <$file>;
        close $file;
    }
    open(STDOUT, '>&', $stdout)
        or die "Can't restore STDOUT: $!\n";
    unlink $outfile;
    return $text;
}
