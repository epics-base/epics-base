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
# This tool replaces makeConfigAppInclude.pl, makeIocCdCommands.pl and
# makeRulesInclude.pl and adds consistency checks for RELEASE files.
#

use Cwd;
use Getopt::Std;

$cwd = cwd();
$cwd =~ s/\/tmp_mnt//;	# hack for sun4
$cwd =~ s/\\/\//g;	# hack for win32

getopt "aht";

if ($opt_a) {
    $arch = $opt_a;
} else {		# Look for O.<arch> in current path
    $_ = $cwd;
    ($arch) = /.*\/O.([\w-]+)$/;
}

$hostarch = $arch;
$hostarch = $opt_h if ($opt_h);

# Find $top from current path; NB only works under iocBoot/* and configure/*
$top = $cwd;
$top =~ s/^\/cygdrive\/(\w)\//$1:\//;
$top =~ s/\/iocBoot.*$//;
$top =~ s/\/configure.*$//;

# The IOC may need a different path to get to $top
if ($opt_t) {
    $iocroot = $ioctop = $opt_t;
    $root = $top;
    while (substr($iocroot, -1, 1) eq substr($root, -1, 1)) {
	chop $iocroot;
	chop $root;
    }
} else {
    $ioctop = $top;
}

unless (@ARGV == 1) {
    print "Usage: convertRelease.pl [-a arch] [-h hostarch] [-t ioctop] outfile\n";
    print "   where outfile is be one of:\n";
    print "\tcheckRelease - checks consistency with support apps\n";
    print "\tcdCommands - generate cd path strings for IOC use\n";
    print "\tCONFIG_APP_INCLUDE - additional build variables\n";
    print "\tRULES_INCLUDE - supports installable build rules\n";
    exit 2;
}
$outfile = $ARGV[0];

# TOP refers to this application
%macros = (TOP => $top);
@apps   = (TOP);	# Records the order of definitions in RELEASE file

# Read the RELEASE file(s)
$relfile = "$top/configure/RELEASE";
die "Can't find configure/RELEASE file" unless (-r $relfile);
&readRelease($relfile, \%macros, \@apps);

if ($hostarch) {
    $relfile .= ".$hostarch";
    &readRelease($relfile, \%macros, \@apps) if (-r $relfile);
}
&expandRelease(\%macros, \@apps);


# This is a perl switch statement:
for ($outfile) {
    /CONFIG_APP_INCLUDE/ and do { &configAppInclude;	last; };
    /RULES_INCLUDE/	 and do { &rulesInclude;	last; };
    /cdCommands/	 and do { &cdCommands;		last; };
    /checkRelease/	 and do { &checkRelease;	last; };
    die "Output file type \'$outfile\' not supported";
}

#
# Parse a configure/RELEASE file.
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
	$path = $macros{"TOP"};
	print OUT "SHRLIB_SEARCH_DIRS = $path/lib/$arch\n";
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
	print OUT "-include \$(strip \$($app))/configure/RULES_BUILD\n";
    }
    close OUT;
}

sub cdCommands {
    die "Architecture not set (use -a option)" unless ($arch);
    @includes = grep !/^TEMPLATE_TOP$/, @apps;
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    
    $startup = $cwd;
    $startup =~ s/^$root/$iocroot/ if ($opt_t);
    
    print OUT "startup = \"$startup\"\n";
    print OUT "appbin = \"$ioctop/bin/$arch\"\n";	# R3.13.1 compatibility
    
    foreach $app (@includes) {
	$iocpath = $path = $macros{$app};
	$iocpath =~ s/^$root/$iocroot/ if ($opt_t);
	$app_lc = lc($app);
        print OUT "$app_lc = \"$iocpath\"\n" if (-d $path);
	print OUT "${app_lc}bin = \"$iocpath/bin/$arch\"\n" if (-d "$path/bin/$arch");
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
	&readRelease($relfile, \%check, \@order) if (-r $relfile);
	if ($hostarch) {
	    $relfile .= ".$hostarch";
	    &readRelease($relfile, \%check, \@order) if (-r $relfile);
	}
	&expandRelease(\%check, \@order);
	delete $check{TOP};
	
	while (($parent, $ppath) = each %check) {
	    if (exists $macros{$parent} && ($macros{$parent} ne $ppath)) {
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
