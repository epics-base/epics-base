#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2022 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

#######################################################################
#
#    capr: A program that attempts to do a "dbpr" command via channel
#    access.
#
#######################################################################

use 5.10.1;
use strict;

use FindBin qw($RealBin);
use lib ("$RealBin/../../lib/perl");

use Getopt::Std;
use Cwd 'abs_path';
use CA;

######### Globals ##########

our ($opt_h, $opt_f, $opt_l, $opt_r, $opt_D);
our $opt_d = $ENV{EPICS_CAPR_DBD_FILE} // abs_path("$RealBin/../../dbd/softIoc.dbd");
our $opt_w = 2;
our $opt_n = 10;

my %record = ();    # Empty hash to put dbd data in

# EPICS field types
my %fieldType = (
    DBF_CHAR     => 'DBF_CHAR',
    DBF_UCHAR    => 'DBF_CHAR',
    DBF_DOUBLE   => 'DBF_FLOAT',
    DBF_FLOAT    => 'DBF_FLOAT',
    DBF_LONG     => 'DBF_LONG',
    DBF_INT64    => 'DBF_FLOAT',
    DBF_SHORT    => 'DBF_LONG',
    DBF_ULONG    => 'DBF_LONG',
    DBF_USHORT   => 'DBF_LONG',
    DBF_UINT64   => 'DBF_FLOAT',
    DBF_DEVICE   => 'DBF_STRING',
    DBF_ENUM     => 'DBF_STRING',
    DBF_FWDLINK  => 'DBF_STRING',
    DBF_INLINK   => 'DBF_STRING',
    DBF_MENU     => 'DBF_STRING',
    DBF_OUTLINK  => 'DBF_STRING',
    DBF_STRING   => 'DBF_STRING',
    DBF_NOACCESS => 'DBF_NOACCESS',
);

# globals for sub caget
my %connected;
my $unconnected_count;
my %callback_data;
my $callback_incomplete;

######### Main program ############

HELP_MESSAGE() unless getopts('hDd:f:l:n:rw:');
HELP_MESSAGE() if $opt_h;

die "File $opt_d not found. (\"capr.pl -h\" gives help)\n"
    unless -f $opt_d;

parseDbd($opt_d);
print "Using $opt_d\n\n";

# Print a list of record types
if ($opt_r) {
    print ("Record types found:\n");
    printList($opt_l);
    exit;
}

# Print the fields defined for given record
if ($opt_f) {
    printRecordList($opt_f);
    exit;
}

HELP_MESSAGE() unless @ARGV;

$_ = shift;
if (@ARGV) {
    # Drop any ".FIELD" part
    s/\. \w+ $//x;
    printRecord($_, @ARGV);
} else {
    if (m/^ \s* ([]+:;<>0-9A-Za-z[-]+) (?:\. \w+)? \s* , \s* (\d+) \s* $/x) {
        # Recognizes ",n" as an interest level, drops any ".FIELD" part
        printRecord($1, $2);
    } else {
        # Drop any ".FIELD" part
        s/\. \w+ $//x;
        printRecord($_, $opt_l);
    }
}

########## End of main ###########



# parseDbd
# Takes given dbd file and parses it to produce a hash table of record types
# giving their fields, and for each field its interest level and data type.
# usage: parseDbd("fileName");
# Output goes to the global %record, a hash of references to other hashes
# containing the fields of each record type. Those hash values (keyed by
# field name) are references to arrays containing the interest level, data
# type and number base of the field.
sub parseDbd {
    my $dbdFile = shift;

    open(DBD, "< $dbdFile") or die "Can't open file $dbdFile: $!\n";
    my @dbd = <DBD>;
    close(DBD) or die "Can't close $dbdFile: $!\n";

    my $i = 1;
    my $level = 0;
    my $isArecord = 0;
    my $isAfield = 0;
    my $thisRecord;
    my $thisField;
    my $thisType;
    my $thisSize = 0;
    my $field = {};
    my $interest = 0;
    my $thisBase = 'DECIMAL';
    my $special = '';

    while (@dbd) {
        $_ = shift @dbd;
        chomp;
        if ( m/recordtype \s* \( \s* (\w+) \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 0;
            $isArecord = 1;
            $thisRecord = $1;
        }
        elsif ( m/field \s* \( \s* (\w+) \s* , \s* (\w+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 1 && $isArecord;
            $thisField = $1;
            $thisType = $2;
            $isAfield = 1;
        }
        elsif ( m/interest \s* \( \s* (\w+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 2 && $isAfield;
            $interest = $1;
        }
        elsif ( m/size \s* \( \s* ([0-9]+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 2 && $isAfield;
            $thisSize = $1;
        }
        elsif ( m/special \s* \( \s* (\w+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 2 && $isAfield;
            $special = $1;
        }
        elsif ( m/base \s* \( \s* (\w+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 2 && $isAfield;
            $thisBase = $1;
        }
        if ( m/\{/ ) {
            $level++;
        }
        if ( m/\}/ ) {
            if ($level == 2 && $isAfield) {
                my $params = {};
                $params->{interest} = $interest;
                $params->{dbfType} = $thisType;
                $params->{base} = $thisBase;
                $params->{special} = $special;
                $params->{size} = $thisSize;
                $field->{$thisField} = $params;

                # Reset default values
                $isAfield = 0;
                $interest = 0;
                $thisBase = 'DECIMAL';
                $special = '';
                $thisSize = 0;
            }
            elsif ($level == 1 && $isArecord) {
                $isArecord = 0;
                $record{$thisRecord} = $field;
                $field = {};                        # start another hash
            }
            $level--;
        }
        $i++;
    }
}


# Given a record name, attempts to find the record and its type.
# Usage: $recordType = getRecType("recordName");
sub getRecType {
    my $arg = shift;
    my $name = "$arg.RTYP";

    my $fields_read = caget($name);

    die "Could not determine record type of $arg\n"
        unless $fields_read == 1;

    return $callback_data{$name};
}

# Given the record type and field, returns the interest level, data type
# and number base for the field
# Usage: ($dataType, $interest, $base, $special, $size) = getFieldParams($recType, $field);
sub getFieldParams {
    my ($recType, $field) = @_;

    my $params = $record{$recType}{$field} or
        die "Can't find params for $recType.$field";
    exists($fieldType{$params->{dbfType}})  ||
        die "Field data type $field for $recType not found in dbd file --";
    exists($params->{interest})  ||
        die "Interest level for $field in $recType not found in dbd file --";

    my $fType     = $fieldType{$params->{dbfType}};
    my $fInterest = $params->{interest};
    my $fBase     = $params->{base};
    my $fSpecial  = $params->{special};
    my $fSize     = $params->{size};
    return ($fType, $fInterest, $fBase, $fSpecial, $fSize);
}

# Prints field name and data for given field. Formats output so
# that fields align in to 4 columns. Tries to imitate dbpf format
# Usage: printField( $fieldName, $data, $dataType, $base, $firstColumnPosn)
sub printField {
    my ($fieldName, $fieldData, $dataType, $base, $col) = @_;

    my $screenWidth  = 80;
    my ($outStr, $wide);


    if (ref $fieldData eq 'ARRAY') {
        my $elems = scalar @{$fieldData};
        my $field = "$fieldName\[$elems\]:";
        my $count = $elems > $opt_n ? $opt_n : $elems;
        my @show = @{$fieldData}[0 .. $count - 1];
        $outStr = sprintf('%-5s %s', $field, join(', ', @show));
        $outStr .= ", ..." if $elems > $count;
    }
    else {
        my $field = "$fieldName:";
        if ( $dataType eq 'DBF_STRING' ) {
            $outStr = sprintf('%-5s %s', $field, $fieldData);
        } elsif ( $base eq 'HEX' ) {
             my $val = ( $dataType eq 'DBF_CHAR' ) ? ord($fieldData) : $fieldData;
             $outStr = sprintf('%-5s 0x%x', $field, $val);
        } elsif ( $dataType eq 'DBF_DOUBLE' || $dataType eq 'DBF_FLOAT' ) {
            $outStr = sprintf('%-5s %.8f', $field, $fieldData);
        } elsif ( $dataType eq 'DBF_UCHAR' ) {
            $outStr = sprintf('%-5s %d', $field, ord($fieldData));
        } else {
            # DBF_INT64, DBF_LONG, DBF_SHORT,
            # DBF_UINT64, DBF_ULONG, DBF_USHORT, DBF_UCHAR,
            $outStr = sprintf('%-5s %d', $field, $fieldData);
        }
    }

    my $len = length($outStr);
    if ($len < 20) { $wide = 20; }
    elsif ( $len < 40 ) { $wide = 40; }
    elsif ( $len < 60 ) { $wide = 60; }
    else { $wide = 80;}

    my $pad = $wide - $len;

    $col += $wide;
    if ($col > $screenWidth ) {
        print("\n");
        $col = $wide;
    }

    print $outStr, ' ' x $pad;

    return $col;
}

#  Query the native values of a list of PVs simultaneously.
#  The data is returned in the the %callback_data global hash.
#  The return value is the number of read pvs
#
#  NOTE: Not re-entrant because results are written to global hash
#        %callback_data
#
#  Usage: $fields_read = caget( @pvlist )
sub caget {
    #clear any previous results;
    %connected = ();
    $unconnected_count = scalar @_;
    %callback_data = ();
    $callback_incomplete = 0;

    my @chans = map {
        print "  Creating channel for $_\n" if $opt_D;
        CA->new($_, \&canew_callback);
    } @_;
    my $channel_count = scalar @chans;
    return 0 unless $channel_count gt 0;

    print "  $channel_count channels created.\n" if $opt_D;

    my $elapsed = 0;
    do {
        print "  Waiting for $unconnected_count channels to connect\n"
            if $unconnected_count && $opt_D;
        print "  Waiting for data from $callback_incomplete channels\n"
            if $callback_incomplete && $opt_D;
        CA->pend_event(0.1);
        $elapsed += 0.1;
    } until (($elapsed > $opt_w) or
             (scalar %connected && $callback_incomplete == 0));
    my $data_count = scalar keys %callback_data;
    printf "  Got data from %d of %d channels\n", $data_count, $channel_count
        if $opt_D;
    return $data_count;
}

sub canew_callback {
    my ($chan, $up) = @_;
    return unless $up;
    $connected{$chan->name} = $chan;
    $unconnected_count--;
    my $ftype = $chan->field_type;
    my $count = $chan->element_count;
    $ftype = 'DBR_LONG' if $ftype eq 'DBR_CHAR' && $count == 1;
    print "  Getting ${\$chan->name} as $ftype\n" if $opt_D;
    # We have to fetch all elements so we can show how many there are
    $chan->get_callback(\&caget_callback, $ftype);
    $callback_incomplete++;
}

sub caget_callback {
    my ($chan, $status, $data) = @_;
    die $status if $status;
    print "  Got ${\$chan->name} = '$data'\n" if $opt_D;
    $callback_data{$chan->name} = $data;
    $callback_incomplete--;
}

# Given record name and interest level prints data from record fields
# that are at or below the interest level specified.
# Usage: printRecord($recordName, $interestLevel)
sub printRecord {
    my ($name, $interest) = @_;

    my $recType = getRecType($name);
    print("Record $name  type $recType\n");
    die "Record type $recType not found\n"
        unless exists $record{$recType};

    #capture list of fields
    my @readlist = ();  #fields to read via CA
    my @fields_pr = (); #fields for print-out
    my @ftypes = ();    #types, from parser
    my @bases = ();     #bases, from parser
    foreach my $field (sort keys %{$record{$recType}}) {
        my ($fType, $fInterest, $base, $special, $size) =
            getFieldParams($recType, $field);
        next if $fInterest > $interest;
        my $fToGet = "$name.$field";
        if ($fType eq 'DBF_NOACCESS') {
            next unless $special eq 'SPC_DBADDR';
            $fType = 'DBF_STRING';
        }
        elsif ($fType eq 'DBF_STRING' && $size >= 40) {
            $fToGet .= '$';
        }
        push @fields_pr, $field;
        push @readlist, $fToGet;
        push @ftypes, $fType;
        push @bases, $base;
    }
    my $fields_read = caget( @readlist );

    my @missing;

    # print while iterating over lists gathered
    my $col = 0;
    for (my $i=0; $i < scalar @readlist; $i++) {
        my $field  = $fields_pr[$i];
        my $fToGet = $readlist[$i];
        my ($fType, $data, $base);
        push @missing, $field unless exists $callback_data{$fToGet};
        next unless exists $callback_data{$fToGet};
        $fType  = $ftypes[$i];
        $base   = $bases[$i];
        $data   = $callback_data{$fToGet};
        $col = printField($field, $data, $fType, $base, $col);
    }
    print("\n");  # Final newline

    printf "\nUnreadable fields: %s\n", join(', ', @missing) if @missing && $opt_D;
}

# Prints list of record types found in dbd file. If level > 0
# then uses printRecordList to display all fields in each record type.
sub printList {
    my $level = shift;

    foreach my $rkey (sort keys(%record)) {
        if ($level > 0) {
            printRecordList($rkey);
        }
        else {
            print("  $rkey\n");
        }
    }
}

# Prints list of fields and metadata for given record type
sub printRecordList {
    my $type = shift;

    if (exists($record{$type}) ) {
        print("Record type - $type\n");
        foreach my $fkey (sort keys %{$record{$type}}) {
            my $param = $record{$type}{$fkey};
            my $dbfType = $param->{dbfType};
            printf "  %-8s", $fkey;
            printf "  interest = %d", $param->{interest};
            printf "  type = %-12s", $dbfType;
            if ($dbfType eq 'DBF_STRING') {
                printf "    size = %s\n", $param->{size};
            }
            elsif ($dbfType =~ m/DBF_U?(CHAR|SHORT|INT|LONG|INT64)/) {
                printf "    base = %s\n", $param->{base};
            }
            elsif ($dbfType eq 'DBF_NOACCESS') {
                printf "    special = %s\n", $param->{special};
            }
            else {
                print "\n";
            }
        }
    }
    else {
        print("Record type $type not defined in dbd file $opt_d\n");
    }
}

sub HELP_MESSAGE {
    print STDERR "\n",
        "Usage: capr.pl -h\n",
        "       capr.pl [options] -r\n",
        "       capr.pl [options] -f <record type>\n",
        "       capr.pl [options] <record name> [<interest>]\n",
        "\n",
        "  -h Print this help message.\n",
        "  -D Print debug messages.\n",
        "Channel Access options:\n",
        "  -w <sec>:  Wait time, specifies CA timeout, default is $opt_w second\n",
        "Database Definitions:\n",
        "  -d <file.dbd>: The file containing record type definitions.\n",
        "     This can be set using the EPICS_CAPR_DBD_FILE environment variable.\n",
        "     Default: ", abs_path($opt_d), "\n",
        "Output Options:\n",
        "  -r Lists all record types in the selected dbd file.\n",
        "  -f <record type>: Lists all fields with interest level, data type\n",
        "     and other information for the given record_type.\n",
        "  -l <interest>: interest level\n",
        "  -n <elems>: Maximum number of array elements to display\n",
        "     Default: $opt_n\n",
        "\n",
        "Base version: ", CA->version, "\n";
    exit 1;
}
