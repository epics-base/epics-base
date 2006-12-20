#!/usr/bin/perl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#-----------------------------------------------------------------------
# mkmf.pl: Perl script to create #include file dependancies
#
# Limitations:
#
# 1) Only handles the #include preprocessor command. Does not understand
#    the preproceeor commands #define, #if, #ifdef, #ifndef, ...
# 2) Does not know a compilers internal macro definitions e.g.
#    __cplusplus, __STDC__
# 3) Does not keep track of the macros defined in #include files so can't
#    do #ifdefs #ifndef ...
# 4) Does not know where system include files are located
# 5) Uses only #include lines with single or double quoted file names
#
#-----------------------------------------------------------------------

use Getopt::Std;

my $version = 'mkmf.pl,v 1.5 2002/03/25 21:33:24 jba Exp $ ';
my $endline = $/;
my %output;
my @includes;

use vars qw( $opt_d $opt_m );
getopts( 'dm:' ) || die "\aSyntax: $0 [-d] [-f dependsFile] includeDirs objFile srcFile\n";
my $debug = $opt_d;
my $depFile = $opt_m;

print "$0 $version\n" if $debug;

# directory list 
my @dirs;
my $i;
foreach $i (0 .. $#ARGV-2) {
    push @dirs, $ARGV[$i];
}

my $objFile = $ARGV[$#ARGV-1];
my $srcFile = $ARGV[$#ARGV];

if( $debug ) {
   print "DEBUG: dirs= @dirs\n";
   print "DEBUG: source= $srcFile\n";
   print "DEBUG: object= $objFile\n";
}

print "Generating dependencies for $objFile\n" if $debug;
scanFile($srcFile);
scanIncludesList();

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

   print "# DO NOT EDIT: This file created by $version\n\n";

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
      $incfile = findNextIncName($line);
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
# find filename on #include line
sub findNextIncName {
   my $line = shift;
   my $incname = "";
   my $incfile = 0;
   my $dir;

   local $/ = $endline;
    return 0 if not $line =~ /^\s*#?\s*include\s*(<.*?>|".*?")/;
    $incname = substr $1, 1, length($1)-2;
    print "DEBUG: $incname\n" if $debug;


   return $incname if -f $incname;
   return 0 if ( $incname =~ /^\// || $incname =~ /^\\/ );

   foreach $dir ( @dirs ) {
      chomp($dir);
      $incfile = "$dir/$incname";
      print "DEBUG: checking for $incname in $dir\n" if $debug;
      return $incfile if -f $incfile;
   }
   return 0;
}
