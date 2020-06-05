#!/usr/bin/env perl
#*************************************************************************
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Portable test-runner to make the output stand out a bit more.

use strict;
use warnings;

use App::Prove;
use Cwd 'abs_path';

my $path = abs_path('.');

printf "\n%s\n%s\n", '-' x length($path), $path;

my $prover = App::Prove->new;
$prover->process_args(@ARGV);
my $res = $prover->run;

print "-------------------\n\n";

exit( $res ? 0 : 1 );
