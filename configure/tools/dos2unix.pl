eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeConfigAppInclude.pl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# Converts text file in DOS CR/LF format to unix ISO format
#  $Id$

@files=@ARGV;

$| = 1;
foreach( @files ) {
    open(INPUT, "<$_");
    $backup = "$_.bak";
    rename( $_, $backup) || die "Unable to rename $_\n$!\n";
    open(OUTPUT, ">$_");
    while(<INPUT>) {
    s/\r\n/\n/;
    print OUTPUT;
    }
    close INPUT;
    close OUTPUT;
    unlink ($backup) or die "Cannot remove $backup";
}
