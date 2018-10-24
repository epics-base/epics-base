#!/usr/bin/perl
=head1 NAME

tap-to-junit-xml - convert perl-style TAP test output to JUnit-style XML

=head1 SYNOPSIS

tap-to-junit-xml [--help|--man]
                [--[no]hidesummary]
                [--input <tap input file>]
                [--output <junit output file>]
                [--puretap]
                [<test suite name>] [outputprefix]

=head1 DESCRIPTION

Parse test suite output in TAP (Test Anything Protocol,
C<http://testanything.org/>) format, and produce XML output in a similar format
to that produced by the <junit> ant task.  This is useful for consumption by
continuous-integration systems like Hudson (C<https://hudson.dev.java.net/>).

C<"test suite name"> is a descriptive string used as the B<name> attribute on the
top-level <testsuites> node of the output XML. Defaults to "make test".

If C<outputprefix> is specified, multi-file output will be generated, with
multiple XML files created using C<outputprefix> as the start of their
filenames. The files are separated by testplan.  This option is ignored
if --puretap is specified (TAP only allows one testplan per input file).
This prefix may contain slashes, in which case the files will be
placed into a directory hierarchy accordingly (although care should be taken to
ensure these directories exist in advance).

If --input I<file name> is not specified, STDIN will be read.
If C<outputprefix> or --output is not specified, a single XML file will be
generated on STDOUT.

--output I<file name> is used to write a single XML file to I<file name>.

--puretap parses a single TAP source and handles parse errors and directives
(todo, skip, bailout).  --puretap ignores unknown (non-TAP) input. Without
--puretap, the script will parse some additional non-TAP test input, such as
Perl tests that can include a "Test Summary Report", but it won't generate
correct XML unless the TAP testplan comes before the test cases.
--hidesummary report (the default) will hide the summary report, --no-hidesummary
will display it (neither has an effect when --puretap is specified).

=head1 EXAMPLE

    prove -v 2>&1 | tee tests.log
    tap-to-junit-xml "make test" testxml/tests < tests.log

(JUnit-formatted XML is now in "testxml/tests*.xml".)

=head1 DEPENDENCIES

 Getopt::Long
 Pod::Usage
 TAP::Parser
 Time::HiRes
 XML::Generator 

=head1 BUGS

 - Output is optimized for Hudson, and may not look quite as good in
   other UIs.
  - Doesn't do anything with the STDERR from tests.
  - Doesn't fill in the 'errors' attribute in the  <testsuite> element.
   (--puretap handles parse errors)
 - Doesn't handle "todo" or "skip" (--puretap does)
  - Doesn't get the elapsed time for each 'test' (i.e. assertion.)
   (TAP output has no elapsed time convention).

=head1 SOURCE

http://github.com/jmason/tap-to-junit-xml/tree/master

=head1 AUTHOR

original, junit_xml.pl, by Matisse Enzer <matisse at matisse.net>; see
C<http://twoalpha.blogspot.com/2007/01/junit-style-xml-from-perl-test-files.html>.

pretty much entirely rewritten by Justin Mason <junit at jmason.org>, Feb 2008.

Miscellaneous fixes and mods (--puretap) by Jascha Lee <jascha at yahoo-inc.com>, Mar 2009.

=head1 VERSION

 Mar 27 2008 jm
 Mar 17 2009 jl

=head1 COPYRIGHT & LICENSE

Copyright (c) 2007 Matisse Enzer. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.
=cut

use strict;
use warnings;

use Getopt::Long qw(:config no_ignore_case);
use Pod::Usage;
use TAP::Parser;
use Time::HiRes qw(gettimeofday tv_interval);
use XML::Generator qw(:noimport);

my %opts;
pod2usage() unless GetOptions( \%opts, 'help|h',
                                      'hidesummary!',
                                      'input=s',
                                      'man',
                                      'output=s',
                                      'puretap'
                            );

pod2usage(-verbose => 1) if defined $opts{'help'};
pod2usage(-verbose => 2) if defined $opts{'man'};

my $opt_suitename = shift @ARGV;
my $opt_multifile = 0;
my $opt_mfprefix;

if (defined $ARGV[0]) {
  $opt_multifile = 1;
  $opt_mfprefix = $ARGV[0];
}

# should the 'Test Summary Report' at the end of a test suite be displayed
# as if it was a testcase?  in my opinion, no
my $HIDE_TEST_SUMMARY_REPORT = defined $opts{'hidesummary'} ? $opts{'hidesummary'} : 1;

my $suite_name = $opt_suitename || 'make test';
my $safe_suite_name = $suite_name; $safe_suite_name =~ s/[^-:_A-Za-z0-9]+/_/gs;

# TODO: it'd be nice to respect 'Universal desirable behavior #1' from
# http://testanything.org/wiki/index.php/TAP_Consumers -- 'Should work on the
# TAP as a stream (ie. as each line is received) rather than wait until all the
# TAP is received'.   But it seems TAP::Parser itself doesn't support it!
# maybe when TAP::Parser does that, we'll do it too.
my $tapfh;
if ( defined $opts{'input'} ) {
   open $tapfh, '<', $opts{'input'} or die "Can't open TAP file '$opts{'input'}': $!\n";
}
else {
   $tapfh = \*STDIN;
}

my $outfh;
if ( defined $opts{'output'} ) {
   open $outfh, '>', $opts{'output'} or die "Can't open output file '$opts{'output'}' for writing: $!\n";
}
else {
   $outfh = \*STDOUT;
}

my $tap             = TAP::Parser->new( { source => $tapfh } );
my $xmlgen          = XML::Generator->new( ':pretty');
my $xmlgenunescaped = XML::Generator->new( escape      => 'unescaped',
                                          conformance => 'strict',
                                          pretty      => 2
                                        );
my @properties = _get_properties($xmlgen);
if ( defined $opts{'puretap'} ) {
   #
   # Instead of trying to parse everything in one pass, which fails if the
   # testplan is last, parse through the results for the test cases and
   # then construct the <testsuite> information from the TAP and wrap it
   # around the test cases. Ignore 'unknown' information.  [JL]
   #
   my @testcases = _parse_testcases( $tap, $xmlgen );
   errorOut( $tap, $xmlgen ) if $tap->parse_errors;
   print $outfh $xmlgen->testsuites(
                   $xmlgen->testsuite( { name     => $safe_suite_name,
                                         tests    => $tap->tests_planned,
                                         failures => scalar $tap->failed,
                                         errors   => 0,
                                         time     => 0,
                                         id       => 1 },
                                         @testcases ));

}
else {
   my $test_results    = _parse_tests( $tap, $xmlgen );
   if ($opt_multifile) {
  _gen_junit_multifile_xml( $xmlgen, \@properties, $test_results );
   } else {
       print $outfh _get_junit_xml( $xmlgen, \@properties, $test_results );
   }
}
exit;

#-------------------------------------------------------------------------------

sub _get_junit_xml {
  my ( $xmlgen, $properties, $test_results ) = @_;
  my $xml = "<?xml version='1.0' encoding='UTF-8' ?>\n" . 
          $xmlgen->testsuites({
              name => $suite_name,
            }, @$test_results);
  return $xml;
}

sub _gen_junit_multifile_xml {
  my ( $xmlgen, $properties, $test_results ) = @_;
  my $count = 1;
  foreach my $testsuite (@$test_results) {
    open OUT, ">${opt_mfprefix}.${count}.xml"
         or die "cannot write ${opt_mfprefix}.${count}.xml";
    print OUT "<?xml version='1.0' encoding='UTF-8' ?>\n";
    print OUT $testsuite;
    close OUT;
    $count++;
  }
}

#
# Wrap up parse errors and output them as test cases.
#
sub errorOut {
   my $parser = shift;
   my $xmlgen = shift;
   die "errorOut() needs some args"  unless $parser and $xmlgen;
   my ($xml, @errors, $name);
   my $count = 1;
   foreach my $error ( $parser->parse_errors ) {
       $name = sprintf "%s%02d", 'Error_', $count++;
       $xml  = $xmlgen->testcase( { name      => $name,
                                    classname => 'TestsNotRun.ParseError',
                                    time      => 0 },

                   $xmlgen->error( { type    => 'TAPParseError',
                                     message => $error } ));
       push @errors, $xml;
   }
   print $outfh $xmlgen->testsuites(
                   $xmlgen->testsuite( { name     => 'TestsNotRun.ParseError',
                                         tests    => $tap->tests_planned,
                                         failures => 0,
                                         errors   => scalar $tap->parse_errors,
                                         time     => 0,
                                         id       => 1 },
                                         @errors ));
   exit 86;
}

#
# Construct an array of XML'd test cases
#
sub _parse_testcases {
   my $parser = shift;
   my $xmlgen = shift;
   return () unless $parser and $xmlgen;
   my ($name, $directive, $xml, @testcases);

   while ( my $result = $parser->next ) {
       if ( $result->is_bailout ) {
           $xml  = $xmlgen->testcase( { name      => 'BailOut',
                                        classname => "$safe_suite_name.Tests",
                                        time      => 0 },

                       $xmlgen->error( { type    => 'BailOut',
                                         message => $result->explanation } ));

           push @testcases, $xml;
           last;
       }
       next unless $result->is_test;
       $directive = $result->directive;
       $name = sprintf "%s%02d", 'Test_', $result->number;
       $name .= "_$directive" if $directive;
       if ( $result->is_ok ) {
           $xml = $xmlgen->testcase( { name      => $name,
                                       classname => "$safe_suite_name.Tests",
                                       time      => 0 } );
           push @testcases, $xml;
       }
       else {
           $xml = $xmlgen->testcase( { name      => $name,
                                       classname => "$safe_suite_name.Tests",
                                       time      => 0 },
                      $xmlgen->failure( { type    => 'TAPTestFailed',
                                          message => $result->as_string } ));
           push @testcases, $xml;
       }
   }

   return @testcases;
}

sub _parse_tests {
  my ( $parser, $xmlgen ) = @_;

  my $ctx = {
    testsuites => [ ],
    test_name => 'notest',
    plan_ntests => 0,
    case_id => 0,
  };

  _new_ctx($ctx);

  my $lastunk = '';

  # unknown t/basic_lint.........
  # plan 1..1
  # comment # Running under perl version 5.008008 for linux
  # comment # Current time local: Thu Jan 24 17:44:30 2008
  # comment # Current time GMT:   Thu Jan 24 17:44:30 2008
  # comment # Using Test.pm version 1.25
  # unknown     /usr/bin/perl -T -w ../spamassassin.raw -C log/test_rules_copy  --siteconfigpath log/localrules.tmp -p log/test_default.cf  -L --lint
  # unknown     Checking anything
  # test ok 1
  # test ok 2
  # unknown t/basic_meta.........
  # plan 1..2
  # comment # Running under perl version 5.008008 for linux
  # comment # Current time local: Thu Jan 24 17:44:31 2008
  # comment # Current time GMT:   Thu Jan 24 17:44:31 2008
  # comment # Using Test.pm version 1.25
  # test not ok 1
  # comment # Failed test 1 in t/basic_meta.t at line 91
  # test ok 2
  # unknown  Failed 1/2 subtests
  # unknown t/basic_obj_api......
  # plan 1..4
  # comment # Running under perl version 5.008008 for linux
  # comment # Current time local: Thu Jan 24 17:44:33 2008
  # comment # Current time GMT:   Thu Jan 24 17:44:33 2008
  # comment # Using Test.pm version 1.25
  # test ok 1
  # test ok 2
  # test ok 3
  # test ok 4
  # test ok 9
  # unknown
  # unknown Test Summary Report
  # unknown -------------------
  # unknown t/basic_meta.t   (Wstat: 0 Tests: 2 Failed: 1)
  # unknown   Failed test:  1
  # unknown Files=3, Tests=7,  6 wallclock secs ( 0.01 usr  0.00 sys +  4.39 cusr  0.23 csys =  4.63 CPU)
  # unknown Result: FAIL
  # unknown Failed 1/3 test programs. 1/7 subtests failed.
  # unknown make: *** [test_dynamic] Error 255

  while ( my $r = $parser->next ) {
    my $t = $r->type;
    my $s = $r->as_string; $s =~ s/\s+$//;

    # warn "JMD $t $s";

    if ($t eq 'unknown') {
      $lastunk = $s;

      # PERL_DL_NONLAZY=1 /usr/bin/perl "-MExtUtils::Command::MM" "-e" "test_harness(1, 'blib/lib', 'blib/arch')" t/basic_*
      # if ($s =~ /test_harness\(.*?\)" (.+)$/) {
      # $suite_name = $1;
      # }
      if ($s =~ /^Test Summary Report$/) {
        # create a <testsuite> block for the summary
        $ctx->{plan_ntests} = 0;
        $ctx->{test_name} = "Test Summary Report";
        $ctx->{case_tests} = 1;
        _finish_test_block($ctx);
      }
      elsif ($s =~ /^Result: FAIL$/) {
        $ctx->{case_tests}++;
        $ctx->{case_failures}++;
        my $test_case = {
            classname => test_name_to_classname($ctx->{test_name}),
            name      => 'result',
            'time'    => 0,
        };
        my $failure = $xmlgen->failure({
          type => "OverallTestsFailed",
          message => $s
        }, "__FAILUREMESSAGETODO__");

        if (!$HIDE_TEST_SUMMARY_REPORT) {
          push @{$ctx->{test_cases}}, $xmlgen->testcase($test_case, $failure);
        }
      }
      elsif ($s =~ /^(\S+?)\.\.\.+1\.\.(\d+?)\s*$/) {
        # perl 5.6.x "Test" format plan line
        # unknown t/basic_lint....................1..1

        my ($name, $nt) = ($1,$2);
        if ($ctx->{plan_ntests}) {       # only if there have been tests planned
          _finish_test_block($ctx);
        }

        $ctx->{plan_ntests} = $nt+0;
        $ctx->{test_name} = "$name.t";
      }
    }
    elsif ($t eq 'plan') {
      if ($ctx->{plan_ntests}) {       # only if there have been tests planned
        _finish_test_block($ctx);
      }

      $ctx->{plan_ntests} = 0;
      $s =~ /(\d+)$/ and $ctx->{plan_ntests} = $1+0;

      $ctx->{test_name} = $lastunk;
      $ctx->{test_name} =~ s/\.*\s*$//gs;
      $ctx->{test_name} .= ".t";
    }
    elsif ($t eq 'test') {
      my $ntest = 0;
      if ($s =~ /(?:not |)\S+ (\d+)/) { $ntest = $1+0; }

      if ($ntest > $ctx->{plan_ntests}) {
        # jump in test numbers, more than planned; this is probably TAP::Parser's wierdness.
        # (when it sees the "ok" line at the end of a test case with no number,
        # it outputs the current total number of tests so far.)
        next;
      }

      # clean this up in a Hudson-compatible way; ":" and "/" are out, "." also causes
      # trouble by creating an extra "directory" in the results

      my $test_case = {
          classname => test_name_to_classname($ctx->{test_name}),
          name      => sprintf("test %6d", $ntest), # space-padding ensures ordering
          'time'    => 0,
      };

      $ctx->{case_tests}++;
      my $failure = undef;
      if ($s =~ /^not /i) {
        $ctx->{case_failures}++;
        $failure = $xmlgen->failure({
          type => "TAPTestFailed",
          message => $s
        }, "__FAILUREMESSAGETODO__");
        push @{$ctx->{test_cases}}, $xmlgen->testcase($test_case, $failure);
      }
      else {
        push @{$ctx->{test_cases}}, $xmlgen->testcase($test_case);
      }
    }
      
    $ctx->{sysout} .= $s."\n";
  }

  if (scalar(@{$ctx->{test_cases}}) == 0 &&
      scalar(@{$ctx->{testsuites}}) == 0)
  { 
    # no tests found! create a <testsuite> block containing *something* at least
    $ctx->{case_tests}++;
    my $test_case = {
        classname => test_name_to_classname($ctx->{test_name}),
        name      => 'result',
        'time'    => 0,
    };
    push @{$ctx->{test_cases}}, $xmlgen->testcase($test_case);
  }

  _finish_test_block($ctx);
  return $ctx->{testsuites};
}

sub _new_ctx {
  my $ctx = shift;
  $ctx->{start_time} = [gettimeofday];
  $ctx->{test_cases} = [];
  $ctx->{case_tests} = 0;
  $ctx->{case_failures} = 0;
  $ctx->{case_time} = 0;
  $ctx->{case_id}++;
  $ctx->{sysout} = '';
  return $ctx;
}

sub _finish_test_block {
  my $ctx = shift;
  $ctx->{sysout} =~ s/\n\S+\.*\s*\n$/\n/s;       # remove next test's "t/foo....." line

  my $elapsed_time = 0;     # TODO
  #my $elapsed_time = tv_interval( $ctx->{start_time}, [gettimeofday] );

  # clean it up to valid Java packagename format (or at least something Hudson will
  # consume)
  my $name = $ctx->{test_name};
  $name =~ s/[^-:_A-Za-z0-9]+/_/gs;
  $name = "$safe_suite_name.$name";      # a "directory" for the suite name

  my $testsuite = {
      'time'         => $elapsed_time,
      'name'         => $name,
      tests          => $ctx->{case_tests},
      failures       => $ctx->{case_failures},
      'id'           => $ctx->{case_id},
      errors         => 0,
  };

  my @fixedcases = ();
  foreach my $tc (@{$ctx->{test_cases}}) {
    if ($tc =~ s/__FAILUREMESSAGETODO__/ cdata($ctx->{sysout}) /ges) {
      push @fixedcases, \$tc;       # inhibits escaping!
    } else {
      push @fixedcases, $tc;
    }
  }

  # use "unescaped"; we have already fixed escaping on these strings.
  # note that a reference means 'this is unescaped', bizarrely.
  push @{$ctx->{testsuites}}, $xmlgenunescaped->testsuite($testsuite,
          @fixedcases,
          \("<system-out>\n".cdata($ctx->{sysout})."\n</system-out>"),
          \("<system-err />"));

  _new_ctx($ctx);
};

sub cdata {
  my $s = shift;
  $s =~ s/\]\]>/\](warning: defanged by tap-to-junit-xml)\]>/gs;
  return '<![CDATA['.$s.']]>';
}

sub _get_properties {
    my $xmlgen = shift;
    my @props;
    foreach my $key ( sort keys %ENV ) {
        push @props, $xmlgen->property( { name => "$key", value => $ENV{$key} } );
    }
    return @props;
}

sub test_name_to_classname {
  my $safe = shift;
  $safe =~ s/[^-:_A-Za-z0-9]+/_/gs;
  $safe = "$safe_suite_name.$safe";      # a "directory" for the suite name
  $safe;
}

__END__

# JUnit references:
# http://www.nabble.com/JUnit-4-XML-schematized--td13946472.html
# http://jra1mw.cvs.cern.ch:8180/cgi-bin/jra1mw.cgi/org.glite.testing.unit/config/JUnitXSchema.xsd?view=markup
# skipped tests:
# https://hudson.dev.java.net/issues/show_bug.cgi?id=1251
# Hudson source:
# http://fisheye5.cenqua.com/browse/hudson/hudson/main/core/src/main/java/hudson/tasks/junit/CaseResult.java
