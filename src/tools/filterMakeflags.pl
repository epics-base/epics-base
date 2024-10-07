#!/bin/env perl
#
# Filter all versions of GNU Make's MAKEFLAGS variable to return
# only the single-letter flags. The content differed slightly
# between 3.81, 3.82 and 4.0; Apple still ship 3.81.

use strict;

my @flags;
foreach (@ARGV) {
    last if m/^--$/ or m/\w+=/;
    next if m/^--/ or m/^-$/;
    push @flags, $_;
};
print join(' ', @flags);
