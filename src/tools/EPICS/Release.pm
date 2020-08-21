#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# Regex to recognize variable names allowed in a RELEASE file
my $MVAR = qr/[A-Za-z_] [A-Za-z_0-9-]*/x;

#
# Parse all relevent configure/RELEASE* files and includes
#
sub readReleaseFiles {
    my ($relfile, $Rmacros, $Rapps, $arch) = @_;

    my $hostarch = $ENV{'EPICS_HOST_ARCH'};
    $Rmacros->{'EPICS_HOST_ARCH'} = $hostarch if $hostarch;

    return unless (-e $relfile);

    my %done;
    &readRelease($relfile, $Rmacros, $Rapps, \%done);

    if ($hostarch) {
        my $hrelfile = "$relfile.$hostarch";
        &readRelease($hrelfile, $Rmacros, $Rapps, \%done) if (-e $hrelfile);
        $hrelfile .= '.Common';
        &readRelease($hrelfile, $Rmacros, $Rapps, \%done) if (-e $hrelfile);
    }

    if ($arch) {
        my $crelfile = "$relfile.Common.$arch";
        &readRelease($crelfile, $Rmacros, $Rapps, \%done) if (-e $crelfile);

        if ($hostarch) {
            my $arelfile = "$relfile.$hostarch.$arch";
            &readRelease($arelfile, $Rmacros, $Rapps, \%done) if (-e $arelfile);
        }
    }
}

#
# Parse a configure/RELEASE* file and anything it includes
#
sub readRelease {
    my ($file, $Rmacros, $Rapps, $Ractive) = @_;
    # $Rmacros and $Ractive are hash-refs, $Rapps an array-ref

    if ($Ractive->{$file} > 0) {
        die "Release.pm: Recursive loop found in RELEASE files,\n" .
            "discovered in $file\n";
    }

    open(my $IN, '<', $file) or die "Can't open $file: $!\n";
    $Ractive->{$file}++;
    while (<$IN>) {
        chomp;
        s/ \r $//x;             # Shouldn't need this, but sometimes...
        s/^ \s+ //x;            # Remove leading whitespace
        next if m/^ $/x;        # Skip blank lines
        s/ # .* $//x;           # Remove trailing comments
        s/ \s+ $//x;            # Remove trailing whitespace

        # Handle "undefine <variable>"
        my ($uvar) = m/^ undefine \s+ ($MVAR)/x;
        if ($uvar ne '') {
            delete $Rmacros->{$uvar};
            next;
        }

        # Handle "include <path>" and "-include <path>" syntax
        my ($op, $path) = m/^ (-? include) \s+ (.*)/x;
        if ($op ne '') {
            $path = expandMacros($path, $Rmacros);
            if (-e $path) {
                &readRelease($path, $Rmacros, $Rapps, $Ractive);
            } elsif ($op eq "include") {
                warn "EPICS/Release.pm: Include file '$path' not found\n";
            }
            next;
        }

        # Handle "<variable> = <path>" plus the := and ?= variants
        my ($var, $op, $val) = m/^ ($MVAR) \s* ([?:]?=) \s* (.*) /x;
        if ($var ne '') {
            $var = 'TOP' if $var =~ m/^ INSTALL_LOCATION /x;
            if (exists $Rmacros->{$var}) {
                next if $op eq '?=';
            } else {
                push @$Rapps, $var;
            }
            $val = expandMacros($val, $Rmacros) if $op eq ':=';
            $Rmacros->{$var} = $val;
            next;
        }
    }
    $Ractive->{$file}--;
    close $IN;
}

#
# Expand all (possibly nested) variables in a string
#
sub expandMacros {
    my ($str, $Rmacros) = @_;
    # $Rmacros is a reference to a hash

    while (my ($pre, $var, $post) = $str =~ m/ (.*) \$\( ($MVAR) \) (.*) /x) {
        last unless exists $Rmacros->{$var};
        $str = $pre . $Rmacros->{$var} . $post;
    }
    return $str;
}

#
# Expand all (possibly nested) variables in a dictionary
#
sub expandRelease {
    my ($Rmacros, $warn) = @_;
    # $Rmacros is a reference to a hash
    $warn = '' unless defined $warn;

    while (my ($relvar, $val) = each %$Rmacros) {
        while (my ($pre,$var,$post) = $val =~ m/ (.*) \$\( ($MVAR) \) (.*) /x) {
            warn "EPICS/Release.pm: Undefined variable \$($var) used $warn\n"
                unless exists $Rmacros->{$var};
            die "EPICS/Release.pm: Circular definition of variable $var $warn\n"
                if $relvar eq $var;
            $val = $pre . $Rmacros->{$var} . $post;
            $Rmacros->{$relvar} = $val;
        }
    }
}

1;
