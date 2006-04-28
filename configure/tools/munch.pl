eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell;
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#  $Id$
#
# Creates a ctdt.c file of C++ static constructors and destructors,
# as required for all vxWorks binaries containing C++ code.

@ctors = ();	%ctors = ();
@dtors = ();	%dtors = ();

while (my $line = <STDIN>)
{
    chomp $line;
    next if ($line =~ m/__?GLOBAL_.F.+/);
    next if ($line =~ m/__?GLOBAL_.I._GLOBAL_.D.+/);
    if ($line =~ m/__?GLOBAL_.D.+/) {
	($adr,$type,$name) = split ' ', $line, 3;
	push @dtors, $name unless exists $dtors{$name};
	$dtors{$name} = 1;
    }
    if ($line =~ m/__?GLOBAL_.I.+/) {
	($adr,$type,$name) = split ' ', $line, 3;
	push @ctors, $name unless exists $ctors{$name};
	$ctors{$name} = 1;
    }
}

print join "\n",
    "/* C++ static constructor and destructor lists */",
    "/* This is a generated file, do not edit! */",
    "\n/* Declarations */",
    (map cDecl($_), @ctors, @dtors),
    "\n/* Constructors */",
    "void (*_ctors[])() = {",
    (join ",\n", (map "    " . cName($_), @ctors), "    NULL"),
    "};",
    "\n/* Destructors */",
    "void (*_dtors[])() = {",
    (join ",\n", (map "    " . cName($_), reverse @dtors), "    NULL"),
    "};",
    "\n";

sub cName {
    my ($name) = @_;
    $name =~ s/^__/_/;
    $name =~ s/\./\$/g;
    return $name;
}

sub cDecl {
    my ($name) = @_;
    my $decl = 'void ' . cName($name) . '()';
    # 68k and MIPS targets allow periods in symbol names, which
    # can only be reached using an assembler string.
    if (m/\./) {
	$decl .= "\n    __asm__ (\"" . $name . "\");";
    } else {
	$decl .= ';';
    }
    return $decl;
}
