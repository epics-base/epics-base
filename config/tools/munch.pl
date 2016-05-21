eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeConfigAppInclude.pl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# Creates a ctdt.c file of c++ static constructors and destructors.

@ctorlist = ();
@dtorlist = ();

while ($line = <STDIN>)
{
    next if ($line =~ /__?GLOBAL_.F.+/);
    next if ($line =~ /__?GLOBAL_.I._GLOBAL_.D.+/);
    if ($line =~ /__?GLOBAL_.D.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        chop $name;
        $name =~ s/^__/_/;
        next if ( $name =~ /^__?GLOBAL_.D.*.\.cpp/ );
        next if ( $name =~ /^__?GLOBAL_.D.\.\./ );
        @dtorlist = (@dtorlist,$name);
    };
    if ($line =~ /__?GLOBAL_.I.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        chop $name;
        $name =~ s/^__/_/;
        next if ( $name =~ /^__?GLOBAL_.I.*.\.cpp/ );
        next if ( $name =~ /^__?GLOBAL_.I.\.\./ );
        @ctorlist = (@ctorlist,$name);
    };
}

foreach $ctor (@ctorlist)
{
    printf "void %s();\n",$ctor;
}

print "extern void (*_ctors[])();\n";
print "void (*_ctors[])() = {\n";
foreach $ctor (@ctorlist)
{
    printf "        %s,\n",$ctor;
}
print "        0};\n";


foreach $dtor (@dtorlist)
{
    printf "void %s();\n",$dtor;
}

print "extern void (*_ctors[])();\n";
print "void (*_dtors[])() = {\n";
foreach $dtor (@dtorlist)
{
    printf "        %s,\n",$dtor;
}
print "        0};\n";
