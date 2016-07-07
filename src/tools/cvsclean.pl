#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
# National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
# Operator of Los Alamos National Laboratory.
# This file is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************

# Find and delete cvs .#* and editor backup *~
# files from all dirs of the directory tree.

use File::Find;

@ARGV = ('.') unless @ARGV;

find sub { unlink if -f && m/(^\.\#)|(~$)/ }, @ARGV;
