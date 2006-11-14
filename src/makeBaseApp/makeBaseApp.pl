eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeBaseApp 

# Authors: Ralph Lange, Marty Kraimer, Andrew Johnson and Janet Anderson
# $Id$

use Cwd;
use Getopt::Std;
use File::Find;
use File::Path;

$app_top  = cwd();

%release = (TOP => $app_top);
@apps   = (TOP);

$bad_ident_chars = '[^0-9A-Za-z_]';

&GetUser;		# Ensure we know who's in charge
&readRelease("configure/RELEASE", \%release, \@apps) if (-r "configure/RELEASE");
&readRelease("configure/RELEASE.$ENV{EPICS_HOST_ARCH}", \%release, \@apps)
   if (-r "configure/RELEASE.$ENV{EPICS_HOST_ARCH}");
&expandRelease(\%release, \@apps);
&get_commandline_opts;	# Check command-line options

#
# Declare two default callback routines for file copy plus two
# hook routines to add conversions
# These may be overriden within $top/$apptypename/Replace.pl

# First: the hooks
sub ReplaceFilenameHook { return $_[0]; }
sub ReplaceLineHook { return $_[0]; }

# ReplaceFilename
# called with the source (template) file or directory name, returns
# the target file/dir name (current directory is the application top).

# Inside iocBoot, templates can install different files for different
# IOC architectures or OSs: 'name@<arch>', 'name@<os>' & 'name@Common'
# The best match is installed as 'name', but if the best matching file
# is empty then no file is created, allowing a file 'name@Common' to
# be omitted by providing an empty 'name@<arch>' or 'name@<os>'.

# Returning an empty string means don't copy this file.
sub ReplaceFilename { # (filename)
    my($file) = $_[0];
    $file =~ s|.*/CVS/?.*||;	# Ignore CVS files and Replace.pl scripts
    $file =~ s|.*/$apptypename/Replace\.pl$||;
    
    if($opt_i) {
	# Handle name@arch stuff, copy only the closest matching file
	# NB: Won't work with directories, don't use '@' in a directory name!
	my($base,$filearch) = split /@/, $file;
	if ($base ne $file) {		# This file is arch-specific
	    my($os,$cpu,$toolset) = split /-/, $arch, 3;
	    if (-r "$base\@$arch") {	# A version exists for this arch
		$base = '' unless ($filearch eq $arch && -s $file);
	    } elsif (-r "$base\@$os") {	# A version exists for this os
		$base = '' unless ($filearch eq $os && -s $file);
	    } elsif (  $ENV{EPICS_HOST_ARCH} !~ "$os-$cpu" && 
                  -r "$base\@Cross" ) {	# Cross version exists
		$base = '' unless ($filearch eq "Cross" && -s $file);
	    } elsif (-r "$base\@Common") {	# Default version exists
		$base = '' unless ($filearch eq "Common" && -s $file);
	    } else {			# No default version
		$base = '';
	    }
	    $file = $base;	# Strip the @... part from the target name
	}
	$file =~ s|/$apptypename|/iocBoot|;	# templateBoot => iocBoot
    }
    if ($ioc) {
	$file =~ s|/iocBoot/ioc|/iocBoot/$ioc|;	# name the ioc subdirectory
	$file =~ s|_IOC_|$ioc|;
    } else {
	$file =~ s|.*/iocBoot/ioc/?.*||;	# Not doing IOCs here
    }
    if ($app) {
	$file =~ s|/$apptypename|/$appdir|;	# templateApp => namedApp
	$file =~ s|/$appdir/configure|/configure/$apptype|;
    }
    $file =~ s|_APPNAME_|$appname|;
    $file =~ s|_APPTYPE_|$apptype|;
    my $qmtop = quotemeta($top);
    $file =~ s|$qmtop/||;   # Change to the target location
    $file = &ReplaceFilenameHook($file); # Call the apptype's hook
    return $file;
}

# ReplaceLine
# called with one line of a file, returns the line after replacing
# this and that
sub ReplaceLine { # (line)
    my($line) = $_[0];
    $line =~ s/_IOC_/$ioc/g if ($ioc);
    $line =~ s/_USER_/$user/go;
    $line =~ s/_EPICS_BASE_/$app_epics_base/go;
    $line =~ s/_TEMPLATE_TOP_/$app_template_top/go;
    $line =~ s/_TOP_/$app_top/go;
    $line =~ s/_APPNAME_/$appname/g;
    $line =~ s/_CSAFEAPPNAME_/$csafeappname/g;
    $line =~ s/_APPTYPE_/$apptype/go;
    $line =~ s/_ARCH_/$arch/go if ($opt_i);
    $line = &ReplaceLineHook($line); # Call the apptype's hook
    return $line;
}

# Source replace overrides for file copy
if (-r "$top/$apptypename/Replace.pl") {
    require "$top/$apptypename/Replace.pl";
}

#
# Copy files and dirs from <top> (other than App & Boot) if not present
#
opendir TOPDIR, "$top" or die "Can't open $top: $!";
foreach $f ( grep !/^\.\.?$|^[^\/]*(App|Boot)/, readdir TOPDIR ) {
   find({wanted => \&FCopyTree, follow => 1}, "$top/$f") unless (-e "$f");
}
closedir TOPDIR;

#
# Create ioc directories
#
if ($opt_i) {
    find({wanted => \&FCopyTree, follow => 1}, "$top/$apptypename");

    $appname=$appnameIn if $appnameIn;
    foreach $ioc ( @names ) {
	($appname = $ioc) =~ s/App$// if !$appnameIn;
	($csafeappname = $appname) =~ s/$bad_ident_chars/_/og;
	$ioc = "ioc" . $ioc unless ($ioc =~ m/ioc/);
	if (-d "iocBoot/$ioc") {
	    print "iocBoot/$ioc exists, not modified.\n";
	    next;
	}
	find({wanted => \&FCopyTree, follow => 1}, "$top/$apptypename/ioc");
    }
    exit 0;			# finished here for -i (no xxxApps)
}

#
# Create app directories (if any names given)
#
foreach $app ( @names ) {
    ($appname = $app) =~ s/App$//;
    ($csafeappname = $appname) =~ s/$bad_ident_chars/_/og;
    $appdir  = $appname . "App";
    if (-d "$appdir") {
	print "$appname exists, not modified.\n";
	next;
    }
    print "Creating $appname from template type $apptypename\n" if $opt_d; 
    find({wanted => \&FCopyTree, follow => 1}, "$top/$apptypename/");
}

exit 0;				# END OF SCRIPT

#
# Get commandline options and check for validity
#
sub get_commandline_opts { #no args
    getopts("a:b:dhilp:T:t:") or Cleanup(1);
    
    # Options help
    Cleanup(0) if $opt_h;

    # Locate epics_base
    my ($command) = UnixPath($0);
    if ($opt_b) {		# first choice is -b base
	$epics_base = UnixPath($opt_b);
    } elsif ($release{"EPICS_BASE"}) { # second choice is configure/RELEASE
	$epics_base = UnixPath($release{"EPICS_BASE"});
	$epics_base =~s|^\$\(TOP\)/||;
    } elsif ($command =~ m|/bin/|) { # assume script was run with full path to base
	$epics_base = $command;
	$epics_base =~ s|^(.*)/bin/.*makeBaseApp.*|$1|;
    }
    $epics_base and -d "$epics_base" or Cleanup(1, "Can't find EPICS base");
    $app_epics_base = LocalPath($epics_base);
    $app_epics_base =~ s|^\.\.|\$(TOP)/..|;

    # Locate template top directory
    if ($opt_T) {		# first choice is -T templ-top
	$top = UnixPath($opt_T);
    } elsif ($release{"TEMPLATE_TOP"}) { # second choice is configure/RELEASE
	$top = UnixPath($release{"TEMPLATE_TOP"});
	$top =~s|^\$\(EPICS_BASE\)|$epics_base|;
	$top =~s|^\$\(TOP\)/||;
    }
    $top = $ENV{EPICS_MBA_TEMPLATE_TOP} unless $top; # third choice is env var
    $top = $epics_base . "/templates/makeBaseApp/top" unless $top; # final
    $top and -d "$top" or Cleanup(1, "Can't find template top directory");
    $app_template_top = LocalPath($top);
    $app_template_top =~s|^\.\.|\$(TOP)/..|;
    $app_template_top =~s|^$epics_base/|\$\(EPICS_BASE\)/|;

    # Print application type list?
    if ($opt_l) {
	&ListAppTypes;
	exit 0;			# finished for -l command
    }
    
    if (!@ARGV){
	if ($opt_t) {
	    if ($opt_i) {
		my @iocs = map {s/iocBoot\///; $_} glob 'iocBoot/ioc*';
		if (@iocs) {
		    print "The following IOCs already exist here:\n",
			  map {"    $_\n"} @iocs;
		}
		print "Name the IOC(s) to be created.\n",
		      "Names given will have \"ioc\" prepended to them.\n",
		      "IOC names? ";
	    } else {
		print "Name the application(s) to be created.\n",
		      "Names given will have \"App\" appended to them.\n",
		      "Application names? ";
	    }
	    $namelist = <STDIN>;
	    chomp($namelist);
	    @names = split /[\s,]/, $namelist;
	} else {
	    Cleanup(1);
	} 
    } else {
	@names = @ARGV;
    } 

    # ioc architecture and application name
    if ($opt_i && @names) {

	# ioc architecture
	opendir BINDIR, "$epics_base/bin" or die "Can't open $epics_base/bin: $!";
	my @archs = grep !/^\./, readdir BINDIR;	# exclude .files
	closedir BINDIR;
	if ($opt_a) {
	    $arch = $opt_a;
	} elsif (@archs == 1) {
	    $arch = $archs[0];
	    print "Using target architecture $arch (only one available)\n";
	} else {
	    print "The following target architectures are available in base:\n";
	    foreach $arch (@archs) {
		print "    $arch\n";
	    }
	    print "What architecture do you want to use? ";
	    $arch = <STDIN>;
	    chomp($arch);
	}
	grep /^$arch$/, @archs or Cleanup(1, "Target architecture $arch not available");

        # Application name
	if ($opt_p){
	    $appnameIn = $opt_p if ($opt_p);
	} else {
	    my @apps = glob '*App';
	    if (@apps) {
		print "The following applications are available:\n",
		      map {s/App$//; "    $_\n"} @apps;
	    }
	    print "What application should the IOC(s) boot?\n",
		  "The default uses the IOC's name, even if not listed above.\n",
		  "Application name? ";
	    $appnameIn = <STDIN>;
	    chomp($appnameIn);
	}
    }

    # Application type
    $appext = $opt_i ? "Boot" : "App";
    if ($opt_t) { # first choice is -t type
	$apptype = $opt_t; 
	$apptype =~ s/$appext$//;
    } elsif ($ENV{EPICS_MBA_DEF_APP_TYPE}) { # second choice is environment var
	$apptype = $ENV{EPICS_MBA_DEF_APP_TYPE};
	$apptype =~ s/(App)|(Boot)$//;
    } elsif (-d "$top/default$appext") { # third choice is default
	$apptype = "default";
    } elsif (-d "$top/example$appext") { # fourth choice is example
	$apptype = "example";
    }
    $apptype or Cleanup(1, "No application type set");
    $apptypename = $apptype . $appext;
    (-r "$top/$apptypename") or
	Cleanup(1, "Can't access template directory '$top/$apptypename'.\n");

    print "\nCommand line / environment options validated:\n"
	. " Templ-Top: $top\n"
	. "Templ-Type: $apptype\n"
	. "Templ-Name: $apptypename\n"
	. "     opt_i: $opt_i\n"
	. "      arch: $arch\n"
	. "EPICS-Base: $epics_base\n\n" if $opt_d;
}

#
# Parse a configure/RELEASE file.
#
# NB: This subroutine also appears in base/configure/tools/convertRelease.pl
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
        next if /^\s*$/;	# Skip blank lines
	
	# Expand all already-defined macros in the line:
	while (($pre,$var,$post) = /(.*)\$\((\w+)\)(.*)/) {
	    last unless (exists $Rmacros->{$var});
	    $_ = $pre . $Rmacros->{$var} . $post;
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

sub expandRelease {
    my ($Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array
    
    # Expand any (possibly nested) macros that were defined after use
    while (($macro, $path) = each %$Rmacros) {
	while (($pre,$var,$post) = $path =~ /(.*)\$\((\w+)\)(.*)/) {
	    $path = $pre . $Rmacros->{$var} . $post;
	    $Rmacros->{$macro} = $path;
	}
    }
}

#
# List application types
#
sub ListAppTypes { # no args
    opendir TYPES, "$top" or die "Can't open $top: $!";
    my @allfiles = readdir TYPES;
    closedir TYPES;
    my @apps = grep /.*App$/, @allfiles;
    my @boots = grep /.*Boot$/, @allfiles;
    print "Valid application types are:\n";
    foreach $name (@apps) {
	$name =~ s|App||;
	printf "\t$name\n" if ($name && -r "$top/$name" . "App");
    }
    print "Valid iocBoot types are:\n";
    foreach $name (@boots) {
	$name =~ s|Boot||;
	printf "\t$name\n" if ($name && -r "$top/$name" . "Boot");;
    }
}

#
# Copy a file with replacements
#
sub CopyFile { # (source)
    $source = $_[0];
    $target = &ReplaceFilename($source);

    if ($target and !-e $target) {
	open(INP, "<$source") and open(OUT, ">$target")
	    or die "$! Copying $source -> $target";

	print "Copying file $source -> $target\n" if $opt_d;
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
    chdir $app_top;		# Sigh
    if (-d "$File::Find::name"
	and ($dir = &ReplaceFilename($File::Find::name))) {
	print "Creating directory $dir\n" if $opt_d;
	&mkpath($dir) unless (-d "$dir");
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

    if (@message) {
	print join("\n", @message), "\n";
    } else {
	&Usage;
    }
    exit $rtncode;
}

sub Usage {
    print <<EOF;
Usage:
<base>/bin/<arch>/makeBaseApp.pl -h
             display help on command options
<base>/bin/<arch>/makeBaseApp.pl -l [options]
             list application types
<base>/bin/<arch>/makeBaseApp.pl -t type [options] [app ...]
             create application directories
<base>/bin/<arch>/makeBaseApp.pl -i -t type [options] [ioc ...]
             create ioc boot directories
where
 app  Application name (the created directory will have \"App\" appended)
 ioc  IOC name (the created directory will have \"ioc\" prepended)
EOF
    print <<EOF if ($opt_h);

 -a arch  Set the IOC architecture for use with -i (e.g. vxWorks-68040)
          If arch is not specified, you will be prompted
 -b base  Set the location of EPICS base (full path)
          If not specified, base path is taken from configure/RELEASE
          If configure does not exist, base path is taken from command
 -d       Enable debug messages
 -i       Specifies that ioc boot directories will be generated
 -l       List valid application types for this installation
	  If this is specified the other options are not used
 -p app   Set the application name for use with -i
          If not specified, you will be prompted
 -T top   Set the template top directory (where the application templates are)
          If not specified, top path is taken from configure/RELEASE
          If configure does not exist, top path is taken from environment
          If not found in environment, the templates from EPICS base are used
 -t type  Set the application type (-l for a list of valid types)
          If not specified, type is taken from environment
          If not found in environment, \"default\" is used

Environment:
EPICS_MBA_DEF_APP_TYPE  Application type you want to use as default
EPICS_MBA_TEMPLATE_TOP  Template top directory

Example: Create exampleApp 

<base>/bin/<arch>/makeBaseApp.pl -t example example
<base>/bin/<arch>/makeBaseApp.pl -i -t example example
EOF
}

sub GetUser {
    # add to this list if new possibilities arise,
    # currently it's UNIX and WIN32:
    $user = $ENV{USER} || $ENV{USERNAME} || Win32::LoginName();
    $user =~ s/\s+//g;   # Bl**dy Windows stupidity...

    unless ($user) {
	print "Strange, I cannot figure out your user name!\n";
	print "What should you be called ? ";
	$user = <STDIN>;
	chomp $user;
    }
    die "No user name" unless $user;
}

# Path rewriting rules for various OSs
# These functions are duplicated in configure/convertRelease.pl
sub UnixPath {
    my ($newpath) = @_;
    if ($^O eq "cygwin") {
	$newpath =~ s|\\|/|go;
	$newpath =~ s%^([a-zA-Z]):/%/cygdrive/$1/%;
    } elsif ($^O eq 'MSWin32') {
	$newpath =~ s|\\|/|go;
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
