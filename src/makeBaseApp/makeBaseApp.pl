eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if 0;                         # makeBaseApp 

# Authors: Ralph Lange and Marty Kraimer
# 1.15.2.2  2000/01/11 13:34:54

use Cwd;
use Getopt::Std;
use File::Copy;
use File::Find;
use File::Path;

use strict;

my $user = GetUser();
my $cwd  = cwd();
my $eAPPTYPE = $ENV{EPICS_MBA_DEF_APP_TYPE};
my $eTOP     = $ENV{EPICS_MBA_TEMPLATE_TOP};

my $Debug=0;

our $apptypename;
our $ioc;
our $app;
our $appdir;
our $appname;
our $apptype;
our $arch;
our $top;
our $epics_base;

my %findopts=(follow=>1, wanted=>\&FCopyTree);

use vars qw($opt_i $opt_d $opt_b $opt_T $opt_a $opt_l $opt_t);

&get_commandline_opts;		# Read and check options

#
# Declare two default callback routines for file copy plus two
# hook routines to add conversions
# These may be overriden within $top/$apptypename/Replace.pl

# First: the hooks
sub ReplaceFilenameHook { return $_[0]; }
sub ReplaceLineHook { return $_[0]; }

# ReplaceFilename
# called with the source (template) file or directory name, returns
# the "real" name (which gets the target after $top is removed)
# Empty string: Don't copy this file
sub ReplaceFilename { # (filename)
    my($file) = $_[0];
    $file =~ s|.*/CVS/?.*||;	# Ignore CVS files
    if($opt_i) {
	$file =~ s|/$apptypename|/iocBoot|;
    }
    if ($ioc) {			# iocBoot/ioc template has dynamic name
	$file =~ s|/iocBoot/ioc|/iocBoot/$ioc|;
	$file =~ s|_IOC_|$ioc|;
    } else {
	$file =~ s|.*/iocBoot/ioc/?.*||;
    }
    if ($app) {			# apptypenameApp itself is dynamic, too
	$file =~ s|/$apptypename|/$appdir|;
	$file =~ s|/$appdir/config|/config/$apptype|;
    }
    $file =~ s|_APPNAME_|$appname|;
    $file =~ s|_APPTYPE_|$apptype|;
				# We don't want the Replace overrides
    $file =~ s|.*/$appdir/Replace.pl$||;
    $file = &ReplaceFilenameHook($file); # Call the user-defineable hook
    return $file;
}

# ReplaceLine
# called with one line of a file, returns the line after replacing
# this and that
sub ReplaceLine { # (line)
    my($line) = $_[0];
    $line =~ s/_USER_/$user/o;
    $line =~ s/_EPICS_BASE_/$epics_base/o;
    $line =~ s/_ARCH_/$arch/o;
    $line =~ s/_APPNAME_/$appname/o;
    $line =~ s/_APPTYPE_/$apptype/o;
    $line =~ s/_TEMPLATE_TOP_/$top/o;
    if ($ioc) {
	$line =~ s/_IOC_/$ioc/o;
    }
    $line = &ReplaceLineHook($line); # Call the user-defineable hook
    return $line;
}

# Source replace overrides for file copy
if (-r "$top/$apptypename/Replace.pl") {
    require "$top/$apptypename/Replace.pl";
}

#
# Copy files and trees from <top> (non-App & non-Boot) if not present
#
opendir TOPDIR, "$top" or die "Can't open $top: $!";
foreach my $f ( grep !/^\.\.?$|^[^\/]*(App|Boot)/, readdir TOPDIR ) {

    if (-f "$f") {
	&CopyFile("$top/$f") unless (-e "$f");
    } else {
	find(\%findopts, "$top/$f") unless (-e "$f");
    }
}
closedir TOPDIR;

#
# Create ioc directories
#
if ($opt_i) {
    find(\%findopts, "$top/$apptypename") unless (-d "iocBoot");

    foreach $ioc ( @ARGV ) {
	($ioc =~ /^ioc/) or $ioc = "ioc" . $ioc;
        if (-d "iocBoot/$ioc") {
	    print "ioc iocBoot/$ioc is already there!\n";
	    next;
        }
	find(\%findopts, "$top/$apptypename/ioc");
    }
    exit 0;			# finished here for -i (no xxxApps)
}

#
# Create app directories (if any names given)
#
foreach $app ( @ARGV ) {
    ($appname = $app) =~ s/App$//;
    $appdir  = $appname . "App";
    if (-d "$appdir") {
	print "Application $appname is already there!\n";
	next;
    }
    print "Creating template structure "
	. "for $appname (of type $apptypename)\n" if $Debug; 
    find(\%findopts, "$top/$apptypename/");
}

exit 0;				# END OF SCRIPT

#
# Get commandline options and check for validity
#
sub get_commandline_opts { #no args
    (@ARGV) and getopts("ldit:T:b:a:") or Cleanup(1);

# Debug option
    $Debug = 1 if $opt_d;

# Locate epics_base
    my ($command) = UnixPath($0);
    if ($opt_b) {		# first choice is -b base
	$epics_base = UnixPath($opt_b);
    } elsif (-r "config/RELEASE") { # second choice is config/RELEASE
	open(IN, "config/RELEASE") or die "Cannot open config/RELEASE";
	while (<IN>) {
	    chomp;
            s/#.*$//; # remove all comments
	    s/^\s*EPICS_BASE\s*=\s*// and $epics_base = UnixPath($_);
	}
	close IN;
    } elsif ($command =~ m|/bin/|) { # assume script was called with full path to base
	$epics_base = $command;
	$epics_base =~ s|(/.*)/bin/.*makeBaseApp.*|$1|;
    }
    "$epics_base" or Cleanup(1, "Cannot find EPICS base");

# Locate template top directory
    if ($opt_T) {		# first choice is -T templ-top
	$top = UnixPath($opt_T);
    } elsif (-r "config/RELEASE") { # second choice is config/RELEASE
	open(IN, "config/RELEASE") or die "Cannot open config/RELEASE";
	while (<IN>) {
	    chomp;
            s/#.*$//; # remove all comments
	    s/^\s*TEMPLATE_TOP\s*=\s*// and $top = UnixPath($_);
	}
	close IN;
    }
    if("$top" eq "") { 
	if ($eTOP) {		# third choice is $ENV{EPICS_MBA_TEMPL_TOP}
	    $top = UnixPath($eTOP);
	} else {			# use templates from EPICS base
	    $top = $epics_base . "/templates/makeBaseApp/top";
	}
    }
    "$top" or Cleanup(1, "Cannot find template top directory");

# Print application type list?
    if ($opt_l) {
	&ListAppTypes;
	exit 0;			# finished for -l command
    }

# iocBoot and architecture stuff
    if ($opt_i) {
	if ($opt_a) {
	    $arch = $opt_a;
	} else {
	    print "What architecture do you want to use for your IOC,";
	    print "e.g. pc486, mv167 ? ";
	    $arch = <STDIN>;
	    chomp($arch);
	}
    }

# Application template type
    if ($opt_t) { # first choice is -t type
	$apptype = $opt_t; 
    } elsif ($eAPPTYPE) { # second choice is $ENV{EPICS_DEFAULT_APP_TYPE}
	$apptype = $eAPPTYPE;
    } elsif (-r "$top/defaultApp") {# third choice is (a link) in the $top dir
	$apptype = "default";
    } elsif (-r "$top/exampleApp") {# fourth choice is (a link) in the $top dir
	$apptype = "example";
    }
    $apptype =~ s/App$//;
    $apptype =~ s/Boot$//;
    "$apptype" or Cleanup(1, "Cannot find default application type");
    if ($opt_i) {			# fixed name when doing ioc dirs
	$apptypename = $apptype . "Boot";
    } else {
	$apptypename = $apptype . "App";
    }

# Valid $apptypename?
    unless (-r "$top/$apptypename") {
	print "Template for application type '$apptype' is unreadable or does not exist.\n";
	&ListAppTypes;
	exit 1;
    }

    print "\nCommand line / environment options validated:\n"
	. " Templ-Top: $top\n"
	. "Templ-Type: $apptype\n"
	. "Templ-Name: $apptypename\n"
	. "     opt_i: $opt_i\n"
	. "      arch: $arch\n"
	. "EPICS-Base: $epics_base\n\n" if $Debug;
}

#
# List application types
#
sub ListAppTypes { # no args
    print "Valid application types are:\n";
    foreach my $name (<$top/*App>) {
	$name =~ s|$top/||;
	$name =~ s|App||;
	printf "\t$name\n" if ($name && -r "$top/$name" . "App");
    }
    print "Valid iocBoot types are:\n";
    foreach my $name (<$top/*Boot>) {
	$name =~ s|$top/||;
	$name =~ s|Boot||;
	printf "\t$name\n" if ($name && -r "$top/$name" . "Boot");;
    }
}

#
# Copy a file with replacements
#
sub CopyFile { # (source)
    my $source = $_[0];
    my $target = &ReplaceFilename($source);

    if ($target) {
	$target =~ s|$top/||;
	open(INP, "<$source") and open(OUT, ">$target")
	    or die "$! Copying $source -> $target";

	print "Copying file $source -> $target\n" if $Debug;
	while (<INP>) {
	    print OUT &ReplaceLine($_);
	}
	close INP; close OUT;
    }
}
	
#
# Find() callback for file or structure copy
#
sub FCopyTree {
    my $dir;
    chdir $cwd;			# Sigh
    if (-d $File::Find::name
	and ($dir = &ReplaceFilename($File::Find::name))) {
	$dir =~ s|$top/||;
	print "Creating directory $dir\n" if $Debug;
	&mkpath($dir);
    } else {
	&CopyFile($File::Find::name);
    }
    chdir $File::Find::dir;
}

#
# Cleanup and exit
#
sub Cleanup { # (return-code [ messsage-line1, line 2, ... ])
    my ($rtncode, @message) = @_;

    foreach my $line ( @message ) {
	print "$line\n";
    }

    print <<EOF;
Usage:
$0 -l [options]
$0 -t type [options] app ...
             create application directories
$0 -i -t type [options] ioc ...
             create ioc boot directories
where
 app  Application name (the created directory will have \"App\" appended to name)
 ioc  IOC name (the created directory will have \"ioc\" prepended to name)

 -t type  Set the application type (-l for a list of valid types)
          If not specified, type is taken from environment
          If not found in environment, \"default\" is used
 -T top   Set the template top directory (where the application templates are)
          If not specified, top path is taken from config/RELEASE
          If config does not exist, top path is taken from environment
          If not found in environment, the templates from EPICS base are used
 -l       List valid application types for this installation
	  If this is specified the other options are not used
 -a arch  Set the IOC architecture (e.g. mv167)
          If not specified, you will be prompted
 -b base  Set the location of EPICS base (full path)
          If not specified, base path is taken from config/RELEASE
          If config does not exist, base path is taken from command
 -d       Verbose output (useful for debugging)

Environment:
EPICS_MBA_DEF_APP_TYPE  Application type you want to use as default
EPICS_MBA_TEMPLATE_TOP  Template top directory

Example: Create exampleApp 

<base>/bin/<arch>/makeBaseApp.pl -t example example
<base>/bin/<arch>/makeBaseApp.pl -i -t example example
EOF

    exit $rtncode;
}

sub GetUser { # no args
    my ($user);

    # add to this list if new possibilities arise,
    # currently it's UNIX and WIN32:
    $user = $ENV{USER} || $ENV{USERNAME} || Win32::LoginName();

    unless ($user) {
	print "I cannot figure out your user name.\n";
	print "What shall you be called ?\n";
	print ">";
	$user = <STDIN>;
	chomp $user;
    }
    die "No user name" unless $user;
    return $user;
}

# replace "\" by "/"  (for WINxx)
sub UnixPath { # path
    my($newpath) = $_[0];
    $newpath =~ s|\\|/|go;
    return $newpath;
}
