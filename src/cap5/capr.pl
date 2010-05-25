#!/usr/bin/perl -w

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
use CA;

######### Globals ##########

# Non APS users will want to modify theDbdFile default settings

my $theDbdFile = "/net/helios/iocapps/R3.13.9/ioc/linac/2/dbd/linac.dbd";

my $hostArch;
if ( defined $ENV{"EPICS_HOST_ARCH"} ) {
    $hostArch = $ENV{"EPICS_HOST_ARCH"} ;
} else {
    $hostArch="solaris-sparc";
}

our( $opt_d, $opt_f, $opt_r);
my $usage = "Usage:\n $0 [-d dbd_file] ( [-f record_type] | [-r] | [record_name] ) [interest]\n";
my %record = ();    # Empty hash to put dbd data in
my $iIdx = 0;        # Array indexes for interest, data type and base
my $tIdx = 1;
my $bIdx = 2;
my %device = ();    # Empty hash to record which rec types have device support
my $DEBUG=0;        # DEBUG

# EPICS field types referenced to their equivalent EZCA types
my %fieldType = (
                DBF_STRING      => "ezcaString",
                DBF_BYTE        => "ezcaByte",
                DBF_CHAR        => "ezcaByte",
                DBF_UCHAR       => "ezcaChar",
                DBF_SHORT       => "ezcaShort",
                DBF_USHORT      => "ezcaLong",
                DBF_LONG        => "ezcaLong",
                DBF_ULONG       => "ezcaDouble",
                DBF_FLOAT       => "ezcaFloat",
                DBF_DOUBLE      => "ezcaDouble",
                DBF_ENUM        => "ezcaString",
                DBF_MENU        => "ezcaString",
                DBF_DEVICE      => "ezcaString",
                DBF_INLINK      => "ezcaString",
                DBF_OUTLINK     => "ezcaString",
                DBF_FWDLINK     => "ezcaString",
                DBF_NOACCESS    => "ezcaNoAccess"
);


######### Main program ############

getopts('d:f:r') or die "$usage";

# Select the dbd file to use
if($opt_d) {                            # command line has highest priority
    $theDbdFile = $opt_d;
}
elsif (exists $ENV{CAPR_DBD_FILE}) {    # Use the env var if it exists
    $theDbdFile = $ENV{CAPR_DBD_FILE};
}                                        # Otherwise use the default set above

parseDbd($theDbdFile);
print "Using $theDbdFile\n\n";

# Print a list of record types
if($opt_r) {
    print ("Record types defined in $theDbdFile\n");
    printList(0);
    exit;
}

# Print the fields defined for given record
if($opt_f) {
    printRecordList($opt_f);
    exit;
}

# Do the business
# Allow commas between arguments as in vxWorks dbpr
die "$usage" unless defined $ARGV[0];
$ARGV[0] =~ s/,/ /;                     # Get rid of pesky comma if it's there
if($ARGV[0] =~ m/\s+\d/) {              # If we replace comma with a space,
    ($ARGV[0], $ARGV[1]) = split(/ /, $ARGV[0]); #split it
}
$ARGV[0] =~ s/\s+//;                    # Remove any spaces
$ARGV[0] =~ s/\..*//;                   # Get rid of field name if it's there
$ARGV[1] =~ s/\D//g;                    # Make sure we only use digits
$ARGV[1] = $ARGV[1] || 0;               # interest defaults to 0
printRecord($ARGV[0], $ARGV[1]);        # Do the do


########## End of main ###########



# parseDbd
# takes given dbd file and parses it to produce a hash table of record types
# giving their fields, and for each field its interest level and data type
# usage: void parseDbd("fileName");
# Output is in the hash %record. This is a hash of (references to) another.
# hash containing the fields of this record, as keys. The value keyed by
# the field names are (references to) arrays. Each of these arrays contains
# the interest level, data type and base of the field
sub parseDbd {
    my $dbdFile = $_[0];
    my @dbd;
    my $length;
    my $level = 0;
    my $i = 0;
    my $isArecord = 0;
    my $isAfield;
    my $thisRecord;
    my $thisField;
    my $thisType;
    my %field = ();
    my @params = ();
    my $interest = 0;
    my $thisBase = "DECIMAL";
    my $item;
    my $newDevice;

    open(DBD, "< $dbdFile") || die "Can't open dbd file $dbdFile --";
    @dbd = <DBD>;
    $length = @dbd;
    close(DBD) || die "Can't close $dbdFile --";

    while ($i < $length) {
        $_ = $dbd[$i];
        chomp;
        print("line $i - level $level\n") if ($DEBUG);
            #$line = $dbd[$i] || die "Unexpected end of file: $dbdFile, line $.";
        if( m/recordtype/ ) {
            ($level == 0) || die "dbd file format error in or before line $i --";
            m/\((.*)\)/;                    #get record type
            #@records = (@records, $1);
            $isArecord = 1;
            $thisRecord = $1;
        }
        if( m/field/ ) {
            ($level == 1 && $isArecord) || die "dbd file format error in or before line $i --";
            m/\((.*),/;                        # get field name
            $thisField = $1;
            m/,(.*)\)/;                        # get field type
            $thisType = $1;
            $isAfield = 1;
            #print("$1 , line $i ");
        }
        if( m/interest/ ) {
            ($level == 2 && $isAfield) || die "dbd file format error in or before line $i --";
            m/\((.*)\)/ ;                    # get interest level, default = 0
            $interest = $1;
        }
        if( m/base/ ) {
            ($level == 2 && $isAfield) || die "dbd file format error in or before line $i --";
            m/\((.*)\)/ ;                    # get base, default = DECIMAL
            $thisBase = $1;
        }
        if( m/\{/ ) { $level++ };
        if( m/\}/ ) {
            if( $level == 2 && $isAfield) {
                $isAfield = 0;
                $params[$iIdx] = $interest;
                $params[$tIdx] = $thisType;
                $params[$bIdx] = $thisBase;
                $field{$thisField} = [@params];
                #print("interest $interest\n");
                $interest = 0;                    # set up default for next time
                $thisBase = "DECIMAL";            # set up default for next time
            }
            if( $level == 1 && $isArecord) {
                $isArecord = 0;
                $record{$thisRecord} = { %field };
                #print("record type $thisRecord ");
                #foreach $key (keys(%field)) {
                #    print("Field $key - interest $field{$key}\n");
                #}
                %field = ();                    # set up for next time
            }
            $level--;
        }
        # Parse for record types with device support
        if( m/device/ ) {
            m/\((.*?),/;
            if(!exists($device{$1})) {
                # Use a hash to make a list of record types with device support
                $device{$1} = 1;
            }
        }
        $i++;
    }
}


# Given a record name attempts to find the record and its type.
# Usage: getRecType(recordName) - returns ($error, $recordType)
sub getRecType {
    my $name = $_[0] . ".RTYP";
    my $type;
    my $data;

    $data = caget($name);
    if ($data =~ m/Invalid channel name/) { die "Record \"$_[0]\" not found\n"; }
    chomp $data;
    $data =~ s/\s+//;
    #print("$name is a \"$data\"type\n");
    return($data);
}

# Given the record type and the field returns the interest level, data type
# and base for the field
# Usage: ($dataType, $interest, $base) getFieldParams( $recType, $field)
sub getFieldParams {
    my $recType = $_[0];
    my $field   = $_[1];
    my ($fType, $fInterest, $fBase);

    exists($fieldType{$record{$recType}{$field}[$tIdx]})  ||
        die "Field data type $field for $recType not found in dbd file --";
    exists($record{$recType}{$field}[$iIdx])  ||
        die "Interest level for $field in $recType not found in dbd file --";

    $fType     = $fieldType{$record{$recType}{$field}[$tIdx]};
    $fInterest = $record{$recType}{$field}[$iIdx];
    $fBase       = $record{$recType}{$field}[$bIdx];
    return($fType, $fInterest, $fBase);
}

# Prints field name and data for given field. Formats output so
# that fields align in to 4 columns. Tries to imitate dbpf format
# Usage: printField( $fieldName, $data, $dataType, $base, $firstColumnPosn)
sub printField {
    my $fieldName = $_[0];
    my $fieldData = $_[1];
    my $dataType  = $_[2];
    my $base      = $_[3];    # base to display numeric data in
    my $col       = $_[4];    # first column to print in

    my $screenWidth  = 80;
    my ($outStr, $len, $wide, $pad, $field);

    $field = $fieldName . ":";

    if( $dataType eq "ezcaString" ) {
        $outStr = sprintf("%-5s %s", $field, $fieldData);
    } elsif ( $base eq "HEX" ) {
        $outStr = sprintf("%-5s %x", $field, $fieldData);
    } elsif ( $dataType eq "ezcaDouble" || $dataType eq "ezcaFloat" ) {
        $outStr = sprintf("%-5s %.8f", $field, $fieldData);
    } elsif ( $dataType eq "ezcaShort" ) { 
        $outStr = sprintf("%-5s %d", $field, $fieldData);
    } elsif ( $dataType eq "ezcaChar" ) { 
        $outStr = sprintf("%-5s %d", $field, ord($fieldData));
    }else {
        $outStr = sprintf("%-5s %d", $field, $fieldData);
    }

    $len = length($outStr);
    if($len <= 20) { $wide = 20; }
    elsif( $len <= 40 ) { $wide = 40; }
    elsif( $len <= 60 ) { $wide = 60; }
    else { $wide = 80;}

    $pad = $wide - $len;
    #print "outStr=$outStr .. pad=$pad\n";

    if( $col + $wide > $screenWidth ) {
        print("\n");
        $col = 0;
    }

    print sprintf("$outStr%*s",$pad," ");
    $col = $col + $wide;

    return($col);
}

# Given record name and interest level prints data from record fields
# that are at or below the interest level specified.
# Useage: printRecord( $recordName, $interestLevel)
sub printRecord {
    my $name = $_[0];
    my $interest = $_[1];
    my ($error, $recType, $field, $fType, $fInterest, $data);
    my ($fToGet, $col, $base);
    #print("checking record $name, interest $interest\n");
    $col = 0;

    $recType = getRecType($name);
    print("$name is record type $recType\n");
    exists($record{$recType})  || die "Record type $recType not found in dbd file --";

    foreach $field (sort keys %{$record{$recType}}) {
        # Skip DTYP field if this rec type doesn't have device support defined
        if($field eq "DTYP" && !(exists($device{$recType}))) { next; }

        ($fType, $fInterest, $base) = getFieldParams($recType, $field);
        unless( $fType eq "ezcaNoAccess" ) {
            if( $interest >= $fInterest ) {
                $fToGet = $name . "." . $field;
                $data = caget($fToGet);
                chomp $data;
                #$data =~ s/\s+//;
                #if( $data[0] )  { die "ca get error on field $field --" };
                $col = printField($field, $data, $fType, $base, $col);
                #print ("fType = $fType $data[1]\n");
            }
        }
    }
    print("\n");  # Final line feed
}

# Prints list of record types found in dbd file. If level > 0
# then the fields of that record type, their interest levels and types are
# also printed.
# Diagnostic routine, usage: void printList(level);
sub printList {
    my $level = $_[0];
    my ($rkey, $fkey);

    foreach $rkey (sort keys(%record)) {
        print("$rkey\n");
        if($level > 0) {
            foreach $fkey (keys %{$record{$rkey}}) {
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
    my ($rkey, $fkey);
    my $type = $_[0];

    if( exists($record{$type}) ) {
        print("Record type - $type\n");
        foreach $fkey (sort keys %{$record{$type}}) {
            printf("%-4s", $fkey);
            printf("    interest = $record{$type}{$fkey}[$iIdx]");
            printf("    type = %-12s ",$record{$type}{$fkey}[$tIdx]);
            print ("    base = $record{$type}{$fkey}[$bIdx]\n");
        }
    }
    else {
        print("Record type $type not defined in dbd file $theDbdFile\n");
    }
}

# variable to communicate callback handler result
my $callback_data;
my $incomplete;
my $cadebug = 0;

# returns a terse result of a caget operation
# using perlCA
sub caget {
    my $name = $_[0];
    print $name . "\n" if $cadebug;
    show_call_stack() if $cadebug;
    my $channel = CA->new($name);
    my $wait = 1;
    eval {  CA->pend_io($wait); };
    if ($@) {
        if ($@ =~ m/^ECA_TIMEOUT/) {
            my $err =  "'" . $channel . "'";
            print "Channel connect timed out: $err not found.\n";
        }
        die $@;
    }
    my $type = $channel->field_type;
    my $count = $channel->element_count;
    $channel->get_callback(\&callback_handler, $type);
    $incomplete = 1;
    CA->pend_event(0.1) while ($incomplete); 
    return $callback_data;
}

sub callback_handler
{
    my ($chan, $status, $data) = @_;
    die $status if $status;
    #display($chan, $type, $data);
    $callback_data = $data;
    $incomplete=0;
}

sub show_call_stack {
  my ( $path, $line, $subr );
  my $max_depth = 30;
  my $i = 1;
  print("--- Begin stack trace ---\n");
  my @call_details;
  @call_details = caller($i);
  while ( (@call_details) && ($i<$max_depth) ) {
    print("$call_details[1] line $call_details[2] in function $call_details[3]\n");
    $i = $i +1;
    @call_details = caller($i);
  }
  print("--- End stack trace ---\n");
}

