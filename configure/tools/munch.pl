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
#  $Id$

@ctorlist = ();
@dtorlist = ();

while ($line = <STDIN>)
{
    chomp $line;
    next if ($line =~ /__?GLOBAL_.F.+/);
    next if ($line =~ /__?GLOBAL_.I._GLOBAL_.D.+/);
    if ($line =~ /__?GLOBAL_.D.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        $name =~ s/^__/_/;
        @dtorlist = (@dtorlist,$name);
    };
    if ($line =~ /__?GLOBAL_.I.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        $name =~ s/^__/_/;
        @ctorlist = (@ctorlist,$name);
    };
}

foreach $ctor (@ctorlist)
{
	if ( $ctor =~ /\./ ) {
		printf "void %s() __asm__ (\"_%s\");\n",convertName($ctor),$ctor;
    } else {
		printf "void %s();\n",$ctor;
	}
}

print "extern void (*_ctors[])();\n";
print "void (*_ctors[])() = {\n";
foreach $ctor (@ctorlist)
{
	if ( $ctor =~ /\./ ) {
		printf "        %s,\n",convertName($ctor);
    } else {
    	printf "        %s,\n",$ctor;
	}
}
print "        0};\n";


foreach $dtor (@dtorlist)
{
	if ( $dtor =~ /\./ ) {
		printf "void %s() __asm__ (\"_%s\");\n",convertName($dtor),$dtor;
    } else {
		printf "void %s();\n",$dtor;
	}
}

print "extern void (*_ctors[])();\n";
print "void (*_dtors[])() = {\n";
foreach $dtor (@dtorlist)
{
	if ( $dtor =~ /\./ ) {
		printf "        %s,\n",convertName($dtor);
    } else {
    	printf "        %s,\n",$dtor;
	}
}
print "        0};\n";

sub convertName {
	my ($name) = @_;
	$name =~ s/\./\$/g;
	return $name;
}

