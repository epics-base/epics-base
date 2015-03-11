#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# The makeTestfile.pl script generates a file $target.t which is needed
# because some versions of the Perl test harness can only run test scripts
# that are actually written in Perl.  The script we generate runs the
# real test program which must be in the same directory as the .t file.
# If the script is given an argument -tap it sets HARNESS_ACTIVE in the
# environment to make the epicsUnitTest code generate strict TAP output.

# Usage: makeTestfile.pl target.t executable
#     target.t is the name of the Perl script to generate
#     executable is the name of the file the script runs

use strict;

my ($target, $exe) = @ARGV;

# Use system on Windows, exec doesn't work the same there and
# GNUmake thinks the test has finished as soon as Perl exits.
my $exec = $^O eq 'MSWin32' ? "system('./$exe') == 0" : "exec './$exe'";

open(my $OUT, '>', $target) or die "Can't create $target: $!\n";

print $OUT <<EOF;
#!/usr/bin/perl

use strict;
use Cwd 'abs_path';

\$ENV{HARNESS_ACTIVE} = 1 if scalar \@ARGV && shift eq '-tap';
\$ENV{TOP} = abs_path(\$ENV{TOP}) if exists \$ENV{TOP};
$exec or die "Can't run $exe: \$!\\n";
EOF

close $OUT or die "Can't close $target: $!\n";
