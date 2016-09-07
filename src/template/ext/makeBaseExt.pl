#!/usr/bin/env perl

# Authors: Ralph Lange, Marty Kraimer, Andrew Johnson and Janet Anderson

use Cwd;
use Getopt::Std;
use File::Copy;
use File::Find;
use File::Path;

$user = GetUser();
$cwd  = cwd();
$eEXTTYPE = $ENV{EPICS_MBE_DEF_EXT_TYPE};
$eTOP     = $ENV{EPICS_MBE_TEMPLATE_TOP};
$eBASE    = $ENV{EPICS_MBE_BASE};

get_commandline_opts();		# Read and check options

$extname = "@ARGV";

#
# Declare two default callback routines for file copy plus two
# hook routines to add conversions
# These may be overriden within $top/$exttypename/Replace.pl

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
    if ($ext) {			# exttypenameExt itself is dynamic, too
	$file =~ s|/$exttypename|/$extdir|;
	$file =~ s|/$extdir/configure|/configure/$exttype|;
    }
    $file =~ s|_EXTNAME_|$extname|;
    $file =~ s|_EXTTYPE_|$exttype|;
				# We don't want the Replace overrides
    $file =~ s|.*/$extdir/Replace.pl$||;
    $file = ReplaceFilenameHook($file); # Call the user-defineable hook
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
    $line =~ s/_EXTNAME_/$extname/o;
    $line =~ s/_EXTTYPE_/$exttype/o;
    $line =~ s/_TEMPLATE_TOP_/$top/o;
    $line = ReplaceLineHook($line); # Call the user-defineable hook
    return $line;
}

# Source replace overrides for file copy
if (-r "$top/$exttypename/Replace.pl") {
    require "$top/$exttypename/Replace.pl";
}

#
# Copy files and trees from <top> (non-Ext) if not present
#
opendir TOPDIR, "$top" or die "Can't open $top: $!";
foreach $f ( grep !/^\.\.?$|^[^\/]*(Ext)/, readdir TOPDIR ) {
    if (-f "$f") {
	CopyFile("$top/$f") unless (-e "$f");
    } else {
	$note = yes  if ("$f" eq "src" && -e "$f");
	find(\&FCopyTree, "$top/$f") unless (-e "$f");
    }
}
closedir TOPDIR;

#
# Create ext directories (if any names given)
#
$cwdsave  = $cwd;
$cwd = "$cwd/src";
foreach $ext ( @ARGV ) {
    ($extname = $ext) =~ s/Ext$//;
    $extdir  = $extname;
    if (-d "src/$extdir") {
	print "Extention $extname is already there!\n";
	next;
    }
    print "Creating template structure "
	. "for $extname (of type $exttypename)\n" if $Debug; 
    find(\&FCopyTree, "$top/$exttypename/");
    if ($note) {
    print "\nNOTE: You must add the line \"DIRS += $extname\" to src/Makefile.\n\n";
	}
}
$cwd  = $cwdsave;

exit 0;				# END OF SCRIPT

#
# Get commandline options and check for validity
#
sub get_commandline_opts { #no args
    ($len = @ARGV) and getopts("ldit:T:b:a:") or Cleanup(1);

# Debug option
    $Debug = 1 if $opt_d;

# Locate epics_base
    my ($command) = UnixPath($0);
    if ($opt_b) {		# first choice is -b base
	$epics_base = UnixPath($opt_b);
    } elsif (-r "configure/RELEASE") { # second choice is configure/RELEASE
	open(IN, "configure/RELEASE") or die "Cannot open configure/RELEASE";
	while (<IN>) {
	    chomp;
	    s/EPICS_BASE\s*=\s*// and $epics_base = UnixPath($_), break;
	}
	close IN;
    } elsif ($eBASE) { # third choice is env var EPICS_MBE_BASE
        $epics_base = UnixPath($eBASE);
    } elsif ($command =~ m|/bin/|) { # assume script was called with full path to base
	$epics_base = $command;
	$epics_base =~ s|(/.*)/bin/.*makeBaseExt.*|$1|;
    }
    "$epics_base" or Cleanup(1, "Cannot find EPICS base");

# Locate template top directory
    if ($opt_T) {		# first choice is -T templ-top
	$top = UnixPath($opt_T);
    } elsif (-r "configure/RELEASE") { # second choice is configure/RELEASE
	open(IN, "configure/RELEASE") or die "Cannot open configure/RELEASE";
	while (<IN>) {
	    chomp;
	    s/TEMPLATE_TOP\s*=\s*// and $top = UnixPath($_), break;
	}
	close IN;
    }
    if("$top" eq "") { 
	if ($eTOP) {		# third choice is $ENV{EPICS_MBE_TEMPL_TOP}
	    $top = UnixPath($eTOP);
	} else {			# use templates from EPICS base
	    $top = $epics_base . "/templates/makeBaseExt/top";
	}
    }
    "$top" or Cleanup(1, "Cannot find template top directory");

# Print extension type list?
    if ($opt_l) {
	ListExtTypes();
	exit 0;			# finished for -l command
    }

# Extention template type
    if ($opt_t) { # first choice is -t type
	$exttype = $opt_t; 
    } elsif ($eEXTTYPE) { # second choice is $ENV{EPICS_DEFAULT_EXT_TYPE}
	$exttype = $eEXTTYPE;
    } elsif (-r "$top/defaultExt") {# third choice is (a link) in the $top dir
	$exttype = "default";
    } elsif (-r "$top/exampleExt") {# fourth choice is (a link) in the $top dir
	$exttype = "example";
    }
    $exttype =~ s/Ext$//;
    "$exttype" or Cleanup(1, "Cannot find default extension type");
    $exttypename = $exttype . "Ext";

# Valid $exttypename?
    unless (-r "$top/$exttypename") {
	print "Template for extension type '$exttype' is unreadable or does not exist.\n";
	ListExtTypes();
	exit 1;
    }

    print "\nCommand line / environment options validated:\n"
	. " Templ-Top: $top\n"
	. "Templ-Type: $exttype\n"
	. "Templ-Name: $exttypename\n"
	. "EPICS-Base: $epics_base\n\n" if $Debug;

}

#
# List extension types
#
sub ListExtTypes { # no args
    print "Valid extension types are:\n";
    foreach $name (<$top/*Ext>) {
	$name =~ s|$top/||;
	$name =~ s|Ext||;
	printf "\t$name\n" if ($name && -r "$top/$name" . "Ext");
    }
}

#
# Copy a file with replacements
#
sub CopyFile { # (source)
    $source = $_[0];
    $target = ReplaceFilename($source);

    if ($target) {
	$target =~ s|$top/||;
	open(INP, "<$source") and open(OUT, ">$target")
	    or die "$! Copying $source -> $target";

	print "Copying file $source -> $target\n" if $Debug;
	while (<INP>) {
	    print OUT ReplaceLine($_);
	}
	close INP; close OUT;
    }
}
	
#
# Find() callback for file or structure copy
#
sub FCopyTree {
    chdir $cwd;			# Sigh
    if (-d $File::Find::name
	and ($dir = ReplaceFilename($File::Find::name))) {
	$dir =~ s|$top/||;
	print "Creating directory $dir\n" if $Debug;
	mkpath($dir);
    } else {
	CopyFile($File::Find::name);
    }
    chdir $File::Find::dir;
}

#
# Cleanup and exit
#
sub Cleanup { # (return-code [ messsage-line1, line 2, ... ])
    my ($rtncode, @message) = @_;

    foreach $line ( @message ) {
	print "$line\n";
    }

    print <<EOF;
Usage:
$0 -l [options]
$0 -t type [options] ext ...
             create extension directories
where
 ext  Ext name

 -t type  Set the extension type (-l for a list of valid types)
          If not specified, type is taken from environment
          If not found in environment, \"default\" is used
 -T top   Set the template top directory (where the extension templates are)
          If not specified, top path is taken from configure/RELEASE
          If configure does not exist, top path is taken from environment
          If not found in environment, the templates from EPICS base are used
 -l       List valid extension types for this installation
	  If this is specified the other options are not used
 -b base  Set the location of EPICS base (full path)
          If not specified, base path is taken from configure/RELEASE
          If configure does not exist, from environment
          If not found in environment, from makeBaseApp.pl location
 -d       Verbose output (useful for debugging)

Environment:
EPICS_MBE_DEF_EXT_TYPE  Ext type you want to use as default
EPICS_MBE_TEMPLATE_TOP  Template top directory
EPICS_MBE_BASE          Location of EPICS base

Example: Create example extension 

<base>/bin/<arch>/makeBaseExt.pl -t example example

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
