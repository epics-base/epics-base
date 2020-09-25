#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# The makeTestfile.pl script generates a file $target.t which is needed
# because some versions of the Perl test harness can only run test scripts
# that are actually written in Perl.  The script we generate runs the
# real test program which must be in the same directory as the .t file.
# If the script is given an argument -tap it sets HARNESS_ACTIVE in the
# environment to make the epicsUnitTest code generate strict TAP output.

# Usage: makeTestfile.pl <target-arch> <host-arch> target.t executable
#     target-arch and host-arch are EPICS build target names (eg. linux-x86)
#     target.t is the name of the Perl script to generate
#     executable is the name of the file the script runs

use strict;

my ($TA, $HA, $target, $exe) = @ARGV;
my ($exec, $error);

if ($TA =~ /^win32-x86/ && $HA !~ /^win/) {
    # Use WINE to run win32-x86 executables on non-windows hosts.
    # New Debian derivatives have wine32 and wine64, older ones have
    # wine and wine64. We prefer wine32 if present.
    my $wine32 = "/usr/bin/wine32";
    $wine32 = "/usr/bin/wine" if ! -x $wine32;
    $error = $exec = "$wine32 $exe";
}
elsif ($TA =~ /^windows-x64/ && $HA !~ /^win/) {
    # Use WINE to run windows-x64 executables on non-windows hosts.
    $error = $exec = "wine64 $exe";
}
elsif ($TA =~ /^RTEMS-pc[36]86-qemu$/) {
    # Run the pc386 and pc686 test harness w/ QEMU
    $exec = "qemu-system-i386 -m 64 -no-reboot "
          . "-serial stdio -display none "
          . "-net nic,model=e1000 -net nic,model=ne2k_pci "
          . "-net user,restrict=yes "
          . "-append --console=/dev/com1 "
          . "-kernel $exe";
    $error = "qemu-system-i386 ... -kernel $exe";
}
elsif ($TA =~ /^RTEMS-/) {
    # Explicitly fail for other RTEMS targets
    die "$0: I don't know how to create scripts for testing $TA on $HA\n";
}
else {
    # Assume it's directly executable on other targets
    $error = $exec = "./$exe";
}

# Run the test program with system on Windows, exec elsewhere.
# This is required by the Perl test harness.
my $runtest = ($^O eq 'MSWin32') ?
    "system('$exec') == 0" : "exec '$exec'";

open my $OUT, '>', $target
    or die "Can't create $target: $!\n";

print $OUT <<__EOF__;
#!/usr/bin/env perl

use strict;
use Cwd 'abs_path';

\$ENV{HARNESS_ACTIVE} = 1 if scalar \@ARGV && shift eq '-tap';
\$ENV{TOP} = abs_path(\$ENV{TOP}) if exists \$ENV{TOP};

$runtest
    or die "Can't run $error: \$!\\n";
__EOF__

close $OUT
    or die "Can't close $target: $!\n";
