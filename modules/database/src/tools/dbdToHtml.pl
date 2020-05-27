#!/usr/bin/env perl

#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
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
use EPICS::Readfile;

BEGIN {
    $::XHTML = eval "require EPICS::PodXHtml; 1";
    if (!$::XHTML) {
        require EPICS::PodHtml;
    }
}

use Pod::Usage;

=head1 NAME

dbdToHtml.pl - Convert DBD file with POD to HTML

=head1 SYNOPSIS

B<dbdToHtml.pl> [B<-h>] [B<-D>] [B<-I> dir] [B<-o> file] file.dbd.pod

=head1 DESCRIPTION

Generates HTML documentation from a B<.dbd.pod> file.

=head1 OPTIONS

B<dbdToHtml.pl> understands the following options:

=over 4

=item B<-h>

Help, display usage information.

=item B<-H>

Conversion help, display information about converting reference documentation
from the EPICS Wiki into a B<.dbd.pod> file for use with this tool.

=item B<-D>

Instead of creating the output file as described, read the input file(s) and
print a B<Makefile> dependency rule for the output file(s) to stdout.

=item B<-o> file

Name of the output file to be created.

=back

If no output filename is set, the file created will be named after the input
file, removing any directory components in the path and replacing any
B<.dbd.pod> file extension with B<.html>.

=cut

our ($opt_h, $opt_H, $opt_D, @opt_I, $opt_o);

my $tool = 'dbdToHtml.pl';

getopts('hHDI@o:') or
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
    ($opt_o = $infile) =~ s/\.dbd\.pod$/.html/;
    $opt_o =~ s/^.*\///;
    $opt_o =~ s/dbCommonRecord/dbCommon/;
}

if ($opt_D) {   # Output dependencies only
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
    exit 0;
}

(my $title = $opt_o) =~ s/\.html$//;

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

$SIG{__DIE__} = sub {
    die @_ if $^S;  # Ignore eval deaths
    close $out;
    unlink $opt_o;
};

my $podHtml;
my $idify;
my $contentType =
    '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" >';

if ($::XHTML) {
    $podHtml = EPICS::PodXHtml->new();
    $podHtml->html_doctype(<< '__END_DOCTYPE');
<?xml version='1.0' encoding='UTF-8'?>
<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN'
     'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>
__END_DOCTYPE
    if ($podHtml->can('html_charset')) {
        $podHtml->html_charset('UTF-8');
    }
    else {
        # Older version of Pod::Simple::XHTML without html_charset()
        $podHtml->html_header_tags($contentType);
    }
    $podHtml->html_header_tags($podHtml->html_header_tags .
        "\n<link rel='stylesheet' href='style.css' type='text/css'>");

    $idify = sub {
        my $title = shift;
        return $podHtml->idify($title, 1);
    }
} else { # Fall back to HTML
    $Pod::Simple::HTML::Content_decl = $contentType;
    $podHtml = EPICS::PodHtml->new();
    $podHtml->html_css('style.css');

    $idify = sub {
        my $title = shift;
        return Pod::Simple::HTML::esc($podHtml->section_escape($title));
    }
}

# Parse the Pod text from the root DBD object
my $pod = join "\n", '=for html <div class="pod">', '',
    map {
        # Handle a 'recordtype' Pod directive
        if (m/^ =recordtype \s+ (\w+) /x) {
            my $rn = $1;
            my $rtyp = $dbd->recordtype($rn);
            die "Unknown recordtype '$rn' in $infile POD directive\n"
                unless $rtyp;
            rtypeToPod($rtyp, $dbd);
        }
        # Handle a 'menu' Pod directive
        elsif (m/^ =menu \s+ (\w+) /x) {
            my $mn = $1;
            my $menu = $dbd->menu($mn);
            die "Unknown menu '$mn' in $infile POD directive\n"
                unless $menu;
            menuToPod($menu);
        }
        elsif (m/^ =title \s+ (.*)/x) {
            $title = $1;
            "=head1 $title";
        }
        else {
            $_;
        }
    } $dbd->pod,
    '=for html </div>', '';

$podHtml->force_title($podHtml->encode_entities($title));
$podHtml->perldoc_url_prefix('');
$podHtml->perldoc_url_postfix('.html');
$podHtml->output_fh($out);
$podHtml->parse_string_document($pod);
close $out;


sub menuToPod {
    my ($menu) = @_;
    my $index = 0;
    return '=begin html', '', '<blockquote><table border="1"><tr>',
        '<th>Index</th><th>Identifier</th><th>Choice String</th></tr>',
        map({choiceTableRow($_, $index++)} $menu->choices),
        '</table></blockquote>', '', '=end html';
}

sub choiceTableRow {
    my ($ch, $index) = @_;
    my ($id, $name) = @{$ch};
    return '<tr>',
        "<td class='cell DBD_Menu index'>$index</td>",
        "<td class='cell DBD_Menu identifier'>$id</td>",
        "<td class='cell DBD_Menu choice'>$name</td>",
        '</tr>';
}

sub rtypeToPod {
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
            '=begin html', '', '<blockquote><table border="1"><tr>',
            '<th>Field</th><th>Summary</th><th>Type</th><th>DCT</th>',
            '<th>Default</th><th>Read</th><th>Write</th><th>CA PP</th>',
            '</tr>',
            map({fieldTableRow($_, $dbd)} @fields),
            '</table></blockquote>', '', '=end html';
        }
        # Handle a 'menu' Pod directive
        elsif (m/^ =menu \s+ (\w+) /x) {
            my $mn = $1;
            my $menu = $dbd->menu($mn);
            die "Unknown menu '$mn' in $infile POD directive\n"
                unless $menu;
            menuToPod($menu);
        }
        else {
            # Raw text line
            $_;
        }
    } $rtyp->pod;
}

sub fieldTableRow {
    my ($fld, $dbd) = @_;
    my $html = '<tr><td class="cell">';
    $html .= $fld->name;
    $html .= '</td><td class="cell">';
    $html .= $fld->attribute('prompt');
    $html .= '</td><td class="cell">';
    my $type = $fld->public_type;
    $html .= $type;
    $html .= ' [' . $fld->attribute('size') . ']'
        if $type eq 'STRING';
    if ($type eq 'MENU') {
        my $mn = $fld->attribute('menu');
        my $menu = $dbd->menu($mn);
        my $url = $menu ? '#' . &$idify("Menu $mn") : "${mn}.html";
        $html .= " (<a href='$url'>$mn</a>)";
    }
    $html .= '</td><td class="cell">';
    $html .= $fld->attribute('promptgroup') ? 'Yes' : 'No';
    $html .= '</td><td class="cell">';
    $html .= $fld->attribute('initial') || '&nbsp;';
    $html .= '</td><td class="cell">';
    $html .= $fld->readable;
    $html .= '</td><td class="cell">';
    $html .= $fld->writable;
    $html .= '</td><td class="cell">';
    $html .= $fld->attribute('pp') eq 'TRUE' ? 'Yes' : 'No';
    $html .= "</td></tr>\n";
    return $html;
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
convert into HTML. The build also generates a plain *Record.dbd file from this
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
record field names on the same line after the keyword and generates an HTML
Table describing those fields based on the field description found in the DBD
parts. In the ai documentation the first such table covers the DTYP and INP
fields, so the line

    =fields DTYP, INP

generates all this in the output:

    <blockquote><table border="1">
    <tr>
    <th>Field</th><th>Summary</th><th>Type</th><th>DCT</th>
    <th>Default</th><th>Read</th><th>Write</th><th>CA PP</th>
    </tr>
    <tr>
    <td class="cell">DTYP</td><td class="cell">Device Type</td>
    <td class="cell">DEVICE</td>
    <td class="cell">Yes</td>
    <td class="cell">&nbsp;</td>
    <td class="cell">Yes</td>
    <td class="cell">Yes</td>
    <td class="cell">No</td>
    </tr>
    <tr>
    <td class="cell">INP</td>
    <td class="cell">Input Specification</td>
    <td class="cell">INLINK</td>
    <td class="cell">Yes</td>
    <td class="cell">&nbsp;</td>
    <td class="cell">Yes</td>
    <td class="cell">Yes</td>
    <td class="cell">No</td>
    </tr>
    </table></blockquote>

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
