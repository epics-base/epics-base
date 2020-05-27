#!/usr/bin/env perl

# Some Windows Perl installations provide a prove.bat file which
# doesn't work properly. This also lets us make the output stand
# out a bit more.

use strict;
use warnings;

use App::Prove;
use Cwd 'abs_path';

my $path = abs_path('.');

printf "\n%s\n%s\n", '-' x length($path), $path;

my $app = App::Prove->new;
$app->process_args(@ARGV);
my $res = $app->run;

print "-------------------\n\n";

exit( $res ? 0 : 1 );
