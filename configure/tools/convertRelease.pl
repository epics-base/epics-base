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
} else {		# Look for O.arch in current path
    $_ = $cwd;
    ($arch) = /.*\/O.([\w-]+)$/;
}

$hostarch = $arch;
$hostarch = $opt_h if ($opt_h);

if ($opt_t) {
    $top = $opt_t;
} else {		# 
    $top = $cwd;
    $top =~ s/\/iocBoot.*//;
    $top =~ s/\/configure\/O\..*//;
}

unless (@ARGV == 1) {
    print "Usage: convertRelease.pl [-a arch] [-h hostarch] [-t top] outfile\n";
    print "   where outfile is be one of:\n";
    print "\tcheckRelease - checks consistency of support apps\n";
    print "\tcdCommands - generate cd path strings for IOC use\n";
    print "\tCONFIG_APP_INCLUDE - additional build variables\n";
    print "\tRULES_INCLUDE - supports installable build rules\n";
    exit 2;
}
$outfile = $ARGV[0];

# TOP is set to this application for macro expansion purposes
$apps{TOP} = $top;

# Read the RELEASE file(s) into %apps
$relfile = "$top/configure/RELEASE";
die "Can't find configure/RELEASE file" unless (-r $relfile);
&readrelease($relfile, \%apps);

if ($hostarch) {
    $relfile .= ".$hostarch";
    &readrelease($relfile, \%apps) if (-r $relfile);
}

# This is a perl switch statement:
for ($outfile) {
    /CONFIG_APP_INCLUDE/ and do { &configAppInclude;	last; };
    /RULES_INCLUDE/	 and do { &rulesInclude;	last; };
    /cdCommands/	 and do { &cdCommands;		last; };
    /checkRelease/	 and do { &checkRelease;	last; };
    die "Output file type \'$outfile\' not supported";
}

sub readrelease {
    my ($file, $apps) = @_;
    # The $apps argument above is a reference to a hash
    my ($pre, $macro, $post, $path);
    local *IN;
    open(IN, $file) or die "Can't open $file: $!\n";
    while (<IN>) {
	chomp;
	s/\s*#.*$//;		# Remove trailing comments
        next if /^\s*$/;	# Skip blank lines
	
	# Expand all macros in the line:
	while (($pre,$macro,$post) = /(.*)\$\((\w+)\)(.*)/, $macro ne "") {
	    $_ = $pre . $apps->{$macro} . $post;
	}
	
	# Handle "<macro> = <path>"
	($macro, $path) = /^\s*(\w+)\s*=\s*(.*)/;
	if ($macro ne "") {
	    $apps->{$macro} = $path;
	    next;
	}
	# Handle "include <path>" syntax
	($path) = /^\s*include\s+(.*)/;
	&readrelease($path, $apps) if (-r $path);
    }
    close IN;
}

sub configAppInclude {
    delete $apps{TOP};
    delete $apps{EPICS_BASE};
    delete $apps{TEMPLATE_TOP};
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "# Do not modify this file, changes made here will\n";
    print OUT "# be lost when the application is next rebuilt.\n\n";
    
    if ($arch) {
	while (($app, $path) = each %apps) {
            next unless (-d "$path/bin/$hostarch");
	    print OUT "${app}_HOST_BIN = \$($app)/bin/\$(EPICS_HOST_ARCH)\n";
	}
	while (($app, $path) = each %apps) {
            next unless (-d "$path/bin/$arch");
	    print OUT "${app}_BIN = \$($app)/bin/$arch\n";
	}
	while (($app, $path) = each %apps) {
            next unless (-d "$path/lib/$arch");
	    print OUT "${app}_LIB = \$($app)/lib/$arch\n";
	}
    }
    while (($app, $path) = each %apps) {
        next unless (-d "$path/include");
	print OUT "RELEASE_INCLUDES += -I\$($app)/include/os/\$(OS_CLASS)\n";
	print OUT "RELEASE_INCLUDES += -I\$($app)/include\n";
    }
    while (($app, $path) = each %apps) {
        next unless (-d "$path/dbd");
        print OUT "RELEASE_DBDFLAGS += -I \$($app)/dbd\n";
    }
    close OUT;
}

sub rulesInclude {
    delete $apps{TOP};
    delete $apps{EPICS_BASE};
    delete $apps{TEMPLATE_TOP};
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "# Do not modify this file, changes made here will\n";
    print OUT "# be lost when the application is next rebuilt.\n\n";
    
    while (($app, $path) = each %apps) {
        next unless (-r "$path/configure/RULES_BUILD");
	print OUT "-include \$($app)/configure/RULES_BUILD\n";
    }
    close OUT;
}

sub cdCommands {
    die "Architecture not set (use -a option)" unless ($arch);
    delete $apps{TEMPLATE_TOP};
    
    # if -t was given, substitute it in the startup path
    $startup = $cwd;
    $startup =~ s/.*(\/iocBoot\/.*)/$top$1/ if ($opt_t);
    
    unlink($outfile);
    open(OUT,">$outfile") or die "$! creating $outfile";
    print OUT "startup = \"$startup\"\n";
    print OUT "appbin = \"$top/bin/$arch\"\n";	# compatibility with R3.13.1
    
    while (($app, $path) = each %apps) {
	$lcapp = lc($app);
        print OUT "$lcapp = \"$path\"\n" if (-d $path);
	print OUT "${lcapp}bin = \"$path/bin/$arch\"\n" if (-d "$path/bin/$arch");
    }
    close OUT;
}

sub checkRelease {
    $status = 0;
    delete $apps{TOP};
    delete $apps{TEMPLATE_TOP};
    
    while (($app, $path) = each %apps) {
	%check = (TOP => $path);
	$relfile = "$path/configure/RELEASE";
	&readrelease($relfile, \%check) if (-r $relfile);
	if ($hostarch) {
	    $relfile .= ".$hostarch";
	    &readrelease($relfile, \%check) if (-r $relfile);
	}
	delete $check{TOP};
	
	while (($parent, $ppath) = each %check) {
	    if (exists $apps{$parent} && ($apps{$parent} ne $ppath)) {
		print "\n" unless ($status);
		print "Definition of $parent conflicts with $app support.\n";
		print "In this application configure/RELEASE defines\n";
		print "\t$parent = $apps{$parent}\n";
		print "but $app at $path has\n";
		print "\t$parent = $ppath\n";
		$status = 1;
	    }
	}
    }
    print "\n" if ($status);
    exit $status;
}
