eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeDbDepends.pl

# Called from within RULES.Db in the Db directories.
# Searches .substitutions and .template files (from the command line) for
#   file ["']xxx["'] {
# and
#   include "xxx"
# entries to include in the DEPENDS file

use strict;

my $target = shift @ARGV;
my %depends;

while (my $line = <>) {
    $depends{$2}++ if $line =~ m/^\s*file\s*(["']?)(.*)\1/;
    $depends{$1}++ if $line =~ m/^\s*include\s+"(.*)"/;
}

if (%depends) {
    my @depends = keys %depends;
    print "$target: @depends\n";
}
