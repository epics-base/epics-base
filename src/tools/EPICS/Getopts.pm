package EPICS::Getopts;
require 5.000;
require Exporter;

=head1 NAME

EPICS::Getopts - Process single-character command-line options

=head1 SYNOPSIS

    use EPICS::Getopts;

    getopts('vo:I@') or die "Bad option\n";
        # -v is a counted flag; -o takes an argument and
        # sets $opt_o to its value; -I may be used more than
        # once, the values are pushed onto @opt_I

=head1 DESCRIPTION

This version of getopts has been modified from the Perl original to add
functionality that was needed by EPICS Perl applications. Some functionality of
the original has also been removed. The main changes were:

=over 2

=item *

Arguments without any modifiers are now counted, rather than storing a binary
value in the C<$opt_x> variable. This means that multiple copies of the same
option can be detected by the program.

=item *

A new B<C<@>> modifier is supported which collects the option arguments in an
array C<@opt_x>. Multiple copies of this option can be given, which pushes each
argument value onto the end of the array.

=back


=head1 FUNCTIONS

=over 4

=item B<C<getopts($argspec)>>

The getopts() function processes single-character options. It takes one
argument, a string containing all the options that should be recognized. For
option letters in the string that are followed by a colon B<C<:>> it sets
C<$opt_x> (where x is the option letter) to the value of that argument. For
option letters followed by an at sign B<C<@>> it pushes each subsequent argument
onto the array C<@opt_x> (where x is the option letter). For option letters
without any qualifier it adds 1 to the value of C<$opt_x>. Options which take an
argument don't care whether there is a space between the option and the
argument.

If unspecified switches are found on the command-line, the user will be warned
that an unknown option was given. The getopts() function returns true unless an
invalid option was found.

Note that, if your code is running under the recommended C<use strict 'vars'>
pragma, you will need to declare the necesary package variables with "our"
before the call to getopts:

    our($opt_v, $opt_o, @opt_I);

To allow programs to process arguments that look like options but aren't, the
function will stop processing options when it sees the argument B<C<-->>. The
B<C<-->> will be removed from @ARGV.

=back

=cut

@ISA = qw(Exporter);
@EXPORT = qw(getopts);

# EPICS::Getopts.pm - An EPICS-specific version of getopts
#
# This version of getopts is modified from the Perl original in the
# following ways:
#
# 1. The perl routine in GetOpt::Std allows a perl hash to be passed
#    in as a third argument and used for storing option values. This
#    functionality has been deleted.
# 2. Arguments without a ':' modifier are now counted, rather than
#    being binary. This means that multiple copies of the same option
#    can be detected by the program.
# 3. A new '@' modifier is provided which collects the option arguments
#    in an array @opt_X. Multiple copies of this option are permitted,
#    which push the argument values onto the end of the list.

sub getopts ( $;$ ) {
    my ($argumentative) = @_;
    my (@args,$first,$rest);
    my $errs = 0;
    local $_;
    local @EXPORT;

    @args = split( / */, $argumentative );
    while(@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (/^--$/) {   # early exit if --
	    shift @ARGV;
	    last;
	}
	$pos = index($argumentative,$first);
	if ($pos >= 0) {
	    if (defined($args[$pos+1]) and (index(':@', $args[$pos+1]) >= 0)) {
		shift(@ARGV);
		if ($rest eq '') {
		    ++$errs unless @ARGV;
		    $rest = shift(@ARGV);
		}
		if ($args[$pos+1] eq ':') {
		    ${"opt_$first"} = $rest;
		    push @EXPORT, "\$opt_$first";
		} elsif ($args[$pos+1] eq '@') {
		    push @{"opt_$first"}, $rest;
		    push @EXPORT, "\@opt_$first";
		}
	    } else {
		${"opt_$first"} += 1;
		push @EXPORT, "\$opt_$first";
		if ($rest eq '') {
		    shift(@ARGV);
		} else {
		    $ARGV[0] = "-$rest";
		}
	    }
	} else {
	    warn "Unknown option: $first\n";
	    ++$errs;
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    } else {
		shift(@ARGV);
	    }
	}
    }
    local $Exporter::ExportLevel = 1;
    import EPICS::Getopts;
    $errs == 0;
}

1;
