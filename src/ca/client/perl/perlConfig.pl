#!/usr/bin/env perl

# This script is used to extract information about the Perl build
# configuration, so the EPICS build system uses the same settings.

use strict;
use Config;

my $arg = shift;
my $val = $Config{$arg};

$val =~ s{\\}{/}go
    if $^O eq 'MSWin32';

print $val;
