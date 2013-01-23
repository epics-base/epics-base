#!/usr/bin/env perl

# This script is used to extract information about the Perl build
# configuration, so the EPICS build system uses the same settings.

use Config;

my $arg = shift;
print $Config{$arg};
