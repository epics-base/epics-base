#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
# National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS Base is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

# Find and delete dependency files from all build dirs in the source tree.
# The extension for dependency files is assumed to be .d (currently true).

use File::Find;

@ARGV = ('.') unless @ARGV;

sub check {
    unlink if -f && m(/O\.[^/]+/[^/]+\.d$);
}

find({ wanted => \&check, no_chdir => 1 }, @ARGV);
