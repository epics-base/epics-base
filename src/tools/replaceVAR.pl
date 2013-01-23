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

# Called from within the object directory
# Replaces VAR(xxx) with $(xxx)
# and      VAR_xxx_ with $(xxx)

while (<STDIN>) {
    s/VAR\(/\$\(/g;
    s/VAR_([^_]*)_/\$\($1\)/g;
    print;
}
