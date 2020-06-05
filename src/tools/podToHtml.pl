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

# To find the EPICS::PodHtml module used below we need to add our lib/perl to
# the lib search path. If the script is running from the src/tools directory
# before everything has been installed though, the search path must include
# our source directory (i.e. $Bin), so we add both here.
use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl", $Bin);

use EPICS::Getopts;

use EPICS::PodHtml;

use Pod::Usage;

=head1 NAME

podToHtml.pl - Convert EPICS .pod files to .html

=head1 SYNOPSIS

B<podToHtml.pl> [B<-h>] [B<-s>] [B<-o> file.html] file.pod

=head1 DESCRIPTION

Converts files from Perl's POD format into HTML format.

The generated HTML output file refers to a CSS style sheet F<style.css> which
can be located in a parent directory of the final installation directory. The
relative path to that file (i.e. the number of parent directories to traverse)
is calculated based on the number of components in the path to the input file.

=head1 OPTIONS

B<podToHtml.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=item B<-s>

Indicates that one leading component of the input file path is not part of the
final installation path, thus should be removed before calculating the relative
path to the style-sheet file. This flag may be repeated as many times as needed
to remove multiple leading components from the path to the style sheet.

=item B<-o> file.html

Name of the HTML output file to be created.

=back

If no output filename is set, the file created will be named after the input
file, removing any directory components in the path and replacing any file
extension with .html.

=cut

our ($opt_o, $opt_h);
our $opt_s = 0;

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h);
}

HELP_MESSAGE() if !getopts('ho:s') || $opt_h || @ARGV != 1;

my $infile = shift @ARGV;

my @inpath = split /\//, $infile;
my $file = pop @inpath;

if (!$opt_o) {
    ($opt_o = $file) =~ s/\. \w+ $/.html/x;
}

# Calculate path to style.css file
shift @inpath while $opt_s--; # Remove leading ..
my $root = '../' x scalar @inpath;

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

my $podHtml = EPICS::PodHtml->new();

$podHtml->html_css($root . 'style.css');
$podHtml->perldoc_url_prefix('');
$podHtml->perldoc_url_postfix('.html');
$podHtml->set_source($infile);
$podHtml->output_string(\my $html);
$podHtml->run;

print $out $html;
close $out;

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut
