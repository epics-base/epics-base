#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;

die "$0: Argument missing, INSTALL_LOCATION\n" if @ARGV == 0;
die "$0: Too many arguments, expecting one\n" unless @ARGV == 1;

my $path = shift;

$path =~ s/\\/\\\\/gx;
$path =~ s/^'//;
$path =~ s/'$//;

print "/* THIS IS A GENERATED FILE. DO NOT EDIT! */\n",
      "\n",
      "#ifndef INC_epicsInstallDir_H\n",
      "#define INC_epicsInstallDir_H\n",
      "\n",
      "#define EPICS_BASE \"$path\"\n",
      "\n",
      "#endif /* INC_epicsInstallDir_H */\n";
