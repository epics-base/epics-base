#!/usr/bin/perl

#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Getopts;
use File::Basename;
use DBD;
use DBD::Parser;
use EPICS::macLib;
use EPICS::Readfile;

my $tool = 'dbdToRecordtypeH.pl';

use vars qw($opt_D @opt_I $opt_o $opt_s);
getopts('DI@o:s') or
    die "Usage: $tool [-D] [-I dir] [-o xRecord.h] xRecord.dbd [xRecord.h]\n";

my @path = map { split /[:;]/ } @opt_I; # FIXME: Broken on Win32?
my $dbd = DBD->new();

my $infile = shift @ARGV;
$infile =~ m/\.dbd$/ or
    die "$tool: Input file '$infile' must have '.dbd' extension\n";
my $inbase = basename($infile);

my $outfile;
if ($opt_o) {
    $outfile = $opt_o;
} elsif (@ARGV) {
    $outfile = shift @ARGV;
} else {
    ($outfile = $infile) =~ s/\.dbd$/.h/;
    $outfile =~ s/^.*\///;
    $outfile =~ s/dbCommonRecord/dbCommon/;
}
my $outbase = basename($outfile);

# Derive a name for the include guard
my $guard_name = "INC_$outbase";
$guard_name =~ tr/a-zA-Z0-9_/_/cs;
$guard_name =~ s/(_[hH])?$/_H/;

ParseDBD($dbd, Readfile($infile, 0, \@opt_I));

my $rtypes = $dbd->recordtypes;
die "$tool: Input file must contain a single recordtype definition.\n"
    unless (1 == keys %{$rtypes});

if ($opt_D) {   # Output dependencies only, to stdout
    my %filecount;
    my @uniqfiles = grep { not $filecount{$_}++ } @inputfiles;
    print "$outfile: ", join(" \\\n    ", @uniqfiles), "\n\n";
    print map { "$_:\n" } @uniqfiles;
} else {
    open OUTFILE, ">$outfile" or die "$tool: Can't open $outfile: $!\n";
    print OUTFILE "/* $outbase generated from $inbase */\n\n",
        "#ifndef $guard_name\n",
        "#define $guard_name\n\n";

    our ($rn, $rtyp) = each %{$rtypes};

    print OUTFILE $rtyp->toCdefs;

    my @menu_fields = grep {
            $_->dbf_type eq 'DBF_MENU'
        } $rtyp->fields;
    my %menu_used;
    grep {
            !$menu_used{$_}++
        } map {
            $_->attribute('menu')
        } @menu_fields;
    our $menus_defined = $dbd->menus;
    while (my ($name, $menu) = each %{$menus_defined}) {
        print OUTFILE $menu->toDeclaration;
        if ($menu_used{$name}) {
            delete $menu_used{$name}
        }
    }
    our @menus_external = keys %menu_used;

    print OUTFILE $rtyp->toDeclaration;

    unless ($rn eq 'dbCommon') {
        my $n = 0;
        print OUTFILE "typedef enum {\n",
            join(",\n",
                map { "\t${rn}Record$_ = " . $n++ } $rtyp->field_names),
            "\n} ${rn}FieldIndex;\n\n";
        print OUTFILE "#ifdef GEN_SIZE_OFFSET\n\n";
        if ($opt_s) {
            newtables();
        } else {
            oldtables();
        }
        print OUTFILE "#endif /* GEN_SIZE_OFFSET */\n";
    }
    print OUTFILE "\n",
        "#endif /* $guard_name */\n";
    close OUTFILE;
}

sub oldtables {
    # Output compatible with R3.14.x
    print OUTFILE
        "#include <epicsAssert.h>\n" .
        "#include <epicsExport.h>\n" .
        "#ifdef __cplusplus\n" .
        "extern \"C\" {\n" .
        "#endif\n" .
        "static int ${rn}RecordSizeOffset(dbRecordType *prt)\n" .
        "{\n" .
        "    ${rn}Record *prec = 0;\n\n" .
        "    assert(prt->no_fields == " . scalar($rtyp->fields) . ");\n" .
        join("\n", map {
                "    prt->papFldDes[${rn}Record" . $_->name . "]->size = " .
                "sizeof(prec->" . $_->C_name . ");"
            } $rtyp->fields) . "\n" .
        join("\n", map {
                "    prt->papFldDes[${rn}Record" . $_->name . "]->offset = (unsigned short)(" .
                "(char *)&prec->" . $_->C_name . " - (char *)prec);"
            } $rtyp->fields) . "\n" .
        "    prt->rec_size = sizeof(*prec);\n" .
        "    return 0;\n" .
        "}\n" .
        "epicsExportRegistrar(${rn}RecordSizeOffset);\n\n" .
        "#ifdef __cplusplus\n" .
        "}\n" .
        "#endif\n";
}

sub newtables {
    # Output for an eventual DBD-less IOC
    print OUTFILE (map {
            "extern const dbMenu ${_}MenuMetaData;\n"
        } @menus_external), "\n";
    while (my ($name, $menu) = each %{$menus_defined}) {
        print OUTFILE $menu->toDefinition;
    }
    print OUTFILE (map {
        "static const char ${rn}FieldName$_\[] = \"$_\";\n" }
        $rtyp->field_names), "\n";
    my $n = 0;
    print OUTFILE "static const dbRecordData ${rn}RecordMetaData;\n\n",
        "static dbFldDes ${rn}FieldMetaData[] = {\n",
        join(",\n", map {
                my $fn = $_->name;
                my $cn = $_->C_name;
                "    { ${rn}FieldName${fn}," .
                    $_->dbf_type . ',"' .
                    $_->attribute('initial') . '",' .
                    ($_->attribute('special') || '0') . ',' .
                    ($_->attribute('pp') || 'FALSE') . ',' .
                    ($_->attribute('interest') || '0') . ',' .
                    ($_->attribute('asl') || 'ASL0') . ',' .
                    $n++ . ",\n\t\&${rn}RecordMetaData," .
                    "GEOMETRY_DATA(${rn}Record,$cn) }";
            } $rtyp->fields),
        "\n};\n\n";
    print OUTFILE "static const ${rn}FieldIndex ${rn}RecordLinkFieldIndices[] = {\n",
        join(",\n", map {
                "    ${rn}Record" . $_->name; 
            } grep {
                $_->dbf_type =~ m/^DBF_(IN|OUT|FWD)LINK/;
            } $rtyp->fields),
        "\n};\n\n";
    my @sorted_names = sort $rtyp->field_names;
    print OUTFILE "static const char * const ${rn}RecordSortedFieldNames[] = {\n",
        join(",\n", map {
            "    ${rn}FieldName$_"
        } @sorted_names),
        "\n};\n\n";
    print OUTFILE "static const ${rn}FieldIndex ${rn}RecordSortedFieldIndices[] = {\n",
        join(",\n", map {
            "    ${rn}Record$_"
        } @sorted_names),
        "\n};\n\n";
    print OUTFILE "extern rset ${rn}RSET;\n\n",
        "static const dbRecordData ${rn}RecordMetaData = {\n",
        "    \"$rn\",\n",
        "    sizeof(${rn}Record),\n",
        "    NELEMENTS(${rn}FieldMetaData),\n",
        "    ${rn}FieldMetaData,\n",
        "    ${rn}RecordVAL,\n",
        "    \&${rn}FieldMetaData[${rn}RecordVAL],\n",
        "    NELEMENTS(${rn}RecordLinkFieldIndices),\n",
        "    ${rn}RecordLinkFieldIndices,\n",
        "    ${rn}RecordSortedFieldNames,\n",
        "    ${rn}RecordSortedFieldIndices,\n",
        "    \&${rn}RSET\n",
        "};\n\n",
        "#ifdef __cplusplus\n",
        "extern \"C\" {\n",
        "#endif\n\n";
    print OUTFILE "dbRecordType * epicsShareAPI ${rn}RecordRegistrar(dbBase *pbase, int nDevs)\n",
        "{\n",
        "    dbRecordType *prt = dbCreateRecordtype(&${rn}RecordMetaData, nDevs);\n";
    print OUTFILE "    ${rn}FieldMetaData[${rn}RecordDTYP].typDat.pdevMenu = \&prt->devMenu;\n";
    while (my ($name, $menu) = each %{$menus_defined}) {
        print OUTFILE "    dbRegisterMenu(pbase, \&${name}MenuMetaData);\n";
    }
    print OUTFILE map {
            "    ${rn}FieldMetaData[${rn}Record" . 
            $_->name .
            "].typDat.pmenu = \n".
            "        \&" .
            $_->attribute('menu') .
            "MenuMetaData;\n";
        } @menu_fields;
    print OUTFILE map {
                "    ${rn}FieldMetaData[${rn}Record" .
                $_->name .
            "].typDat.base = CT_HEX;\n"; 
            } grep {
                $_->attribute('base') eq 'HEX';
            } $rtyp->fields;
    print OUTFILE "    dbRegisterRecordtype(pbase, prt);\n";
    print OUTFILE "    return prt;\n}\n\n",
        "#ifdef __cplusplus\n",
        "} /* extern \"C\" */\n",
        "#endif\n\n";
}
