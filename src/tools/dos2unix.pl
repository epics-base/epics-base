#!/usr/bin/env perl
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

@files=@ARGV;

$| = 1;
foreach( @files ) {
    open(INPUT, "<$_");
    $backup = "$_.bak";
    rename( $_, $backup) || die "Unable to rename $_\n$!\n";
    # Make the output be binary so it won't convert /n back to /r/n
    binmode OUTPUT, ":raw";
    open(OUTPUT, ">$_");
    binmode OUTPUT, ":raw";
    while(<INPUT>) {
	# Remove CR-LF sequences
	s/\r\n/\n/;
	print OUTPUT;
    }
    close INPUT;
    close OUTPUT;
    unlink ($backup) or die "Cannot remove $backup";
}
