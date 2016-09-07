#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use Carp;

#
# Parse all relevent configure/RELEASE* files and includes
#
sub readReleaseFiles {
    my ($relfile, $Rmacros, $Rapps, $arch) = @_;

    my $hostarch = $ENV{'EPICS_HOST_ARCH'};
    $Rmacros->{'EPICS_HOST_ARCH'} = $hostarch if $hostarch;

    return unless (-e $relfile);
    readRelease($relfile, $Rmacros, $Rapps);

    if ($hostarch) {
        my $hrelfile = "$relfile.$hostarch";
        readRelease($hrelfile, $Rmacros, $Rapps) if (-e $hrelfile);
        $hrelfile .= '.Common';
        readRelease($hrelfile, $Rmacros, $Rapps) if (-e $hrelfile);
    }

    if ($arch) {
        my $crelfile = "$relfile.Common.$arch";
        readRelease($crelfile, $Rmacros, $Rapps) if (-e $crelfile);

        if ($hostarch) {
            my $arelfile = "$relfile.$hostarch.$arch";
            readRelease($arelfile, $Rmacros, $Rapps) if (-e $arelfile);
        }
    }
}

#
# Parse a configure/RELEASE* file and anything it includes
#
sub readRelease {
    my ($file, $Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array

    open(my $IN, '<', $file) or croak "Can't open $file: $!\n";
    while (<$IN>) {
        chomp;
        s/ \r $//x;             # Shouldn't need this, but sometimes...
        s/ # .* $//x;           # Remove trailing comments
        s/ \s+ $//x;            # Remove trailing whitespace
        next if m/^ \s* $/x;    # Skip blank lines

        # Handle "<macro> = <path>" plus the := and ?= variants
        my ($macro, $op, $val) = m/^ \s* (\w+) \s* ([?:]?=) \s* (.*) /x;
        if ($macro ne '') {
            $macro = 'TOP' if $macro =~ m/^ INSTALL_LOCATION /x;
            if (exists $Rmacros->{$macro}) {
                next if $op eq '?=';
            } else {
                push @$Rapps, $macro;
            }
            $val = expandMacros($val, $Rmacros) if $op eq ':=';
            $Rmacros->{$macro} = $val;
            next;
        }
        # Handle "include <path>" and "-include <path>" syntax
        my ($op, $path) = m/^ \s* (-? include) \s+ (.*)/x;
        $path = expandMacros($path, $Rmacros);
        if (-e $path) {
            readRelease($path, $Rmacros, $Rapps);
        } elsif ($op eq "include") {
            carp "EPICS/Release.pm: Include file '$path' not found\n";
        }
    }
    close $IN;
}

#
# Expand all (possibly nested) macros in a string
#
sub expandMacros {
    my ($str, $Rmacros) = @_;
    # $Rmacros is a reference to a hash

    while (my ($pre, $var, $post) = $str =~ m/ (.*) \$\( (\w+) \) (.*) /x) {
        last unless exists $Rmacros->{$var};
        $str = $pre . $Rmacros->{$var} . $post;
    }
    return $str;
}

#
# Expand all (possibly nested) macros in dictionary
#
sub expandRelease {
    my ($Rmacros) = @_;
    # $Rmacros is a reference to a hash

    while (my ($macro, $val) = each %$Rmacros) {
        while (my ($pre,$var,$post) = $val =~ m/ (.*) \$\( (\w+) \) (.*) /x) {
            carp "EPICS/Release.pm: Undefined macro \$($var) used\n"
                unless exists $Rmacros->{$var};
            croak "EPICS/Release.pm: Circular definition of macro $macro\n"
                if $macro eq $var;
            $val = $pre . $Rmacros->{$var} . $post;
            $Rmacros->{$macro} = $val;
        }
    }
}

1;
