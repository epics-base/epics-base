eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # convertRelease.pl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Id$
#
# Parse configure/RELEASE file(s) and generate a derived output file.
#

use Cwd qw(cwd abs_path);
use Getopt::Std;

$cwd = UnixPath(cwd());

getopt "ahtT";

if ($opt_a) {
    $arch = $opt_a;
} else {		# Look for O.<arch> in current path
    $_ = $cwd;
    ($arch) = /.*\/O.([\w-]+)$/;
}

$hostarch = $arch;
$hostarch = $opt_h if ($opt_h);

if ($opt_T) {
    $top = $opt_T;
} else {		# Find $top from current path
    # This approach is only possible under iocBoot/* and configure/*
    $top = $cwd;
    $top =~ s/\/iocBoot.*$//;
    $top =~ s/\/configure.*$//;
}

# The IOC may need a different path to get to $top
if ($opt_t) {
    $iocroot = $opt_t;
    $root = $top;
    while (substr($iocroot, -1, 1) eq substr($root, -1, 1)) {
	chop $iocroot;
	chop $root;
    }
}

unless (@ARGV == 1) {
    print "Usage: convertRelease.pl [-a arch] [-h hostarch] [-T top] [-t ioctop] outfile\n";
    print "   where outfile is be one of:\n";
    print "\tcheckRelease - checks consistency with support apps\n";
    print "\tcdCommands - generate cd path strings for vxWorks IOCs\n";
    print "\tenvPaths - generate epicsEnvSet commands for other IOCs\n";
    print "\tCONFIG_APP_INCLUDE - additional build variables\n";
    print "\tRULES_INCLUDE - supports installable build rules\n";
    exit 2;
}
$outfile = $ARGV[0];

# TOP refers to this application
%macros = (TOP => LocalPath($top));
@apps   = (TOP);	# Records the order of definitions in RELEASE file

# Read the RELEASE file(s)
$relfile = "$top/configure/RELEASE";
die "Can't find $relfile" unless (-f $relfile);
&readReleaseFiles($relfile, \%macros, \@apps);
&expandRelease(\%macros, \@apps);


# This is a perl switch statement:
for ($outfile) {
    /CONFIG_APP_INCLUDE/ and do { &configAppInclude;	last; };
    /RULES_INCLUDE/	 and do { &rulesInclude;	last; };
    /cdCommands/	 and do { &cdCommands;		last; };
    /envPaths/  	 and do { &envPaths;		last; };
    /checkRelease/	 and do { &checkRelease;	last; };
    die "Output file type \'$outfile\' not supported";
}

#
# Parse all relevent configure/RELEASE* files and includes
#
sub readReleaseFiles {
    my ($relfile, $Rmacros, $Rapps) = @_;

    return unless (-r $relfile);
    &readRelease($relfile, $Rmacros, $Rapps);
    if ($hostarch) {
	my ($hrelfile) = "$relfile.$hostarch";
	&readRelease($hrelfile, $Rmacros, $Rapps) if (-r $hrelfile);
    }
    if ($arch) {
	my ($crelfile) = "$relfile.Common.$arch";
	&readRelease($crelfile, $Rmacros, $Rapps) if (-r $crelfile);
	if ($hostarch) {
	    my ($arelfile) = "$relfile.$hostarch.$arch";
	    &readRelease($arelfile, $Rmacros, $Rapps) if (-r $arelfile);
	}
    }
}

#
# Parse a configure/RELEASE file and its includes.
#
# NB: This subroutine also appears in base/src/makeBaseApp/makeBaseApp.pl
# If you make changes here, they will be needed there as well.
#
sub readRelease {
    my ($file, $Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array
    my ($pre, $var, $post, $macro, $path);
    local *IN;
    open(IN, $file) or die "Can't open $file: $!\n";
    while (<IN>) {
	chomp;
	s/\r$//;		# Shouldn't need this, but sometimes...
	s/\s*#.*$//;		# Remove trailing comments
	s/\s+$//;		# Remove trailing whitespace
	next if /^\s*$/;	# Skip blank lines
	
	# Expand all already-defined macros in the line:
	while (($pre,$var,$post) = /(.*)\$\((\w+)\)(.*)/) {
	    last unless (exists $Rmacros->{$var});
	    $_ = $pre . $Rmacros->{$var} . $post;
	}
	
	# Handle "<macro> = <path>"
	($macro, $path) = /^\s*(\w+)\s*=\s*(.*)/;
	if ($macro ne "") {
		$macro="TOP" if $macro =~ /^INSTALL_LOCATION/ ;
		if (exists $Rmacros->{$macro}) {
			delete $Rmacros->{$macro};
		} else {
			push @$Rapps, $macro;
		}
	    $Rmacros->{$macro} = $path;
	    next;
	}
	# Handle "include <path>" syntax
	($path) = /^\s*include\s+(.*)/;
	&readRelease($path, $Rmacros, $Rapps) if (-r $path);
    }
    close IN;
}

sub expandRelease {
    my ($Rmacros, $Rapps) = @_;
    # Expand any (possibly nested) macros that were defined after use
    while (($macro, $path) = each %$Rmacros) {
	while (($pre,$var,$post) = $path =~ /(.*)\$\((\w+)\)(.*)/) {
	    $path = $pre . $Rmacros->{$var} . $post;
	    $Rmacros->{$macro} = $path;
	}
    }
}

sub configAppInclude {
    @includes = grep !/^(TOP|TEMPLATE_TOP)$/, @apps;
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "# Do not modify this file, changes made here will\n";
    print OUT "# be lost when the application is next rebuilt.\n\n";
    
    if ($arch) {
	print OUT "export TOP\n";
	foreach $app (@includes) {
	    print OUT "export ${app}\n";
	}
	foreach $app (@includes) {
	    $path = $macros{$app};
	    next unless (-d "$path/bin/$hostarch");
	    print OUT "${app}_HOST_BIN = \$(strip \$($app))/bin/\$(EPICS_HOST_ARCH)\n";
	}
	foreach $app (@includes) {
	    $path = $macros{$app};
	    next unless (-d "$path/lib/$hostarch");
	    print OUT "${app}_HOST_LIB = \$(strip \$($app))/bin/\$(EPICS_HOST_ARCH)\n";
	}
	foreach $app (@includes) {
	    $path = $macros{$app};
	    next unless (-d "$path/bin/$arch");
	    print OUT "${app}_BIN = \$(strip \$($app))/bin/$arch\n";
	}
	foreach $app (@includes) {
	    $path = $macros{$app};
	    next unless (-d "$path/lib/$arch");
	    print OUT "${app}_LIB = \$(strip \$($app))/lib/$arch\n";
	}
	# We can't just include TOP in the foreach list:
	# 1. The lib directory probably doesn't exist yet, and
	# 2. We need an abolute path but $(TOP_LIB) is relative
	foreach $app (@includes) {
	    $path = $macros{$app};
	    next unless (-d "$path/lib/$arch");
	    print OUT "SHRLIB_SEARCH_DIRS += \$(${app}_LIB)\n";
	}
    }
    foreach $app (@includes) {
	$path = $macros{$app};
	next unless (-d "$path/include");
	print OUT "RELEASE_INCLUDES += -I\$(strip \$($app))/include/os/\$(OS_CLASS)\n";
	print OUT "RELEASE_INCLUDES += -I\$(strip \$($app))/include\n";
    }
    foreach $app (@includes) {
	$path = $macros{$app};
	next unless (-d "$path/dbd");
	print OUT "RELEASE_DBDFLAGS += -I \$(strip \$($app))/dbd\n";
    }
    close OUT;
}

sub rulesInclude {
    @includes = grep !/^(TOP|TEMPLATE_TOP|EPICS_BASE)$/, @apps;
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "# Do not modify this file, changes made here will\n";
    print OUT "# be lost when the application is next rebuilt.\n\n";
    
    foreach $app (@includes) {
	$path = $macros{$app};
	next unless (-r "$path/configure/RULES_BUILD");
	print OUT "RULES_TOP:=\$($app)\n";
	print OUT "-include \$(strip \$(RULES_TOP))/configure/RULES_BUILD\n";
    }
    close OUT;
}

sub cdCommands {
    die "Architecture not set (use -a option)" unless ($arch);
    @includes = grep !/^TEMPLATE_TOP$/, @apps;
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    
    $startup = $cwd;
    $startup =~ s/^$root/$iocroot/o if ($opt_t);
    
    print OUT "startup = \"$startup\"\n";
    
    $ioc = $cwd;
    $ioc =~ s/^.*\///;	# iocname is last component of directory name
    
    print OUT "putenv \"ARCH=$arch\"\n";
    print OUT "putenv \"IOC=$ioc\"\n";
    
    foreach $app (@includes) {
	$iocpath = $path = $macros{$app};
	$iocpath =~ s/^$root/$iocroot/o if ($opt_t);
	$app_lc = lc($app);
	print OUT "$app_lc = \"$iocpath\"\n" if (-d $path);
	print OUT "putenv \"$app=$iocpath\"\n" if (-d $path);
	print OUT "${app_lc}bin = \"$iocpath/bin/$arch\"\n" if (-d "$path/bin/$arch");
    }
    close OUT;
}

sub envPaths {
    die "Architecture not set (use -a option)" unless ($arch);
    @includes = grep !/^TEMPLATE_TOP$/, @apps;
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    
    $ioc = $cwd;
    $ioc =~ s/^.*\///;	# iocname is last component of directory name
    
    print OUT "epicsEnvSet(ARCH,\"$arch\")\n";
    print OUT "epicsEnvSet(IOC,\"$ioc\")\n";
    
    foreach $app (@includes) {
	$iocpath = $path = $macros{$app};
	$iocpath =~ s/^$root/$iocroot/o if ($opt_t);
	print OUT "epicsEnvSet($app,\"$iocpath\")\n" if (-d $path);
    }
    close OUT;
}

sub checkRelease {
    $status = 0;
    delete $macros{TOP};
    delete $macros{TEMPLATE_TOP};
    
    while (($app, $path) = each %macros) {
	%check = (TOP => $path);
	@order = ();
	$relfile = "$path/configure/RELEASE";
	&readReleaseFiles($relfile, \%check, \@order);
	&expandRelease(\%check, \@order);
	delete $check{TOP};
	
	while (($parent, $ppath) = each %check) {
	    if (exists $macros{$parent} &&
		abs_path($macros{$parent}) ne abs_path($ppath)) {
		print "\n" unless ($status);
		print "Definition of $parent conflicts with $app support.\n";
		print "In this application configure/RELEASE defines\n";
		print "\t$parent = $macros{$parent}\n";
		print "but $app at $path has\n";
		print "\t$parent = $ppath\n";
		$status = 1;
	    }
	}
    }
    print "\n" if ($status);
    exit $status;
}

# Path rewriting rules for various OSs
# These functions are duplicated in src/makeBaseApp/makeBaseApp.pl
sub UnixPath {
    my ($newpath) = @_;
    if ($^O eq "cygwin") {
	$newpath =~ s|\\|/|go;
	$newpath =~ s%^([a-zA-Z]):/%/cygdrive/$1/%;
    } elsif ($^O eq 'sunos') {
	$newpath =~ s(^\/tmp_mnt/)(/);
    }
    return $newpath;
}

sub LocalPath {
    my ($newpath) = @_;
    if ($^O eq "cygwin") {
	$newpath =~ s%^/cygdrive/([a-zA-Z])/%$1:/%;
    } elsif ($^O eq "darwin") {
	# These rules are likely to be site-specific
	$newpath =~ s%^/private/var/auto\.home/%/home/%;    # APS
    }
    return $newpath;
}
