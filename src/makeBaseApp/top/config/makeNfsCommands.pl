eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeNfsCommands.pl

use Cwd;

$cwd  = cwd();

unlink("nfs.cmd");
open(INP,"<nfsCommands") and open(OUT,">nfs.cmd")
    or die "$! Copying nfsCommands to nfs.cmd";
while (<INP>) {
    print OUT $_;
}
print OUT "cd \"$cwd\"\n";
close INP; close OUT;
