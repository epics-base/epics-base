eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # convertRelease.pl
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

if ($opt_t) {
    $top = $opt_t;
} else {		# Find $top from current path
    $top = $cwd;
    $top =~ s/\/iocBoot.*//;
    $top =~ s/\/configure\/O\..*//;
}

unless (@ARGV == 1) {
    print "Usage: convertRelease.pl [-a arch] [-h hostarch] [-t top] outfile\n";
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
@apps   = (TOP);	# Provides the order of apps in RELEASE file

# Read the RELEASE file(s)
$relfile = "$top/configure/RELEASE";
die "Can't find configure/RELEASE file" unless (-r $relfile);
&readRelease($relfile, \%macros, \@apps);

if ($hostarch) {
    $relfile .= ".$hostarch";
    &readRelease($relfile, \%macros, \@apps) if (-r $relfile);
}

# This is a perl switch statement:
for ($outfile) {
    /CONFIG_APP_INCLUDE/ and do { &configAppInclude;	last; };
    /RULES_INCLUDE/	 and do { &rulesInclude;	last; };
    /cdCommands/	 and do { &cdCommands;		last; };
    /checkRelease/	 and do { &checkRelease;	last; };
    die "Output file type \'$outfile\' not supported";
}

sub readRelease {
    my ($file, $Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array
    my ($pre, $macro, $post, $path);
    local *IN;
    open(IN, $file) or die "Can't open $file: $!\n";
    while (<IN>) {
	chomp;
	s/\s*#.*$//;		# Remove trailing comments
        next if /^\s*$/;	# Skip blank lines
	
	# Expand all macros in the line:
	while (($pre,$macro,$post) = /(.*)\$\((\w+)\)(.*)/, $macro ne "") {
	    $_ = $pre . $Rmacros->{$macro} . $post;
	}
	
	# Handle "<macro> = <path>"
	($macro, $path) = /^\s*(\w+)\s*=\s*(.*)/;
	if ($macro ne "") {
	    $Rmacros->{$macro} = $path;
	    push @$Rapps, $macro;
	    next;
	}
	# Handle "include <path>" syntax
	($path) = /^\s*include\s+(.*)/;
	&readRelease($path, $Rmacros, $Rapps) if (-r $path);
    }
    close IN;
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
	    print OUT "${app}_HOST_BIN = \$($app)/bin/\$(EPICS_HOST_ARCH)\n";
	}
	foreach $app (@includes) {
            $path = $macros{$app};
	    next unless (-d "$path/lib/$hostarch");
	    print OUT "${app}_HOST_LIB = \$($app)/bin/\$(EPICS_HOST_ARCH)\n";
	}
	foreach $app (@includes) {
            $path = $macros{$app};
            next unless (-d "$path/bin/$arch");
	    print OUT "${app}_BIN = \$($app)/bin/$arch\n";
	}
	foreach $app (@includes) {
            $path = $macros{$app};
            next unless (-d "$path/lib/$arch");
	    print OUT "${app}_LIB = \$($app)/lib/$arch\n";
	}
    }
    foreach $app (@includes) {
        $path = $macros{$app};
        next unless (-d "$path/include");
	print OUT "RELEASE_INCLUDES += -I\$($app)/include/os/\$(OS_CLASS)\n";
	print OUT "RELEASE_INCLUDES += -I\$($app)/include\n";
    }
    foreach $app (@includes) {
        $path = $macros{$app};
        next unless (-d "$path/dbd");
        print OUT "RELEASE_DBDFLAGS += -I \$($app)/dbd\n";
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
	print OUT "-include \$($app)/configure/RULES_BUILD\n";
    }
    close OUT;
}

sub cdCommands {
    die "Architecture not set (use -a option)" unless ($arch);
    @includes = grep !/^TEMPLATE_TOP$/, @apps;
    
    # if -t <top> was given, substitute it in the startup path
    $startup = $cwd;
    $startup =~ s/.*(\/iocBoot\/.*)/$top$1/ if ($opt_t);
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "startup = \"$startup\"\n";
    print OUT "appbin = \"$top/bin/$arch\"\n";	# compatibility with R3.13.1
    
    foreach $app (@includes) {
        $path = $macros{$app};
	$lcapp = lc($app);
        print OUT "$lcapp = \"$path\"\n" if (-d $path);
	print OUT "${lcapp}bin = \"$path/bin/$arch\"\n" if (-d "$path/bin/$arch");
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
