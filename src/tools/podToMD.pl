#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

# To find the EPICS::Getopts module used below we need to add our lib/perl to
# the lib search path. If the script is running from the src/tools directory
# before everything has been installed though, the search path must include
# our source directory (i.e. $Bin), so we add both here.
use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use EPICS::Getopts;

use EPICS::PodMD;

use Pod::Usage;

=head1 NAME

podToMD.pl - Convert EPICS .pod files to .md

=head1 SYNOPSIS

B<podToMD.pl> [B<-h>] [B<-o> file.md] file.pod

=head1 DESCRIPTION

Converts files from Perl's POD format into Markdown format.

=head1 OPTIONS

B<podToMD.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=item B<-o> file.md

Name of the Markdown output file to be created.

=back

If no output filename is set, the file created will be named after the input
file, removing any directory components in the path and replacing any file
extension with .md.

=cut

our ($opt_o, $opt_h);

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h);
}

HELP_MESSAGE() if !getopts('ho:') || $opt_h || @ARGV != 1;

my $infile = shift @ARGV;

my @inpath = split /\//, $infile;
my $file = pop @inpath;

if (!$opt_o) {
    ($opt_o = $file) =~ s/\. \w+ $/.md/x;
}

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

my $podMD = EPICS::PodMD->new();

$podMD->perldoc_url_prefix('');
# $podMD->perldoc_url_postfix('.md');
$podMD->output_string(\my $md);
$podMD->parse_file($infile);

print $out $md;
close $out;

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut
