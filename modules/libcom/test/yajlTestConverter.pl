#!/usr/bin/perl
#
# This script converts the parsing test cases from the yajl release tree
# into the yajlTestCases module as used by libCom/test/yajlTest.plt
#
# Re-do this conversion and commit after checking out a new version of yajl
# from https://github.com/lloyd/yajl as follows:
#   $ cd <base>/src/libCom/test
#   $ perl yajlTestConverter.pl /path/to/yajl
# The tests are saved into the file yajlTestCases.pm in the src/libCom/test
# directory which will be read by the yajlTest.t test script.

use Data::Dumper;

my $yajl = shift @ARGV
    or die "Usage: $0 /path/to/yajl\n";

my @files = glob "$yajl/test/parsing/cases/*.json";

my $caseFile = 'yajlTestCases.pm';

my @cases;

for my $file (@files) {
    $file =~ m|/([afn][cgmp]_)?([^/]*)\.json$|;
    my $allow = $1;
    my $name = $2;
    next if $name eq '';

    my $case = { name => $name };

    if ($allow eq 'ac_') {
        $case->{opts} = ['-c'];
    }
    elsif ($allow eq 'ag_') {
        $case->{opts} = ['-g'];
    }
    elsif ($allow eq 'am_') {
        $case->{opts} = ['-m'];
    }
    elsif ($allow eq 'ap_') {
        $case->{opts} = ['-p'];
    }
    else {
        $case->{opts} = [];
    }

    my $input = slurp($file);
    my @input = split "\n", $input;
    push @input, '' if $input =~ m/\n$/;
    $case->{input} = \@input;

    my @gives = split "\n", slurp("$file.gold");
    $case->{gives} = \@gives;

    push @cases, $case;
}

# Configure Dumper() output
$Data::Dumper::Pad = '  ';
$Data::Dumper::Indent = 1;
$Data::Dumper::Useqq = 1;
$Data::Dumper::Quotekeys = 0;
$Data::Dumper::Sortkeys = sub { return ['name', 'opts', 'input', 'gives'] };

my $data = Dumper(\@cases);

open my $out, '>', $caseFile
    or die "Can't open/create $caseFile: $@\n";
print $out <<"EOF";
# Parser test cases from https://github.com/lloyd/yajl
#
# This file is generated, DO NOT EDIT!
#
# See comments in yajlTestConverter.pl for instructions on
# how to regenerate this file from the original yajl sources.

sub cases {
  my$data
  return \@{\$VAR1};
}

1;
EOF

close $out
    or die "Problem writing $caseFile: $@\n";

exit 0;


sub slurp {
    my ($file) = @_;
    open my $in, '<', $file
        or die "Can't open file $file: $!\n";
    my $contents = do { local $/; <$in> };
    return $contents;
}
