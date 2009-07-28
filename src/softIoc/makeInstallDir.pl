eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # registerRecordDeviceDriver 
#*************************************************************************
# Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

use strict;

use FindBin qw($Bin);
use lib "$Bin/../../lib/perl";

use EPICS::Path;

die "Path to INSTALL_LOCATION missing\n" unless @ARGV == 1;

my $path = AbsPath(shift);

$path =~ s/\\/\\\\/gx;

print "/* THIS IS A GENERATED FILE. DO NOT EDIT! */\n",
      "\n",
      "#ifndef INC_epicsInstallDir_H\n",
      "#define INC_epicsInstallDir_H\n",
      "\n",
      "#define EPICS_BASE \"$path\"\n",
      "\n",
      "#endif /* INC_epicsInstallDir_H */\n";

