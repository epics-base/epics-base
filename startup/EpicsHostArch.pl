eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # EpicsHostArch.pl

# $Id$
# Returns the Epics host architecture suitable
# for assigning to the EPICS_HOST_ARCH variable

use Config;

$suffix="";
$suffix="-".$ARGV[0] if ($ARGV[0] ne "");

$EpicsHostArch = GetEpicsHostArch();
print "$EpicsHostArch$suffix";

sub GetEpicsHostArch { # no args
    $arch=$Config{'archname'};
    if ($arch =~ /sun4-solaris/)       { return "solaris-sparc";
    } elsif ($arch =~ m/i86pc-solaris/) { return "solaris-x86";
    } elsif ($arch =~ m/sun4-sunos/)    { return "sun4-68k";
    } elsif ($arch =~ m/i[3-6]86-linux/)    { return "linux-x86";
    } elsif ($arch =~ m/MSWin32-x86/)   { return "win32-x86";
    } elsif ($arch =~ m/PA-RISC1.1/)    { return "hpux-parisc";
    } else { return "unsupported"; }
}

#EOF EpicsHostArch.pl

