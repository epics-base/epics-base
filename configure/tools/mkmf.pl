#!/usr/bin/perl
#-----------------------------------------------------------------------
#              mkmf.pl: Perl script to create include file  dependancies
#
# mkmf.pl is mkmf modified for the EPICS system to output only include
# file dependancies. It is intendend as a fallback when the gnu compilers
# gcc and g++ are not available, and it has the following limitations.
#
# 1) Only handles the #include preprocessor command. Does not understand
#    the preproceeor commands #define, #if, #ifdef, #ifndef, ...
# 2) Does not know a compilers internal macro definitions e.g.
#    __cplusplus, __STDC__
# 3) Does not keep track of the macros defined in #include files so can't
#    do #ifdefs #ifndef ...
# 4) Does not know where system include files are located
# 
#-----------------------------------------------------------------------
#              mkmf: Perl script for makefile construction
#
# AUTHOR: V. Balaji (vb@gfdl.gov)
#         SGI/GFDL Princeton University
#
# Full web documentation for mkmf:
#     http://www.gfdl.gov/~vb/mkmf.html
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# For the full text of the GNU General Public License,
# write to: Free Software Foundation, Inc.,
#           675 Mass Ave, Cambridge, MA 02139, USA.
#-----------------------------------------------------------------------

require 5;
use strict;
use File::Basename;
use Getopt::Std;
use vars qw( $opt_d $opt_m $opt_v ); # declare these global to be shared with Getopt:Std

my $version = '$Id$ ';

# initialize variables: use getopts for these
getopts( 'dm:v' ) || die "\aSyntax: $0 [-d] [-m makefile] [-v] [targets]\n";
$opt_v = 1 if $opt_d;    # debug flag turns on verbose flag also
print "$0 $version\n" if $opt_v;

$opt_m = 'depends' unless $opt_m; # set default file name
print "Making depends file  $opt_m ...\n" if $opt_v;

my @dirs;
my $i;
foreach $i (0 .. $#ARGV-2) {
    push @dirs, $ARGV[$i];
}
#some generic declarations
my( $file, $line);

my %scanned;                   # list of directories/files already scanned
my $object = $ARGV[$#ARGV-1];  # object file
my $source = $ARGV[$#ARGV];    # sourcefile from object

#some constants
my $endline = $/;
my %delim_match = ( q/'/ => q/'/,    # hash to find includefile delimiter pair
            q/"/ => q/"/,
            q/</ => q/>/ );

#formatting command for MAKEFILE, keeps very long lines to 80 characters
format MAKEFILE =
^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< \~
$line
.

sub print_formatted_list{
#this routine, in conjunction with the format line above, can be used to break up long lines
# it is currently used to break up the potentially long defs of SRC, OBJ, CPPDEFS, etc.
# not used for the dependency lists
   $line = "@_";
   local $: = " \t\n"; # the default formatting word boundary includes the hyphen, but not here
   while ( length $line > 78 ) {
      write MAKEFILE, $line;
   }
   print MAKEFILE $line unless $line eq '';
   print MAKEFILE "\n";
}

#begin writing makefile
open MAKEFILE, ">$opt_m" or die "\aERROR opening file $opt_m for writing: $!\n";
printf MAKEFILE "# Makefile created by %s $version\n\n", basename($0);

if( $opt_d ) {
   print "DEBUG: dirs= @dirs\n";
   print "DEBUG: source= $source\n";
   print "DEBUG: object= $object\n";
}

my %includes_in;  # hash of includes in a given source file (hashed against the corresponding object)
#subroutine to scan file for include files
# first argument is $object, second is $file
sub scanfile_for_keywords {
   my $object = shift;
   my $file = shift;
   local $/ = $endline;
   #if file has already been scanned, return
   return if $scanned{$file};
   print "Scanning file $file of object $object ...\n" if $opt_v;
   open FILE, $file or die "\aERROR opening file $file of object $object: $!\n";
   foreach $line ( <FILE> ) {
      next if !($line =~ m/^[ 	]*\#[ 	]*include/);
      if( $line =~ /^[\#\s]*include\s*(['""'<])([\w\.\/]*)$delim_match{\1}/ix ) {
     $includes_in{$file} .= ' ' . $2 if $2;
      }
   }
   close FILE;
   $scanned{$file} = 1;
   print "   uses includes=$includes_in{$file}.\n" if $opt_d;
}

&scanfile_for_keywords( $object, $source );

my %includes;            # global list of includes
my @cmdline;
my %obj_of_include;        # hash of includes for current object
print "Collecting dependencies for $object ...\n" if $opt_v;
@cmdline = "$object: $source";
#includes: done in subroutine since it must be recursively called to look for embedded includes
&get_include_list( $object, $source );
#write the command line: if no file-specific command, use generic command for this suffix
&print_formatted_list(@cmdline);

# subroutine to seek out includes recursively
sub get_include_list {
  my( $incfile, $incname, $incpath );
  my @paths;
  my $object = shift;
  my $file = shift;
  my $include;
  foreach ( split /\s+/, $includes_in{$file} ) {
     print "object=$object, file=$file, include=$_.\n" if $opt_d;
     if ( $scanned{"$_"} ) {
        print "DEBUG: skipping scanned file $_\n" if $opt_d;
        next;
     }
     $include = $_;
     $incname = basename($_);
     next if ( $incname eq '' );
     $incpath = dirname($_);
     undef $incpath if $incpath eq './';
     undef $incpath if $incpath eq '.';
     if( $incpath =~ /^\// ) {
        @paths = $incpath; # exact incpath specified, use it
     } else {
        @paths = @dirs;
     }
     foreach ( @paths ) {
        local $/ = '/'; chomp; # remove trailing / if present
        my $newincpath = "$_/$incpath" if $_;
        undef $newincpath if $newincpath eq './';
        $incfile = "$newincpath$incname";
        print "DEBUG: checking for $incfile in $_\n" if $opt_d;
        if ( -f $incfile and $obj_of_include{$incfile} ne $object ) {
            push @cmdline, $incfile;
            $includes{$incfile} = 1;
            chomp( $newincpath );
            $newincpath = '.' if $newincpath eq '';
            &scanfile_for_keywords($object,$incfile);
            $obj_of_include{$incfile} = $object;
            &get_include_list($object,$incfile); # recursively look for includes
            last;
         }
      }
      $scanned{"$include"} = 1;
   }
}

