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
use vars qw( $opt_a $opt_c $opt_d $opt_f $opt_m $opt_p $opt_t $opt_v $opt_x ); # declare these global to be shared with Getopt:Std

my $version = '$Id$ ';

# initialize variables: use getopts for these
getopts( 'a:c:dfm:p:t:vx' ) || die "\aSyntax: $0 [-a abspath] [-c cppdefs] [-d] [-f] [-m makefile] [-p program] [-t template] [-v] [-x] [targets]\n";
$opt_v = 1 if $opt_d;	# debug flag turns on verbose flag also
print "$0 $version\n" if $opt_v;

$opt_m = 'Makefile' unless $opt_m; # set default makefile name
print "Making makefile $opt_m ...\n" if $opt_v;

$opt_p = 'a.out' unless $opt_p;	# set default program name
my @targets = '.';		# current working directory is always included in targets
push @targets, @ARGV;		# then add remaining arguments on command line

if( $opt_a ) {			# add a trailing / if required
   local $/ = '/'; chomp $opt_a; $opt_a = "$opt_a/";
}

#some generic declarations
my( $file, $include, $line, $module, $name, $object, $path, $source, $suffix, $target, $word );
my @list;
#some constants
my $endline = $/;
my @src_suffixes = ( q/\.c/, q/\.cc/, q/\.cpp/, q/\.C/ );
my @inc_suffixes = ( q/\.h/, q/\.H/, q/\.inc/ );
push @inc_suffixes, @src_suffixes; # sourcefiles can be includefiles too

my %delim_match = ( q/'/ => q/'/,	# hash to find includefile delimiter pair
		    q/"/ => q/"/,
		    q/</ => q/>/ );

#formatting command for MAKEFILE, keeps very long lines to 256 characters
format MAKEFILE =
^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< \~
$line
.

sub print_formatted_list{
#this routine, in conjunction with the format line above, can be used to break up long lines
# it is currently used to break up the potentially long defs of SRC, OBJ, CPPDEFS, etc.
# not used for the dependency lists
   $line = "@_";
   local $: = " \t\n";		# the default formatting word boundary includes the hyphen, but not here
   while ( $opt_f && length $line > 254 ) {
      write MAKEFILE, $line;
   }
   print MAKEFILE $line unless $line eq '';
   print MAKEFILE "\n";
}

#begin writing makefile
open MAKEFILE, ">$opt_m" or die "\aERROR opening file $opt_m for writing: $!\n";
printf MAKEFILE "# Makefile created by %s $version\n\n", basename($0);

#if cppdefs flag is present, look for changes in cppdefs
my %chgdefs;
if ( $opt_c ) {
#split argument of -c into newdefs
   my %newdefs;
   foreach ( split /\s*-D/, $opt_c ) {
      $newdefs{$_} = 1;
   }
#get olddefs from file .cppdefs
   my %olddefs;
   my $cppdefsfile = '.cppdefs';
   if ( -f $cppdefsfile ) {
      open CPPFILE, $cppdefsfile or die "\aERROR opening cppdefsfile $cppdefsfile: $!\n";
      while ( <CPPFILE> ) {
	 foreach $word ( split ) {
	    $olddefs{$word} = 1;
	 }
      }
      close CPPFILE;
#get words that are not in both newdefs and olddefs
#if you move this foreach{} outside the enclosing if{} then
#   all cppdefs will be considered changed if there is no .cppdefs file.
      foreach ( keys %newdefs, keys %olddefs ) {
	 $chgdefs{$_} = 1 unless( $newdefs{$_} && $olddefs{$_} );
      }
   }
#write current cppdefs list to file .cppdefs
   open  CPPFILE, ">$cppdefsfile";
   my @newdefs = keys %newdefs;
   print CPPFILE " @newdefs\n";
   close CPPFILE;
   if( $opt_d ) {
      @list = keys %newdefs; print "newdefs= @list\n";
      @list = keys %olddefs; print "olddefs= @list\n";
      @list = keys %chgdefs; print "chgdefs= @list\n";
   }
}
delete $chgdefs{''};

# get a list of sourcefiles to be treated from targets
# (a sourcefile is any regular file with a suffix matching src_suffixes)
# if target is a sourcefile, add to list
# if target is a directory, get all sourcefiles there
# if target is a regular file that is not a sourcefile, look for a
#    sourcefile on last work of each line, rest of line (if present) is the
#    compile command to apply to this file.
#@sources will contain a unique list of sourcefiles in targets
#@objects will contain corresponding objects

#separate targets into directories and files
my %scanned;			# list of directories/files already scanned
my %source_of;			# hash returning sourcefile from object
foreach $target ( @targets ) {
#   print STDERR "." unless $opt_v; # show progress on screen (STDERR is used because it is unbuffered)
#directory
   if ( -d $target && !$scanned{$target} ) {
      print "Processing directory $target\n" if $opt_v;
      $scanned{$target} = 1;
   } elsif ( -f $target ) {
#file: check if it is a sourcefile
      ( $name, $path, $suffix ) = fileparse( $target, @src_suffixes );
      $object = "$name.o";
      if ( !$source_of{$object} ) {
	 if ( $suffix ) {
	    $path = '' if $path eq './';
	    if ( $opt_a and substr($path,0,1) ne '/' ) { # if an abs_path exists, attach it to all relative paths
	       local $/ = '/'; chomp $path;
	       $path = "$opt_a$path/";
	    }
	    $source_of{$object} = "$path$name$suffix";
	 } else {
#not a sourcefile: assume it contains list of sourcefiles
#specify files requiring special commands (e.g special compiler flags) thus:
#   f90 -Oaggress a.f90
#if last word on line is not a valid sourcefile, line is ignored
	    open CMDFILE, $target;
	    print "Reading commands from $target...\n" if $opt_v;
	    while ( <CMDFILE> ) {
	       $line = $_;
	       my @wordlist = split;
	       $file = @wordlist[$#wordlist];	       	# last word on line
	       ( $name, $path, $suffix ) = fileparse( $file, @src_suffixes );
	       $object = "$name.o";
	       if ( $suffix && !$source_of{$object} ) {
		  $path = '' if $path eq './';
		  if ( $opt_a and ( substr($path,0,1) ne '/' ) ) { # if an abs_path exists, attach it to all relative paths
		     local $/ = '/'; chomp $path;
		     $path = "$opt_a$path/";
		  }
		  $source_of{$object} = "$path$name$suffix";
#command for this file is all of line except the filename
		  $line =~ /\s+$file/; $line=$`;
	       }
	    }
	    close CMDFILE;
	 }
      }
   }
}
delete $source_of{''};
my @dirs = keys %scanned;
my @sources = values %source_of;
my @objects = keys   %source_of;
if( $opt_d ) {
   print "DEBUG: dirs= @dirs\n";
   print "DEBUG: sources= @sources\n";
   print "DEBUG: objects= @objects\n";
}

my %includes_in;		# hash of includes in a given source file (hashed against the corresponding object)
my %has_chgdefs;		# hash of files contains cppdefs that have been changed
#subroutine to scan file for use and module statements, and include files
# first argument is $object, second is $file
sub scanfile_for_keywords {
   my $object = shift;
   my $file = shift;
   local $/ = $endline;
#if file has already been scanned, return: but first check if any .o needs to be removed
   if( $scanned{$file} ) {
       return;
   }
   print "Scanning file $file of object $object ...\n" if $opt_v;
   open FILE, $file or die "\aERROR opening file $file of object $object: $!\n";
   foreach $line ( <FILE> ) {
      if( $line =~ /^[\#\s]*include\s*(['""'<])([\w\.\/]*)$delim_match{\1}/ix ) {
	 $includes_in{$file} .= ' ' . $2 if $2;
      }
      foreach ( keys %chgdefs ) {
	 $_ .= '='; /\s*=/; $word=$`; #cut string at =sign, else whole string
	 if( $line =~ /\b$word\b/ ) {
	     $has_chgdefs{$file} = 1;
	 }
      }
   }
   close FILE;
   $scanned{$file} = 1;
#   print "   uses modules=$modules_used_by{$object}, and includes=$includes_in{$file}.\n" if $opt_d;
   print "   uses includes=$includes_in{$file}.\n" if $opt_d;
}

foreach $object ( @objects ) {
   &scanfile_for_keywords( $object, $source_of{$object} );
}

my %off_sources;		# list of source files not in current directory
my %includes;			# global list of includes
my %used;			# list of object files that are used by others
my @includepaths;
my @cmdline;
# for each file in sources, write down dependencies on includes and modules
foreach $object ( @objects ) {
#   print STDERR "." unless $opt_v; # show progress on screen (STDERR is used because it is unbuffered)
#   my %is_used;			# hash of objects containing modules used by current object
   my %obj_of_include;		# hash of includes for current object
#   $is_used{$object} = 1;	# initialize with current object so as to avoid recursion
   print "Collecting dependencies for $object ...\n" if $opt_v;
   @cmdline = "$object: $source_of{$object}";
   ( $name, $path, $suffix ) = fileparse( $source_of{$object}, @src_suffixes );
   $off_sources{$source_of{$object}} = 1 unless( $path eq './' or $path eq '' );
#includes: done in subroutine since it must be recursively called to look for embedded includes
   @includepaths = '';
   &get_include_list( $object, $source_of{$object} );
#write the command line: if no file-specific command, use generic command for this suffix
   &print_formatted_list(@cmdline);

# subroutine to seek out includes recursively
   sub get_include_list {
      my( $incfile, $incname, $incpath, $incsuffix );
      my @paths;
      my $object = shift;
      my $file = shift;
      foreach ( split /\s+/, $includes_in{$file} ) {
	 print "object=$object, file=$file, include=$_.\n" if $opt_d;
	 ( $incname, $incpath, $incsuffix ) = fileparse( $_, @inc_suffixes );
	 if( $incsuffix ) {	# only check for files with proper suffix
	    undef $incpath if $incpath eq './';
	    if( $incpath =~ /^\// ) {
	       @paths = $incpath; # exact incpath specified, use it
	    } else {
	       @paths = reverse ( $path, @dirs ); # no incpath specified, check in path to source, or along directory list
	    }
	    foreach ( @paths ) {
	       local $/ = '/'; chomp; # remove trailing / if present
	       my $newincpath = "$_/$incpath" if $_;
	       undef $newincpath if $newincpath eq './';
	       $incfile = "$newincpath$incname$incsuffix";
	       print "DEBUG: checking for $incfile in $_ ...\n" if $opt_d;
	       if ( -f $incfile and $obj_of_include{$incfile} ne $object ) {
		  print "   found $incfile ...\n" if $opt_v;
		  push @cmdline, $incfile;
		  $includes{$incfile} = 1;
		  chomp( $newincpath, $path );
		  $off_sources{$incfile} = 1 if $newincpath;
		  $newincpath = '.' if $newincpath eq '';
		  push @includepaths, $newincpath unless( grep $_ eq $newincpath, @includepaths );
		  &scanfile_for_keywords($object,$incfile);
		  $obj_of_include{$incfile} = $object;
		  &get_include_list($object,$incfile); # recursively look for includes
		  last;
	       }
	    }
	 }
      }
   }
}

#lines to facilitate creation of local copies of source from other directories
#commented out because it makes make default rules kick in
#foreach ( keys %off_sources ) {
#    printf MAKEFILE "$_:\n", basename($_);
#}

#objects not used by other objects
#if every object is a module, then only the unused objects
#need to be passed to the linker (see commented OBJ = line below).
#if any f77 or C routines are present, we need complete list
my @unused_objects;
foreach $object ( @objects ) {
   push @unused_objects, $object unless $used{$object};
}

