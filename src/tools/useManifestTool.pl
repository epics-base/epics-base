eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
        if 0; 

#
# Use MS Visual C++ compiler version number to determine if
# we want to use the Manifest Tool (status=1) or not (status=0)
#
# VC compiler versions >= 14.00 will have status=1
# VC compiler versions 10.00 - 13.10 will have status=0
# EPICS builds with older VC compilers is not supported
#

my $versionString=`cl 2>&1`;

if ($versionString =~ m/Version 14./) {
 $status=1;
} elsif ($versionString =~ m/Version 13.10/){
 $status=0;
} elsif ($versionString =~ m/Version 13.0/){
 $status=0;
} elsif ($versionString =~ m/Version 12./){
 $status=0;
} elsif ($versionString =~ m/Version 11./){
 $status=0;
} elsif ($versionString =~ m/Version 10./){
 $status=0;
} else {
 $status=1;
}
print "$status\n";
exit;
