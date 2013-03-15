#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#-----------------------------------------------------------------------
# mkmf.pl: Perl script to create #include file dependencies
#
# Limitations:
#
# 1) Only handles the #include preprocessor command. Does not understand
#    the preproceeor commands #define, #if, #ifdef, #ifndef, ...
# 2) Does not know a compilers internal macro definitions e.g.
#    __cplusplus, __STDC__, __GNUC__
# 3) Does not keep track of the macros defined in #include files so can't
#    do #ifdefs #ifndef ...
# 4) Does not know where system include files are located
# 5) Accepts #include lines with single, double or angle-quoted file names
# 6) Accepts -Mxxx options for compatibility with msi, but ignores them
#
#-----------------------------------------------------------------------

use strict;

use FindBin;
use lib "$FindBin::Bin/../../lib/perl";

use EPICS::Getopts;

my $tool = 'mkmf.pl';
my $endline = $/;
my %output;
my @includes;

our ( $opt_d, $opt_m, @opt_I, @opt_M);
getopts( 'dm:I@M@' ) ||
    die "\aSyntax: $0 [-d] [-m dependsFile] [-I incdir] objFile srcFile [srcfile]... \n";
my $debug = $opt_d;
my $depFile = $opt_m;
my @incdirs = @opt_I;
my $objFile = shift or die "No target file argument";
my @srcFiles=@ARGV;

if( $debug ) {
   print "$0 $tool\n";
   print "DEBUG: incdirs= @incdirs\n";
   print "DEBUG: objFile= $objFile\n";
   print "DEBUG: srcFiles= @srcFiles\n";
}

print "Generating dependencies for $objFile\n" if $debug;

foreach my $srcFile (@srcFiles) {
   scanFile($srcFile);
   scanIncludesList();
}

$depFile = 'depends' unless $depFile;

print "Creating file $depFile\n" if $debug;
printList($depFile,$objFile);

print "\n ALL DONE \n\n" if $debug;



#----------------------------------------
sub printList{
   my $depFile = shift; 
   my $objFile = shift; 
   my $file; 

   unlink($depFile) or die "Can't delete $depFile: $!\n" if -f $depFile;

   open DEPENDS, ">$depFile" or die "\aERROR opening file $depFile for writing: $!\n";

   my $old_handle = select(DEPENDS);

   print "# DO NOT EDIT: This file created by $tool\n\n";

   foreach $file (@includes) {
       print "$objFile : $file\n";
   }
   print "\n\n";

   select($old_handle) ; # in this case, STDOUT
}

#-------------------------------------------
# scan file for #includes
sub scanFile {
   my $file = shift;
   my $incfile;
   my $line;
   print "Scanning file $file\n" if $debug;
   open FILE, $file or return;
   foreach $line ( <FILE> ) {
      $incfile = findNextIncName($line,$file=~/\.substitutions$/);
      next if !$incfile;
      next if $output{$incfile};   
      push @includes,$incfile;
      $output{$incfile} = 1;
   }
   close FILE;
}

#------------------------------------------
# scan files in includes list
sub scanIncludesList {
   my $file;
   foreach $file (@includes) {
      scanFile($file);
   }
}

#-----------------------------------------
# find filename on #include and file lines
sub findNextIncName {
   my $line = shift;
   my $is_subst = shift;
   my $incname = "";
   my $incfile = 0;
   my $dir;

   local $/ = $endline;
   if ($is_subst) {
      return 0 if not $line =~ /^\s*file\s*([^\s{]*)/;
      $incname = $1;
      $incname = substr $incname, 1, length($incname)-2 if $incname =~ /^".+?"$/;
   } else {
      return 0 if not $line =~ /^#?\s*include\s*('.*?'|<.*?>|".*?")/;
      $incname = substr $1, 1, length($1)-2;
   }
   print "DEBUG: $incname\n" if $debug;

   return $incname if -f $incname;
   return 0 if ( $incname =~ /^\// || $incname =~ /^\\/ );

   foreach $dir ( @incdirs ) {
      chomp($dir);
      $incfile = "$dir/$incname";
      print "DEBUG: checking for $incname in $dir\n" if $debug;
      return $incfile if -f $incfile;
   }
   return 0;
}
