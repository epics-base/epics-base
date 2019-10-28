#!/usr/bin/env perl

# Some Windows Perl installations provide a prove.bat file which
# doesn't work properly.

use App::Prove;

my $app = App::Prove->new;
$app->process_args(@ARGV);
exit( $app->run ? 0 : 1 );
