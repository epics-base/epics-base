eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeIocCdCommands.pl

use Cwd;

$cwd  = cwd();
#hack for sun4
$cwd =~ s|/tmp_mnt||;
$arch = $ARGV[0];

unlink("cdCommands");
open(OUT,">cdCommands") or die "$! opening cdCommands";
print OUT "startup = \"$cwd\"\n";
$appbin = $cwd;
$appbin =~ s/iocBoot.*//;
$appbin = $appbin . "/bin/${arch}";
print OUT "appbin = \"$appbin\"\n";
#look for SHARE in <top>/config/RELEASE
$release = $cwd;
$release =~ s/iocBoot.*//;
$release = $release . "config/RELEASE";
if (-r "$release") {
    open(IN, "$release") or die "Cannot open $release\n";
    while (<IN>) {
	chomp;
	s/^SHARE\s*=\s*// and $share = $_, break;
    }
    close IN;
}
if ( "$share" ) {
    print OUT "share = \"$share\"\n";
}
close OUT;
