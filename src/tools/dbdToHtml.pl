#!/usr/bin/perl

#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use DBD;
use DBD::Parser;
use EPICS::Getopts;
use EPICS::macLib;
use EPICS::Readfile;

my $tool = 'dbdToHtml';
getopts('DI@o:') or
    die "Usage: $tool [-D] [-I dir] [-o xRecord.html] xRecord.dbd\n";

my @path = map { split /[:;]/ } @opt_I;
my $dbd = DBD->new();

my $infile = shift @ARGV;
$infile =~ m/\.dbd$/ or
    die "$tool: Input file '$infile' must have '.dbd' extension\n";

&ParseDBD($dbd, &Readfile($infile, 0, \@opt_I));

if ($opt_D) {   # Output dependencies only
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$opt_o: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
    exit 0;
}

my $out;
if ($opt_o) {
    $out = $opt_o;
} else {
    ($out = $infile) =~ s/\.dbd$/.html/;
    $out =~ s/^.*\///;
    $out =~ s/dbCommonRecord/dbCommon/;
}
open $out, '>', $opt_o or die "Can't create $opt_o: $!\n";

print $out "<h1>$infile</h1>\n";

my $rtypes = $dbd->recordtypes;

my ($rn, $rtyp) = each %{$rtypes};
print $out "<h2>Record Name $rn</h2>\n";

my @fields = $rtyp->fields;

#create a Hash to store the table of field information for each GUI type
%dbdTables = (
    "GUI_COMMON" => "",
    "GUI_COMMON" => "",
    "GUI_ALARMS" => "",
    "GUI_BITS1" => "",
    "GUI_BITS2" => "",
    "GUI_CALC" => "",
    "GUI_CLOCK" => "",
    "GUI_COMPRESS" => "",
    "GUI_CONVERT" => "",
    "GUI_DISPLAY" => "",
    "GUI_HIST" => "",
    "GUI_INPUTS" => "",
    "GUI_LINKS" => "",
    "GUI_MBB" => "",
    "GUI_MOTOR" => "",
    "GUI_OUTPUT" => "",
    "GUI_PID" => "",
    "GUI_PULSE" => "",
    "GUI_SELECT" => "",
    "GUI_SEQ1" => "",
    "GUI_SEQ2" => "",
    "GUI_SEQ3" => "",
    "GUI_SUB" => "",
    "GUI_TIMER" => "",
    "GUI_WAVE" => "",
    "GUI_SCAN" => "",
    "GUI_NONE" => ""
);


#Loop over all of the fields.  Build a string that contains the table body
#for each of the GUI Types based on which fields go with which GUI type.
foreach $fVal (@fields) {
    my $pg = $fVal->attribute('promptgroup');
    while ( ($typ1, $content) = each %dbdTables) {
        if ( $pg eq $typ1 or ($pg eq "" and $typ1 eq "GUI_NONE")) {
            buildTableRow($fVal, $dbdTables{$typ1} );
        }
    }
}

#Write out each table
while ( ($typ2, $content) = each %dbdTables) {
    printHtmlTable($typ2, $content);
}


#add a field to a table body.  The specified field and table body are passed 
#in as parameters
sub buildTableRow {
    my ( $fld, $outStr) = @_;
    $longDesc = "&nbsp;";
    %htmlCellFmt = (
        rowStart => "<tr><td rowspan = \"2\">",
        nextCell => "</td><td>",
        endRow   => "</td></tr>",
        nextRow  => "<tr><td colspan = \"7\" align=left>"
        );
    my %cellFmt = %htmlCellFmt;
    my $rowStart = $cellFmt{rowStart};
    my $nextCell = $cellFmt{nextCell};
    my $endRow   = $cellFmt{endRow};
    my $nextRow  = $cellFmt{nextRow};
    $outStr = $outStr . $rowStart;
    $outStr = $outStr . $fld->name;
    $outStr = $outStr . $nextCell;
    $outStr = $outStr . $fld->attribute('prompt');
    $outStr = $outStr . $nextCell;
    my $recType = $fld->dbf_type;
    $typStr = $recType;
    if ($recType eq "DBF_STRING") {
        $typStr = $recType . " [" . $fld->attribute('size') . "]";
    }
    
    $outStr = $outStr . $typStr;
    $outStr = $outStr . $nextCell;
    $outStr = $outStr . design($fld);
    $outStr = $outStr . $nextCell;
    my $initial = $fld->attribute('initial');
    if ( $initial eq '' ) {$initial = "&nbsp;";}
    $outStr = $outStr . $initial;
    $outStr = $outStr . $nextCell;
    $outStr = $outStr . readable($fld);
    $outStr = $outStr . $nextCell;
    $outStr = $outStr . writable($fld);
    $outStr = $outStr . $nextCell;
    $outStr = $outStr . processPassive($fld);
    $outStr = $outStr . $endRow;
    $outStr = $outStr . "\n";
    $outStr = $outStr . $nextRow;
    $outStr = $outStr . $longDesc;
    $outStr = $outStr . $endRow;
    $outStr = $outStr . "\n";
    $_[1] = $outStr;
}

#Check if the prompt group is defined so that this can be used by clients
sub design {
    my $fld = $_[0];
    my $pg = $fld->attribute('promptgroup');
    if ( $pg eq '' ) {
        my $result = 'No';
    }
    else {
        my $result = 'Yes';
    } 
}

#Check if this field is readable by clients
sub readable {
    my $fld = $_[0];
    if ( $fld->attribute('special') eq "SPC_DBADDR") {
        $return = "Probably";
    }
    else{
        if ( $fld->dbf_type eq "DBF_NOACCESS" ) {
            $return = "No";
        }
        else {
            $return = "Yes"
        }
    }
}

#Check if this field is writable by clients
sub writable {
    my $fld = $_[0];
    my $spec = $fld->attribute('special');
    if ( $spec eq "SPC_NOMOD" ) {
        $return = "No";
    }
    else {
        if ( $spec ne "SPC_DBADDR") {
            if ( $fld->dbf_type eq "DBF_NOACCESS" ) {
                $return = "No";
            }
            else {
                $return = "Yes";
            }
        }
        else {
            $return = "Maybe";
        }
    }
}


#Check to see if the field is process passive on caput
sub processPassive {
    my $fld = $_[0];
    $pp = $fld->attribute('pp');
    if ( $pp eq "YES" or $pp eq "TRUE" ) {
        $result = "Yes";
    }
    elsif ( $PP eq "NO" or $pp eq "FALSE"  or $pp eq "" ) {
        $result = "No";
    }
}

#print the start row to define a table
sub printTableStart {
    print $out "<table border =\"1\"> \n";
    print $out "<caption><em>$_[0]</em></caption>";
    print $out "<th>Field</th>\n";
    print $out "<th>Summary</th>\n";
    print $out "<th>Type</th>\n";
    print $out "<th>DCT</th>\n";
    print $out "<th>Default</th>\n";
    print $out "<th>Read</th>\n";
    print $out "<th>Write</th>\n";
    print $out "<th>caPut=PP</th></tr>\n";

}

#print the tail end of the table
sub printTableEnd {
    print $out "</table>\n";
}

# Print the table for a GUI type.  The name of the GUI type and the Table body
# for this type are fed in as parameters
sub printHtmlTable {
    my ($typ2, $content) = $_;
    if ( (length $_[1]) gt 0) {
        printTableStart($_[0]);
        print $out "$_[1]\n";
        printTableEnd();
    }
    
}
