#!/usr/bin/perl
#
#	makeMakefile.pl
#
#	called from RULES_ARCHS
#
#
#	Usage: perl makeMakefile.pl O.*-dir top

$dir = $ARGV[0];
$top= $ARGV[1];
$makefile="$dir/Makefile";

if ($dir =~ m'O.(.+)')
{
	$t_a = $1;
}
else
{
	die "Cannot extract T_A from $dir";
}

mkdir ($dir, 0777)  unless -d $dir;

open OUT, "> $makefile"  or die "Cannot create $makefile";

print OUT "#This Makefile created by makeMakefiles.pl\n\n\n";
print OUT "all :\n";
print OUT "	\$(MAKE) -f ../Makefile TOP=../$top T_A=$t_a \$@\n\n";
print OUT ".DEFAULT: force\n";
print OUT "	\$(MAKE) -f ../Makefile TOP=../$top T_A=$t_a \$@\n\n";
print OUT "force:  ;\n";

close OUT;

#	EOF makeMakefile.pl

