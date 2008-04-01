package EPICS::Getopts;
require 5.000;
require Exporter;

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
