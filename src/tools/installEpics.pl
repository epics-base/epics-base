#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# InstallEpics.pl
#
# InstallEpics is used within makefiles to copy new versions of
# files into a destination directory.
#
# Author: Kay Kasemir 02-04-1997

use strict;

use File::Basename;
use Getopt::Std;
use File::Path;
use File::Copy;

my $tool = basename($0);
my $mode = 0755;

our ($opt_d, $opt_h, $opt_m, $opt_q);

getopt "m";
$mode = oct $opt_m if $opt_m;

Usage() if $opt_h;

Usage('Nothing to install') if @ARGV < 2;

my $install_dir = pop @ARGV;    # Last arg

$install_dir =~ s[\\][/]g if $^O eq 'cygwin' || $^O eq 'MSWin32';
$install_dir =~ s[/$][];	# remove trailing '/'
$install_dir =~ s[//][/]g;	# replace '//' by '/'

# Do we have to create the directory?
unless (-d $install_dir || -l $install_dir) {
    # Create dir only if -d option given
    die "$tool: Directory $install_dir does not exist" unless $opt_d;

    # Create all the subdirs that lead to $install_dir
    mkpath($install_dir, !$opt_q, 0777);
}

foreach my $source (@ARGV) {
    die "$tool: No such file '$source'" unless -f $source;

    my $basename = basename($source);
    my $target   = "$install_dir/$basename";
    my $temp     = "$target.$$";

    if (-f $target) {
        next if -M $target < -M $source and -C $target < -C $source;
        # Remove old target, making sure it is deletable first
        chmod 0777, $target;
        unlink $target;
    }

    # Using copy + rename fixes problems with parallel builds
    copy($source, $temp) or die "$tool: Copy failed: $!\n";
    rename $temp, $target or die "$tool: Rename failed: $!\n";

    # chmod 0555 <read-only> DOES work on Win32, but the above
    # chmod 0777 fails to install a newer version on top.
    chmod $mode, $target unless $^O eq 'MSWin32';
}

sub Usage {
    my ($txt) = @_;
    my $omode = sprintf '%#o', $mode;

    print << "END";
Usage: $tool [OPTIONS]... SRCS... DEST
  -d        Create non-existing directories
  -h        Print usage
  -m mode   Octal permissions for installed files ($omode by default)
  -q        Install quietly
  SRCS      Source files to be installed
  DEST      Destination directory
END

    print "\n$txt\n" if $txt;

    exit $opt_h ? 0 : 2;
}
