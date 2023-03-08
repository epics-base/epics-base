#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

use Getopt::Std;
$Getopt::Std::STANDARD_HELP_VERSION = 1;

use Config;
use POSIX;

use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use Pod::Usage;

=head1 NAME

EpicsHostArch.pl - Prints the current host architecture

=head1 SYNOPSIS

B<EpicsHostArch.pl> [extension]

B<EpicsHostArch.pl> -g <GNU-like arch> [extension]

B<EpicsHostArch.pl> -h

=head1 DESCRIPTION

Returns an architecture name for EPICS_HOST_ARCH that should be
appropriate for the CPU that this version of Perl was built for.
Any arguments to the program will be appended with separator '-'
to allow flags like -gnu -debug and/or -static to be added.

Before Base has been built, use a command like this:

C<bash$ export EPICS_HOST_ARCH=`perl src/tools/EpicsHostArch.pl`>

If Base is already built, use:

C<tcsh% setenv EPICS_HOST_ARCH `perl base/lib/perl/EpicsHostArch.pl`>

If your architecture is not recognized by this script, please send
the output from running C<perl --version> to the EPICS tech-talk
mailing list to have it added.

If the C<-g> option is provided with an argument, print the EPICS
architecture corresponding to the given GNU architecture tuplet.

=head1 OPTIONS

B<EpicsHostArch.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=item B<-g> <GNU arch tuplet>

If specified, convert the given GNU architecture tuplet instead.

=back

=head1 EXAMPLES

C<EpicsHostArch.pl>

C<EpicsHostArch.pl static>

C<EpicsHostArch.pl -g powerpc64-linux>

C<EpicsHostArch.pl -g MSWin32-x64 static>

=cut

our ($opt_h);
our ($opt_g);
$opt_h = 0;

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h * 3);
}

HELP_MESSAGE() if !getopts('hg:') || $opt_h;

# Convert GNU-like architecture tuples (<cpu>-<system>)
# to EPICS terminology (<system>-<cpu>)
#
# Documentation for GNU-like terminology:
# - https://www.gnu.org/software/autoconf/manual/autoconf-2.65/html_node/System-Type.html#System-Type
# - https://git.savannah.gnu.org/cgit/config.git/tree/
#
# Some examples from the Debian project:
# - https://wiki.debian.org/Multiarch/Tuples
sub toEpicsArch {
    my $arch = shift;
    for ($arch) {
        return 'linux-x86_64'   if m/^x86_64-linux/;
        return 'linux-x86'      if m/^i[3-6]86-linux/;
        return 'linux-arm'      if m/^arm-linux/;
        return 'linux-aarch64'  if m/^aarch64-linux/;
        return 'linux-ppc64'    if m/^powerpc64-linux/;
        return 'windows-x64'    if m/^MSWin32-x64/;
        return 'win32-x86'      if m/^MSWin32-x86/;
        return "cygwin-x86_64"  if m/^x86_64-cygwin/;
        return "cygwin-x86"     if m/^i[3-6]86-cygwin/;
        return 'solaris-sparc'  if m/^sun4-solaris/;
        return 'solaris-x86'    if m/^i86pc-solaris/;
        return 'freebsd-x86_64' if m/^x86_64-freebsd/;
        return 'freebsd-x86_64' if m/^amd64-freebsd/;
        return 'darwin-x86'     if m/^x86(_64)?-darwin/;
        return 'darwin-aarch64' if m/^(arm64|aarch64)-darwin/;

        # mingw64 has 32bit and 64bit build shells which give same arch result
        if (m/^(x86_64|i686)-msys/) {
            die "$0: Architecture '$arch' is unclear,\n".
                "try again after explicitly setting the EPICS_HOST_ARCH environment variable.\n".
                "Use  EPICS_HOST_ARCH=windows-x64-mingw  for a 64bit MinGW build, or\n".
                "EPICS_HOST_ARCH=win32-x86-mingw  for a 32bit MinGW build.\n"
        }
        die "$0: Architecture '$arch' not recognized\n";
    }
}

my $arch;

if ($opt_g) {
    $arch = $opt_g;
} else {
    $arch = $Config{archname};

    # On darwin, $Config{archname} returns darwin-2level, which is unusable
    # so we use `uname` instead
    $_ = $arch;
    if (m/^darwin/) {
        my ($kernel, $hostname, $release, $version, $cpu) = uname;
        $arch = $cpu . "-darwin";
    }
}

print join('-', toEpicsArch($arch), @ARGV), "\n";
