#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
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
my $exec;

# Use WINE to run windows target executables on non-windows host
if( $TA =~ /^win32-x86/ && $HA !~ /^win/ ) {
  # new deb. derivatives have wine32 and wine64
  # older have wine and wine64
  # prefer wine32 if present
  my $wine32 = "/usr/bin/wine32";
  $wine32 = "/usr/bin/wine" if ! -x $wine32;
  $exec = "$wine32 $exe";
} elsif( $TA =~ /^windows-x64/ && $HA !~ /^win/ ) {
  $exec = "wine64 $exe";

# Run pc386 test harness w/ QEMU
} elsif( $TA =~ /^RTEMS-pc386-qemu$/ ) {
  $exec = "qemu-system-i386 -m 64 -no-reboot -serial stdio -display none -net nic,model=ne2k_pci -net user,restrict=yes -kernel $exe";

# Explicitly fail for other RTEMS targets
} elsif( $TA =~ /^RTEMS-/ ) {
  die "$0: I don't know how to create scripts for testing $TA on $HA\n";

} else {
  $exec = "./$exe";
}

open(my $OUT, '>', $target) or die "Can't create $target: $!\n";

print $OUT <<EOF;
#!/usr/bin/env perl

use strict;
use Cwd 'abs_path';

\$ENV{HARNESS_ACTIVE} = 1 if scalar \@ARGV && shift eq '-tap';
\$ENV{TOP} = abs_path(\$ENV{TOP}) if exists \$ENV{TOP};

system('$exec') == 0 or die "Can't run $exec: \$!\\n";
EOF

close $OUT or die "Can't close $target: $!\n";
