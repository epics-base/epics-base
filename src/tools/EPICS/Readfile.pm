#*************************************************************************
# Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

package EPICS::Readfile;
require 5.000;
require Exporter;

=head1 NAME

EPICS::Readfile - DBD and DB-file input for EPICS tools

=head1 SYNOPSIS

  use lib '/path/to/base/lib/perl';
  use EPICS::macLib;
  use EPICS::Readfile;

  my $macros = EPICS::macLib->new('a=1', 'b=2');
  my @path = qw(../dbd /opt/epics/base/dbd);
  my $contents = Readfile('input.dbd', $macros, \@path);
  printf "Read in %d files", scalar @inputfiles;

=head1 DESCRIPTION

This module provides a function for reading DBD and DB files that is commonly
needed by EPICS tools.

=cut

use EPICS::macLib;

@ISA = qw(Exporter);
@EXPORT = qw(@inputfiles &Readfile);

our $debug=0;
our @inputfiles;

sub slurp {
    my ($FILE, $Rpath) = @_;
    my @path = @{$Rpath};
    print "slurp($FILE):\n" if $debug;
    if ($FILE !~ m[/]) {
        foreach $dir (@path) {
            print " trying $dir/$FILE\n" if $debug;
            if (-r "$dir/$FILE") {
                $FILE = "$dir/$FILE";
                last;
            }
        }
        die "Can't find file '$FILE'\n" unless -r $FILE;
    }
    print " opening $FILE\n" if $debug;
    open FILE, "<$FILE" or die "Can't open $FILE: $!\n";
    push @inputfiles, $FILE;
    my @lines = ("##!BEGIN{$FILE}!##\n");
    # Consider replacing these markers with C pre-processor linemarkers.
    # See 'info cpp' * Preprocessor Output:: for details.
    push @lines, <FILE>;
    push @lines, "##!END{$FILE}!##\n";
    close FILE or die "Error closing $FILE: $!\n";
    print " read ", scalar @lines, " lines\n" if $debug;
    return join '', @lines;
}

sub expandMacros {
    my ($macros, $input) = @_;
    return $input unless $macros;
    return $macros->expandString($input);
}

sub splitPath {
    my ($path) = @_;
    my (@path) = split /[:;]/, $path;
    grep s/^$/./, @path;
    return @path;
}

my $RXstr = qr/ " (?: [^"] | \\" )* "/ox;
my $RXnam = qr/[a-zA-Z0-9_\-:.[\]<>;]+/o;
my $string = qr/ ( $RXnam | $RXstr ) /ox;

sub unquote {
    my ($s) = @_;
    $s =~ s/^"(.*)"$/$1/o;
    return $s;
}


=head1 FUNCTIONS

=over 4

=item B<C<Readfile($file, $macros, \@path)>>

This function reads an EPICS DBD or DB file into a string, substitutes any
macros present, then parses the contents for any C<include>, C<addpath> and
C<path> commands found therein and recursively executes those commands. The
return value from the function is a string comprising the fully expanded
contents of those files. Before executing them any commands will be replaced
with specially formatted comments that allow the original command to be
recovered during later parsing.

I<Readfile> takes as arguments the input filename, an optional handle to a set
of macro values from L<EPICS::macLib|macLib>, and a reference to an array
containing the current search path.

If macro expansion is not required, the second argument may be any boolean False
value such as C<0> or C<()>. See L<EPICS::macLib|macLib> for more details about
this argument.

The path argument is a reference to an array of directory paths to be searched,
in order. These paths may be used to locate the original input file and any
include files that it references. The path array will be modified by any
C<addpath> or C<path> commands found while parsing the input files.

While processing input filenames (either the original argument or an include
filename) if the filename does not contain any forward-slash C</> characters the
path will be searched and the first file matching that name will be used. If the
filename contains one or more forward-slash characters it must be either an
absolute path or one that is relative to the current working directory; the
search path is not used in this case.

=back

=head1 VARIABLES

=over 4

=item B<C<@inputfiles>>

As new files are processed their names are added to this array which may be
examimed after the I<Readfile> function returns, for example to calculate the
complete set of dependencies for the input file.

=back

=cut

sub Readfile {
    my ($file, $macros, $Rpath) = @_;
    print "Readfile($file)\n" if $debug;
    my $input = expandMacros($macros, slurp($file, $Rpath));
    my @input = split /\n/, $input;
    my @output;
    foreach (@input) {
        if (m/^ \s* include \s+ $string /ox) {
            $arg = unquote($1);
            print " include $arg\n" if $debug;
            push @output, "##! include \"$arg\"";
            push @output, Readfile($arg, $macros, $Rpath);
        } elsif (m/^ \s* addpath \s+ $string /ox) {
            $arg = unquote($1);
            print " addpath $arg\n" if $debug;
            push @output, "##! addpath \"$arg\"";
            push @{$Rpath}, splitPath($arg);
        } elsif (m/^ \s* path \s+ $string /ox) {
            $arg = unquote($1);
            print " path $arg\n" if $debug;
            push @output, "##! path \"$arg\"";
            @{$Rpath} = splitPath($arg);
        } else {
            push @output, $_;
        }
    }
    return join "\n", @output;
}

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2015 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut

1;
