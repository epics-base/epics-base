eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeConfigAppInclude.pl

# Creates a ctdt.c file of c++ static constructors and destructors.
#  $Id$

@ctorlist = ();
@dtorlist = ();

while ($line = <STDIN>)
{
    if ($line =~ /__?GLOBAL_.D.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        chop $name;
        $name =~ s/^__/_/;
        @dtorlist = (@dtorlist,$name);
    };
    if ($line =~ /__?GLOBAL_.I.+/) {
        ($adr,$type,$name) = split ' ',$line,3;
        chop $name;
        $name =~ s/^__/_/;
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
