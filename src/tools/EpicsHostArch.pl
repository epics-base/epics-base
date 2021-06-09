#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Returns an architecture name for EPICS_HOST_ARCH that should be
# appropriate for the CPU that this version of Perl was built for.
# Any arguments to the program will be appended with separator '-'
# to allow flags like -gnu -debug and/or -static to be added.

# Before Base has been built, use a command like this:
#   bash$ export EPICS_HOST_ARCH=`perl src/tools/EpicsHostArch.pl`
#
# If Base is already built, use
#   tcsh% setenv EPICS_HOST_ARCH `perl base/lib/perl/EpicsHostArch.pl`

# If your architecture is not recognized by this script, please send
# the output from running 'perl --version' to the EPICS tech-talk
# mailing list to have it added.

use strict;

use Config;
use POSIX;

print join('-', HostArch(), @ARGV), "\n";

sub HostArch {
    my $arch = $Config{archname};
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

        my ($kernel, $hostname, $release, $version, $cpu) = uname;
        if (m/^darwin/) {
            for ($cpu) {
                return 'darwin-x86'     if m/^x86_64/;
                return 'darwin-aarch64' if m/^arm64/;
            }
            die "$0: macOS CPU type '$cpu' not recognized\n";
        }

        die "$0: Architecture '$arch' not recognized\n";
    }
}
