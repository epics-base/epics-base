#!/usr/bin/env perl

#######################################################################
#
#    capr: A program that attempts to do a "dbpr" command via channel
#    access.
#
#######################################################################

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use Getopt::Std;
use EPICS::Path;
use CA;

######### Globals ##########

our ($opt_h, $opt_f, $opt_r);
our $opt_d = $ENV{EPICS_CAPR_DBD_FILE} || "$Bin/../../dbd/softIoc.dbd";
our $opt_w = 1;

my %record = ();    # Empty hash to put dbd data in
my $iIdx = 0;       # Array indexes for interest, data type and base
my $tIdx = 1;
my $bIdx = 2;
my %device = ();    # Empty hash to record which rec types have device support

# EPICS field types
my %fieldType = (
    DBF_CHAR     => 'DBF_CHAR',
    DBF_UCHAR    => 'DBF_CHAR',
    DBF_DOUBLE   => 'DBF_FLOAT',
    DBF_FLOAT    => 'DBF_FLOAT',
    DBF_LONG     => 'DBF_LONG',
    DBF_SHORT    => 'DBF_LONG',
    DBF_ULONG    => 'DBF_LONG',
    DBF_USHORT   => 'DBF_LONG',
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
my %callback_data;
my %timed_out;
my $callback_incomplete;

######### Main program ############

HELP_MESSAGE() unless getopts('hd:f:rw:');
HELP_MESSAGE() if $opt_h;

die "File $opt_d not found. (\"capr.pl -h\" gives help)\n"
    unless -f $opt_d;

parseDbd($opt_d);
print "Using $opt_d\n\n";

# Print a list of record types
if ($opt_r) {
    print ("Record types found:\n");
    printList(0);
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
        # Recognizes ",n" as an interest leve, drops any ".FIELD" part
        printRecord($1, $2);
    } else {
        # Drop any ".FIELD" part
        s/\. \w+ $//x;
        printRecord($_, 0);
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
    my $field = {};
    my $interest = 0;
    my $thisBase = 'DECIMAL';

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
        elsif ( m/base \s* \( \s* (\w+) \s* \)/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 2 && $isAfield;
            $thisBase = $1;
        }
        elsif ( m/device \s* \( (\w+) \s* ,/x ) {
            die "File format error at line $i of file\n    $opt_d\n"
                unless $level == 0;
            $device{$1}++;
        }
        if ( m/\{/ ) {
            $level++;
        }
        if ( m/\}/ ) {
            if ($level == 2 && $isAfield) {
                my $params = [];
                $params->[$iIdx] = $interest;
                $params->[$tIdx] = $thisType;
                $params->[$bIdx] = $thisBase;
                $field->{$thisField} = $params;
                $isAfield = 0;
                $interest = 0;                      # reset default
                $thisBase = 'DECIMAL';              # reset default
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
# Usage: ($dataType, $interest, $base) = getFieldParams($recType, $field);
sub getFieldParams {
    my ($recType, $field) = @_;

    my $params = $record{$recType}{$field} or
        die "Can't find params for $recType.$field";
    exists($fieldType{$params->[$tIdx]})  ||
        die "Field data type $field for $recType not found in dbd file --";
    exists($params->[$iIdx])  ||
        die "Interest level for $field in $recType not found in dbd file --";

    my $fType     = $fieldType{$params->[$tIdx]};
    my $fInterest = $params->[$iIdx];
    my $fBase     = $params->[$bIdx];
    return ($fType, $fInterest, $fBase);
}

# Prints field name and data for given field. Formats output so
# that fields align in to 4 columns. Tries to imitate dbpf format
# Usage: printField( $fieldName, $data, $dataType, $base, $firstColumnPosn)
sub printField {
    my ($fieldName, $fieldData, $dataType, $base, $col) = @_;

    my $screenWidth  = 80;
    my ($outStr, $wide);

    my $field = "$fieldName:";

    if ( $dataType eq 'DBF_STRING' ) {
        $outStr = sprintf('%-5s %s', $field, $fieldData);
    } elsif ( $base eq 'HEX' ) {
         my $val = ( $dataType eq 'DBF_CHAR' ) ? ord($fieldData) : $fieldData;
         $outStr = sprintf('%-5s 0x%x', $field, $val);
    } elsif ( $dataType eq 'DBF_DOUBLE' || $dataType eq 'DBF_FLOAT' ) {
        $outStr = sprintf('%-5s %.8f', $field, $fieldData);
    } elsif ( $dataType eq 'DBF_CHAR' ) {
        $outStr = sprintf('%-5s %d', $field, ord($fieldData));
    }else {
        # DBF_LONG, DBF_SHORT, DBF_UCHAR, DBF_ULONG, DBF_USHORT
        $outStr = sprintf('%-5s %d', $field, $fieldData);
    }

    my $len = length($outStr);
    if ($len <= 20) { $wide = 20; }
    elsif ( $len <= 40 ) { $wide = 40; }
    elsif ( $len <= 60 ) { $wide = 60; }
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

#  Query for a list of fields simultaneously.
#  The results are filled in the the %callback_data global hash
#  and the result of the operation is the number of read pvs
#
#  NOTE: Not re-entrant because results are written to global hash
#        %callback_data
#
#  Usage: $fields_read = caget( @pvlist )
sub caget {
    my @chans = map { CA->new($_); } @_;

    #clear results;
    %callback_data = ();
    %timed_out = ();

    eval { CA->pend_io($opt_w); };
    if ($@) {
        if ($@ =~ m/^ECA_TIMEOUT/) {
            my $err = (@chans > 1) ? 'some PV(s)' : '"' . $chans[0]->name . '"';
            print "Channel connect timed out: $err not found.\n";
            foreach my $chan (@chans) {
                $timed_out{$chan->name} = $chan->is_connected;
            }
            @chans = grep { $_->is_connected } @chans;
        } else {
            die $@;
        }
    }

    map {
        my $type;
        $type = $_->field_type;
        $_->get_callback(\&caget_callback, $type);
    } @chans;

    my $fields_read = @chans;
    $callback_incomplete = @chans;
    CA->pend_event(0.1) while $callback_incomplete;
    return $fields_read;
}

sub caget_callback {
    my ($chan, $status, $data) = @_;
    die $status if $status;
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
        # Skip DTYP field if this rec type doesn't have device support defined
        if ($field eq 'DTYP' && !(exists($device{$recType}))) { next; }

        my ($fType, $fInterest, $base) = getFieldParams($recType, $field);
        unless( $fType eq 'DBF_NOACCESS' ) {
            if ($interest >= $fInterest ) {
                my $fToGet = "$name.$field";
                push @fields_pr, $field;
                push @readlist, $fToGet;
                push @ftypes, $fType;
                push @bases, $base;
            }
        }
    }
    my $fields_read = caget( @readlist );

    # print while iterating over lists gathered
    my $col = 0;
    for (my $i=0; $i < scalar @readlist; $i++) {
        my $field  = $fields_pr[$i];
        my $fToGet = $readlist[$i];
        my ($fType, $data, $base);
        if ($timed_out{$fToGet}) {
            $fType = $fieldType{DBF_STRING};
            $data  = '<timeout>';
        }
        else {
            $fType  = $ftypes[$i];
            $base   = $bases[$i];
            $data   = $callback_data{$fToGet};
        }
        $col = printField($field, $data, $fType, $base, $col);
    }
    print("\n");  # Final newline
}

# Prints list of record types found in dbd file. If level > 0
# then the fields of that record type, their interest levels and types are
# also printed.
# Diagnostic routine, usage: void printList(level);
sub printList {
    my $level = shift;

    foreach my $rkey (sort keys(%record)) {
        print("  $rkey\n");
        if ($level > 0) {
            foreach my $fkey (keys %{$record{$rkey}}) {
                print("\tField $fkey - interest $record{$rkey}{$fkey}[$iIdx] ");
                print("- type $record{$rkey}{$fkey}[$tIdx] ");
                print("- base $record{$rkey}{$fkey}[$bIdx]\n");
            }
        }
    }
}

# Prints list of fields with interest levels for given record type
# Diagnostic routine, usage: void printRecordList("recordType");
sub printRecordList {
    my $type = shift;

    if (exists($record{$type}) ) {
        print("Record type - $type\n");
        foreach my $fkey (sort keys %{$record{$type}}) {
            printf('%-4s', $fkey);
            printf("    interest = $record{$type}{$fkey}[$iIdx]");
            printf("    type = %-12s ",$record{$type}{$fkey}[$tIdx]);
            print ("    base = $record{$type}{$fkey}[$bIdx]\n");
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
        "Channel Access options:\n",
        "  -w <sec>:  Wait time, specifies CA timeout, default is $opt_w second\n",
        "Database Definitions:\n",
        "  -d <file.dbd>: The file containing record type definitions.\n",
        "     This can be set using the EPICS_CAPR_DBD_FILE environment variable.\n",
        "     Default: ", AbsPath($opt_d), "\n",
        "Output Options:\n",
        "  -r Lists all record types in the selected dbd file.\n",
        "  -f <record type>: Lists all fields with their interest level, data type\n",
        "     and number base for the given record_type.\n",
        "\n",
        "Base version: ", CA->version, "\n";
    exit 1;
}
