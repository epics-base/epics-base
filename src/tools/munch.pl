eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell;
#*************************************************************************
# Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
#  $Id$
#
# Creates a ctdt.c file of C++ static constructors and destructors,
# as required for all vxWorks binaries containing C++ code.

# Is exception handler frame info required?
my $need_eh_frame = 0;

@ctors = ();	%ctors = ();
@dtors = ();	%dtors = ();

while (my $line = <STDIN>)
{
    chomp $line;
    $need_eh_frame++ if m/__?gxx_personality_v[0-9]/;
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
    "#include <vxWorks.h>",
    "\n/* Declarations */",
    (map cDecl($_), @ctors, @dtors),
    '';

exceptionHandlerFrame() if $need_eh_frame;

print join "\n",
    "\n/* Constructors */",
    "void (*_ctors[])() = {",
    (join ",\n", (map "    " . cName($_), @ctors), "    NULL"),
    "};",
    "\n/* Destructors */",
    "void (*_dtors[])() = {",
    (join ",\n", (map "    " . cName($_), reverse @dtors), "    NULL"),
    "};",
    "\n";

# Outputs the C code for registering exception handler frame info
sub exceptionHandlerFrame {
    my $eh_ctor = 'eh_ctor';
    my $eh_dtor = 'eh_dtor';

    # Add EH ctor/dtor to _start_ of arrays
    unshift @ctors, $eh_ctor;
    unshift @dtors, $eh_dtor;

    print join "\n",
        '/* Exception handler frame */',
        'extern const unsigned __EH_FRAME_BEGIN__[];',
        '',
        "static void $eh_ctor(void) {",
        '    extern void __register_frame_info (const void *, void *);',
        '    static struct { unsigned pad[8]; } object;',
        '',
        '    __register_frame_info(__EH_FRAME_BEGIN__, &object);',
        '}',
        '',
        "static void $eh_dtor(void) {",
        '    extern void *__deregister_frame_info (const void *);',
        '',
        '    __deregister_frame_info(__EH_FRAME_BEGIN__);',
        '}',
        '';
    return;
}

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
