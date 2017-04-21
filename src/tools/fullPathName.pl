#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Determines an absolute pathname for its argument,
# which may be either a relative or absolute path and
# might have trailing directory names that don't exist yet.

use strict;
use warnings;

use Getopt::Std;
$Getopt::Std::STANDARD_HELP_VERSION = 1;

use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use EPICS::Path;

use Pod::Usage;

=head1 NAME

fullPathName.pl - Convert a pathname to an absolute path

=head1 SYNOPSIS

B<fullPathName.pl> [B<-h>] /path/to/something

=head1 DESCRIPTION

The EPICS build system needs the ability to get the absolute path of a file or
directory that does not exist at the time. The AbsPath() function in the
EPICS::Path module provides the necessary functionality, which this script makes
available to the build system. The string which is returned on the standard
output stream has had any shell special characters escaped with a back-slash
(except on Windows).

=head1 OPTIONS

B<fullPathName.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=back

=cut

our ($opt_h);

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h);
}

HELP_MESSAGE() if !getopts('h') || $opt_h || @ARGV != 1;

my $path = AbsPath(shift);

# Escape shell special characters unless on Windows, which doesn't allow them.
$path =~ s/([!"\$&'\(\)*,:;<=>?\[\\\]^`{|}])/\\$1/g unless $^O eq 'MSWin32';

print "$path\n";

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut
