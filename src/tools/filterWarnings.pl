#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
#     Synchrotronstrahlung.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

#
#  Author: Ralph Lange
#

use Text::Wrap;

my ($errline, $errno, $codeline, $pointline);

$Text::Wrap::columns = $ENV{COLUMNS} if $ENV{COLUMNS};

sub Usage
{
    print "Usage:\n";
    print "<compile line> 2>&1 | $0\n";

    exit 2;
}

# Invalid whit space filter on solaris not implemented
#if ( $ENV{"EPICS_HOST_ARCH"} =~ m/solaris-sparc/ ) {
#  while ( $errline = <> ) {
#    if ( $errline !~ m/invalid white space character in directive/ ) {
#      print $errline;
#    }
#  }
#} else {
  while ( $errline = <> ) {
    if ( $errline =~ m/^(Warning|Error)/ ) {
      ($errno) = ($errline =~ m/.* ([0-9]+):/);
      $codeline = <>;
      $pointline = <>;
      next if $codeline =~ m|/[/*]\s*X.*aCC[^a-zA-Z]*$errno|;

      print wrap ("", "    ", $errline);
      print $codeline;
      print $pointline;
    }
  }
#}
