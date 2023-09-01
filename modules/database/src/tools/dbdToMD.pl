#!/usr/bin/env perl

#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;


use FindBin qw($Bin);
use lib ("$Bin/../../lib/perl");

use DBD;
use DBD::Parser;
use EPICS::Getopts;
use EPICS::macLib;
use EPICS::PodMD;
use EPICS::Readfile;

use Pod::Usage;

=head1 NAME

dbdToMD.pl - Convert DBD file with POD to Markdown

=head1 SYNOPSIS

B<dbdToMD.pl> [B<-h>] [B<-D>] [B<-I> dir] [B<-o> file] file.dbd.pod

=head1 DESCRIPTION

Generates MArkdown documentation from a B<.dbd.pod> file.

=head1 OPTIONS

B<dbdToMD.pl> understands the following options:

=over 4

=item B<-h>

Help, display usage information.

=item B<-H>

Conversion help, display information about converting reference documentation
from the EPICS Wiki into a B<.dbd.pod> file for use with this tool.

=item B<-I>

Path to look for include files.

=item B<-o> file

Name of the output file to be created.

=back

If no output filename is set, the file created will be named after the input
file, removing any directory components in the path and replacing any
B<.dbd.pod> file extension with B<.md>.

=cut

our ($opt_h, $opt_H, @opt_I, $opt_o);

my $tool = 'dbdToMD.pl';

getopts('hHI@o:') or
    pod2usage(2);
pod2usage(-verbose => 2) if $opt_H;
pod2usage(1) if $opt_h;
pod2usage("$tool: No input file given.\n") if @ARGV != 1;

my $dbd = DBD->new();

my $infile = shift @ARGV;
$infile =~ m/\.dbd.pod$/ or
    pod2usage("$tool: Input file '$infile' must have '.dbd.pod' extension.\n");

ParseDBD($dbd, Readfile($infile, 0, \@opt_I));

if (!$opt_o) {
    ($opt_o = $infile) =~ s/\.dbd\.pod$/.md/;
    $opt_o =~ s/^.*\///;
    $opt_o =~ s/dbCommonRecord/dbCommon/;
}

(my $title = $opt_o) =~ s/\.md$//;

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

sub make_fragment {
	my $fragment = $_[1];
	$fragment =~ s/\W+/-/g;
	$fragment = lc($fragment);
	$_[1] = $fragment;
}

my $podRst = EPICS::PodMD->new(
	perldoc_url_prefix => '',
	perldoc_fragment_format => make_fragment,
	markdown_fragment_format => make_fragment,
);

# Parse the Pod text from the root DBD object
my $pod = join "\n",
    map {
        # Handle a 'recordtype' Pod directive
        if (m/^ =recordtype \s+ (\w+) /x) {
            my $rn = $1;
            my $rtyp = $dbd->recordtype($rn);
            die "Unknown recordtype '$rn' in $infile POD directive\n"
                unless $rtyp;
            rtypeToMD($rtyp, $dbd);
        }
        # Handle a 'menu' Pod directive
        elsif (m/^ =menu \s+ (\w+) /x) {
            my $mn = $1;
            my $menu = $dbd->menu($mn);
            die "Unknown menu '$mn' in $infile POD directive\n"
                unless $menu;
            menuToMD($menu);
        }
        elsif (m/^ =title \s+ (.*)/x) {
            $title = $1;
            "=head1 $title";
        }
        else {
            $_;
        }
    } $dbd->pod;

$podRst->output_fh($out);
$podRst->parse_string_document($pod);
close $out;

sub menuToMD {
    my ($menu) = @_;
    my $index = 0;
    return "| Index | Identifier | Choice String |",
           "| ----- | ---------- | ------------- |",
        map({choiceTableRow($_, $index++)} $menu->choices);
}

sub choiceTableRow {
    my ($ch, $index) = @_;
    my ($id, $name) = @{$ch};
    return "| $index | $id | $name |";
}

sub rtypeToMD {
    my ($rtyp, $dbd) = @_;
    return map {
        # Handle a 'fields' Pod directive
        if (m/^ =fields \s+ (\w+ (?:\s* , \s* \w+ )* )/x) {
            my @names = split /\s*,\s*/, $1;
            # Look up the named fields
            my @fields = map {
                    my $field = $rtyp->field($_);
                    die "Unknown field name '$_' in $infile POD\n"
                        unless $field;
                    $field;
                } @names;
            # Generate Pod for the table
            "| Field | Summary | Type | DCT | Default | Read | Write | CA PP |",
            "| ----- | ------- | ---- | --- | ------- | ---- | ----- | ----- |",
            map({fieldTableRow($_, $dbd)} @fields);
        }
        # Handle a 'menu' Pod directive
        elsif (m/^ =menu \s+ (\w+) /x) {
            my $mn = $1;
            my $menu = $dbd->menu($mn);
            die "Unknown menu '$mn' in $infile POD directive\n"
                unless $menu;
            menuToMD($menu);
        }
        else {
            # Raw text line
            $_;
        }
    } $rtyp->pod;
}

sub fieldTableRow {
    my ($fld, $dbd) = @_;
    my @md;
    push @md, $fld->name, $fld->attribute('prompt');

    my $type = $fld->public_type;
    if ($type eq 'STRING') {
        $type .= ' [' . $fld->attribute('size') . ']';
    } elsif ($type eq 'MENU') {
        my $mn = $fld->attribute('menu');
        my $menu = $dbd->menu($mn);
        my $mnl = lc($mn);
        my $url = $menu ? "/menu-$mnl" : "${mn}.md";
        #just pass a L directive for the parser
        $type .= " L<$mn|$url>";
    }
    push @md, $type;

    push @md, $fld->attribute('promptgroup') ? 'Yes' : 'No';
    push @md, $fld->attribute('initial') || ' ';
    push @md, $fld->readable;
    push @md, $fld->writable;
    push @md, $fld->attribute('pp') eq 'TRUE' ? 'Yes' : 'No';
    return '| ' . join(' | ', @md) . ' |';
}

# Native type presented to dbAccess users
sub DBD::Recfield::public_type {
    my $fld = shift;
    m/^ =type \s+ (.+) /x && return $1 for $fld->comments;
    my $type = $fld->dbf_type;
    $type =~ s/^DBF_//;
    return $type;
}

# Check if this field is readable
sub DBD::Recfield::readable {
    my $fld = shift;
    m/^ =read \s+ (?i) (Yes|No) /x && return $1 for $fld->comments;
    return 'Probably'
        if $fld->attribute('special') eq "SPC_DBADDR";
    return $fld->dbf_type eq 'DBF_NOACCESS' ? 'No' : 'Yes';
}

# Check if this field is writable
sub DBD::Recfield::writable {
    my $fld = shift;
    m/^ =write \s+ (?i) (Yes|No) /x && return $1 for $fld->comments;
    my $special = $fld->attribute('special');
    return 'No'
        if $special eq "SPC_NOMOD";
    return 'Maybe'
        if $special eq "SPC_DBADDR";
    return $fld->dbf_type eq "DBF_NOACCESS" ? 'No' : 'Yes';
}

=pod

=head1 Converting Wiki Record Reference to POD

If you open the src/std/rec/aiRecord.dbd.pod file in your favourite plain text
editor you'll see what input was required to generate the aiRecord.html file.
The text markup language we're using is a standard called POD (Plain Old
Documentation) which is used by Perl developers, but you don't need to know Perl
at all to be able to use it.

When we add POD markup to a record type, we rename its *Record.dbd file to
.dbd.pod in the src/std/rec directory; no other changes are needed for the build
system to find it by its new name. The POD content is effectively just a new
kind of comment that appears in .dbd.pod files, which the formatter knows how to
convert into Markdown. The build also generates a plain *Record.dbd file from this
same input file by stripping out all of the POD markup.

Documentation for Perl's POD markup standard can be found online at
L<https://perldoc.perl.org/perlpod.html> or you may be able to type 'perldoc
perlpod' into a Linux command-line to see the same text. We added a few POD
keywords of our own to handle the table generation, and I'll cover those briefly
below.

POD text can appear almost anywhere in a dbd.pod file. It always starts with a
line "=[keyword] [additional text...]" where [keyword] is "title", "head1"
through "head4" etc.. The POD text ends with a line "=cut". There must be a
blank line above every POD line, and in many cases below it as well.

The POD keywords we have added are "title", "recordtype", "menu", "fields",
"type", "read" and "write". The last 3 are less common but are used in some of
the other record types such as the waveform and aSub records.

The most interesting of our new keywords is "fields", which takes a list of
record field names on the same line after the keyword and generates an Markdown
Table describing those fields based on the field description found in the DBD
parts. In the ai documentation the first such table covers the DTYP and INP
fields.

Note that the "=fields" line must appear inside the DBD's declaration of the
record type, i.e. after the line

    recordtype(ai) {

The "type", "read" and "write" POD keywords are used inside an individual record
field declaration and provide information for the "Type", "Read" and "Write"
columns of the field's table output for fields where this information is
normally supplied by the record support code. Usage examples for these keywords
can be found in the aai and aSub record types.

If you look at the L<aoRecord.dbd.pod> file you'll see that the POD there starts
by documenting a record-specific menu definition. The "menu" keyword generates a
table that lists all the choices found in the named menu. Any MENU fields in the
field tables that refer to a locally-defined menu will generate a link to a
document section which must be titled "Menu [menuName]".

=cut
