eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeIoccdcmds.pl

use Cwd;

$cwd  = cwd();
#hack for sun4
$cwd =~ s|/tmp_mnt||;
$arch = $ARGV[0];
print "arch $arch";

unlink("cdcmds");
open(OUT,">cdcmds") or die "$! opening cdcmds";
print OUT "startup = \"$cwd\"\n";
$appbin = $cwd;
$appbin =~ s/iocBoot.*//;
$appbin = $appbin . "/bin/${arch}";
print OUT "appbin = \"$appbin\"\n";
close OUT;
