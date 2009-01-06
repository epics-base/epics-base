#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# $Id$

#
# Parse all relevent configure/RELEASE* files and includes
#
sub readReleaseFiles {
    my ($relfile, $Rmacros, $Rapps, $arch) = @_;

    return unless (-r $relfile);
    &readRelease($relfile, $Rmacros, $Rapps);

    my $hostarch = $ENV{'EPICS_HOST_ARCH'};
    if ($hostarch) {
        my $hrelfile = "$relfile.$hostarch";
        &readRelease($hrelfile, $Rmacros, $Rapps) if (-r $hrelfile);
        $hrelfile .= '.Common';
        &readRelease($hrelfile, $Rmacros, $Rapps) if (-r $hrelfile);
    }

    if ($arch) {
        my $crelfile = "$relfile.Common.$arch";
        &readRelease($crelfile, $Rmacros, $Rapps) if (-r $crelfile);

        if ($hostarch) {
            my $arelfile = "$relfile.$hostarch.$arch";
            &readRelease($arelfile, $Rmacros, $Rapps) if (-r $arelfile);
        }
    }
}

#
# Parse a configure/RELEASE* file and anything it includes
#
sub readRelease {
    my ($file, $Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array

    open(my $IN, '<', $file) or die "Can't open $file: $!\n";
    while (<$IN>) {
        chomp;
        s/ \r $//x;             # Shouldn't need this, but sometimes...
        s/ # .* $//x;           # Remove trailing comments
        s/ \s+ $//x;            # Remove trailing whitespace
        next if m/^ \s* $/x;    # Skip blank lines

        # Expand all already-defined macros in the line:
        while (my ($pre,$var,$post) = m/ (.*) \$\( (\w+) \) (.*) /x) {
            last unless exists $Rmacros->{$var};
            $_ = $pre . $Rmacros->{$var} . $post;
        }

        # Handle "<macro> = <path>"
        my ($macro, $path) = m/^ \s* (\w+) \s* = \s* (.*) /x;
        if ($macro ne '') {
            $macro='TOP' if $macro =~ m/^ INSTALL_LOCATION /x;
            if (exists $Rmacros->{$macro}) {
                delete $Rmacros->{$macro};
            } else {
                push @$Rapps, $macro;
            }
            $Rmacros->{$macro} = $path;
            next;
        }
        # Handle "include <path>" and "-include <path>" syntax
        ($path) = m/^ \s* -? include \s+ (.*)/x;
        &readRelease($path, $Rmacros, $Rapps) if (-r $path);
    }
    close $IN;
}

#
# Expand any (possibly nested) macros that were defined after use
#
sub expandRelease {
    my ($Rmacros) = @_;
    # $Rmacros is a reference to a hash

    while (my ($macro, $path) = each %$Rmacros) {
        while (my ($pre,$var,$post) = $path =~ m/(.*)\$\((\w+?)\)(.*)/) {
            warn "Undefined macro \$($var) used in RELEASE file\n"
                unless exists $Rmacros->{$var};
            $path = $pre . $Rmacros->{$var} . $post;
            $Rmacros->{$macro} = $path;
        }
    }
}

1;
