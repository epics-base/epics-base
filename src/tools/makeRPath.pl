#!/usr/bin/env perl
#*************************************************************************
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

use Getopt::Long;
use Cwd qw(abs_path getcwd);
use File::Spec;
use File::Basename;
use Pod::Usage;

# Example:
#   target to be installed as: /build/bin/blah
#   post-install will copy as: /install/bin/blah
#
# Need to link against:
#   /install/lib/libA.so
#   /build/lib/libB.so
#   /other/lib/libC.so
#
# Want final result to be:
#   -rpath $ORIGIN/../lib -rpath /other/lib \
#   -rpath-link /build/lib -rpath-link /install/lib

warn "[" . join(' ', $0, @ARGV) . "]\n"
    if $ENV{EPICS_DEBUG_RPATH} && $ENV{EPICS_DEBUG_RPATH} eq 'YES';

# Defaults for command-line arguments
my $final = getcwd();
my $root = '';
my $origin = '$ORIGIN';
my $help = 0;

# Parse command-line arguments
GetOptions(
    'final|F=s'  => \$final,
    'root|R=s'   => \$root,
    'origin|O=s' => \$origin,
    'help|h'     => \$help,
) or pod2usage(
    -exitval => 2,
    -verbose => 1,
    -noperldoc => 1,
);

# Display help message if requested
pod2usage(
    -exitval => 1,
    -verbose => 2,
    -noperldoc => 1,
) if $help;

# Convert paths to absolute
$final = abs_path($final);
my @roots = map { abs_path($_) } grep { length($_) } split(/:/, $root);

# Determine the root containing the final location
my $froot;
foreach my $root (@roots) {
    my $frel = File::Spec->abs2rel($final, $root);
    if ($frel !~ /^\.\./) {
        $froot = $root;
        last;
    }
}

if (!defined $froot) {
    warn "makeRPath: Final location $final\n" .
         "Not under any of: @roots\n";
    @roots = ();  # Skip $ORIGIN handling below
}

# Prepare output
my (@output, %output);
foreach my $path (@ARGV) {
    $path = abs_path($path);
    foreach my $root (@roots) {
        my $rrel = File::Spec->abs2rel($path, $root);
        if ($rrel !~ /^\.\./) {
            # Add rpath-link for internal use by 'ld'
            my $opt = "-Wl,-rpath-link,$path";
            push @output, $opt unless $output{$opt}++;

            # Calculate relative path
            my $rel_path = File::Spec->abs2rel($rrel, $final);
            my $opath = File::Spec->catfile($origin, $rel_path);
            $opt = "-Wl,-rpath,$opath";
            push @output, $opt unless $output{$opt}++;
            last;
        }
    }
}

# Print the output
print join(' ', @output), "\n";

__END__

=head1 NAME

makeRPath.pl - Compute and output -rpath entries for given paths

=head1 SYNOPSIS

makeRPath.pl [options] [path ...]

=head1 OPTIONS

    -h, --help      Display detailed help and exit
    -F, --final     Final install location for ELF file
    -R, --root      Root(s) of relocatable tree, separated by ':'
    -O, --origin    Origin path (default: '$ORIGIN')

=head1 DESCRIPTION

Computes and outputs -rpath entries for each of the given paths.
Paths under C<--root> will be computed as relative to C<--final>.

=head1 EXAMPLE

A library to be placed in C</build/lib> and linked against libraries in
C</build/lib>, C</build/module/lib>, and C</other/lib> would pass

  makeRPath.pl -F /build/lib -R /build /build/lib /build/module/lib /other/lib

which generates

  -Wl,-rpath,$ORIGIN/. -Wl,-rpath,$ORIGIN/../module/lib -Wl,-rpath,/other/lib

=cut
