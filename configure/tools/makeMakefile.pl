#!/usr/bin/perl
#
#	makeMakefile.pl
#
#	called from RULES_ARCHS
#
#
#	Usage: perl makeMakefile.pl O.*-dir b_t top

$dir = $ARGV[0];
$t_a= $ARGV[1];
$top= $ARGV[2];
$b_t = $ARGV[3];
$makefile="$dir/Makefile";

mkdir ($dir, 0777)  unless -d $dir;

open OUT, "> $makefile"  or die "Cannot create $makefile";

print OUT "#This Makefile created by makeMakefiles.pl\n\n\n";
print OUT "all :\n";
print OUT "	\$(MAKE) -f ../Makefile.$b_t TOP=../$top T_A=$t_a B_T=$b_t \$@\n\n";
print OUT ".DEFAULT: force\n";
print OUT "	\$(MAKE) -f ../Makefile.$b_t TOP=../$top T_A=$t_a B_T=$b_t \$@\n\n";
print OUT "force:  ;\n";

close OUT;

#	EOF makeMakefile.pl

