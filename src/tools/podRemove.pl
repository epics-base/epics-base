#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

use Getopt::Std;
$Getopt::Std::STANDARD_HELP_VERSION = 1;

use Pod::Usage;

=head1 NAME

podRemove.pl - Remove POD directives from files

=head1 SYNOPSIS

B<podRemove.pl> [B<-h>] [B<-o> file] file.pod

=head1 DESCRIPTION

Removes Perl's POD documentation from a text file

=head1 OPTIONS

B<podRemove.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=item B<-o> file

Name of the output file to be created.

=back

If no output filename is set, the file created will be named after the input
file, removing any directory components in the path and removing any .pod file
extension.

=cut

our ($opt_o, $opt_h);

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h);
}

HELP_MESSAGE() if !getopts('ho:') || $opt_h || @ARGV != 1;

my $infile = shift @ARGV;

if (!$opt_o) {
    ($opt_o = $infile) =~ s/\.pod$//;
    $opt_o =~ s/^.*\///;
}

open my $inp, '<', $infile or
    die "podRemove.pl: Can't open $infile: $!\n";
open my $out, '>', $opt_o or
    die "podRemove.pl: Can't create $opt_o: $!\n";

my $inPod = 0;
while (<$inp>) {
    if (m/\A=[a-zA-Z]/) {
        $inPod = !m/\A=cut/;
    }
    else {
        print $out $_ unless $inPod;
    }
}

close $out;
close $inp;

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2015 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut
