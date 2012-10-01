#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use EPICS::Getopts;
use EPICS::macLib;
use EPICS::Readfile;
use Pod::Simple::HTML;

my $tool = 'dbdToHtml';

use vars qw($opt_D @opt_I $opt_o);
getopts('DI@o:') or
    die "Usage: $tool [-D] [-I dir] [-o file.html] file.dbd\n";

my $dbd = DBD->new();

my $infile = shift @ARGV;
$infile =~ m/\.dbd$/ or
    die "$tool: Input file '$infile' must have '.dbd' extension\n";

&ParseDBD($dbd, &Readfile($infile, 0, \@opt_I));

if (!$opt_o) {
    ($opt_o = $infile) =~ s/\.dbd$/.html/;
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

open my $out, '>', $opt_o or
    die "Can't create $opt_o: $!\n";

# Parse the Pod text from the root DBD object
my @pod = map {
    # Handle a 'recordtype' Pod directive
    if (m/^ =recordtype \s+ (.*)/x) {
        my $rn = $1;
        my $rtyp = $dbd->recordtype($rn);
        die "Unknown recordtype '$rn' in $infile POD directive\n"
            unless $rtyp;
        rtypeToPod($rtyp, $dbd);
    }
    # Handle a 'menu' Pod directive
    elsif (m/^ =menu \s+ (.*)/x) {
        my $mn = $1;
        my $menu = $dbd->menu($mn);
        die "Unknown menu '$mn' in $infile POD directive\n"
            unless $menu;
        menuToPod($menu);
    }
    else {
        $_;
    }
} $dbd->pod;

my $podHtml = Pod::Simple::HTML->new();
$podHtml->html_css('style.css');
$podHtml->perldoc_url_prefix('');
$podHtml->perldoc_url_postfix('.html');
$podHtml->set_source(\@pod);
# $podHtml->index(1);
$podHtml->output_string(\my $html);
$podHtml->run;
print $out $html;
close $out;


sub menuToPod {
    my ($menu) = @_;
    my $index = 0;
    return "=begin html\n", "\n",
        "<blockquote><table border =\"1\"><tr>\n",
        "<th>Index</th><th>Identifier</th><th>Choice String</th>\n",
        "</tr>\n",
        map({choiceTableRow($_, $index++)} $menu->choices),
        "</table></blockquote>\n",
        "\n", "=end html\n";
}

sub choiceTableRow {
    my ($ch, $index) = @_;
    my ($id, $name) = @{$ch};
    return '<tr><td class="cell">',
        $index,
        '</td><td class="cell">',
        $id,
        '</td><td class="cell">',
        $name,
        "</td></tr>\n";
}

sub rtypeToPod {
    my ($rtyp, $dbd) = @_;
    return map {
        # Handle a 'fields' Pod directive
        if (m/^ =fields \s+ (.*)/x) {
            my @names = split /\s*,\s*/, $1;
            # Look up the named fields
            my @fields = map {
                    my $field = $rtyp->field($_);
                    die "Unknown field name '$_' in $infile POD\n"
                        unless $field;
                    $field;
                } @names;
            # Generate Pod for the table
            "=begin html\n", "\n",
            "<blockquote><table border =\"1\"><tr>\n",
            "<th>Field</th><th>Summary</th><th>Type</th><th>DCT</th>",
            "<th>Default</th><th>Read</th><th>Write</th><th>CA PP</th>",
            "</tr>\n",
            map({fieldTableRow($_, $dbd)} @fields),
            "</table></blockquote>\n",
            "\n", "=end html\n";
        }
        # Handle a 'menu' Pod directive
        elsif (m/^ =menu \s+ (.*)/x) {
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
        my $url = $menu ? "#Menu_$mn" : "${mn}.html";
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
    $html .= $fld->attribute('pp') eq "TRUE" ? 'Yes' : 'No';
    $html .= "</td></tr>\n";
    return $html;
}

# Native type presented to dbAccess users
sub DBD::Recfield::public_type {
    my $fld = shift;
    m/^=type (.+)$/i && return $1 for $fld->comments;
    my $type = $fld->dbf_type;
    $type =~ s/^DBF_//;
    return $type;
}

# Check if this field is readable
sub DBD::Recfield::readable {
    my $fld = shift;
    m/^=read (Yes|No)$/i && return $1 for $fld->comments;
    return 'Probably'
        if $fld->attribute('special') eq "SPC_DBADDR";
    return $fld->dbf_type eq 'DBF_NOACCESS' ? 'No' : 'Yes';
}

# Check if this field is writable
sub DBD::Recfield::writable {
    my $fld = shift;
    m/^=write (Yes|No)$/i && return $1 for $fld->comments;
    my $special = $fld->attribute('special');
    return 'No'
        if $special eq "SPC_NOMOD";
    return 'Maybe'
        if $special eq "SPC_DBADDR";
    return $fld->dbf_type eq "DBF_NOACCESS" ? 'No' : 'Yes';
}

