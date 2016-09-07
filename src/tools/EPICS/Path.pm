#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

package EPICS::Path;
require 5.000;
require Exporter;

=head1 NAME

EPICS::Path - Path-handling utilities for EPICS tools

=head1 SYNOPSIS

  use lib '/path/to/base/lib/perl';
  use EPICS::Path;

  my $dir = UnixPath('C:\Program Files\EPICS');
  print LocalPath($dir), "\n";
  print AbsPath('../lib', $dir);

=head1 DESCRIPTION

This module provides functions for processing pathnames that are commonly needed
by EPICS tools. Windows is not the only culprit, some older automount daemons
insert strange prefixes into absolute directory paths that we have to remove
before storing the result for use later.

Note that these functions are only designed to work on operating systems that
recognize a forward slash C</> as a directory separator; this does include
Windows, but not pre-OS-X versions of MacOS which used a colon C<:> instead.

=head1 FUNCTIONS

=over 4

=cut

use Carp;
use Cwd qw(getcwd abs_path);
use File::Spec;

@ISA = qw(Exporter);
@EXPORT = qw(UnixPath LocalPath AbsPath);

=item B<C<UnixPath($path)>>

C<UnixPath> should be used on any pathnames provided by external tools to
convert them into a form that Perl understands.

On cygwin we convert Windows drive specs to the equivalent cygdrive path, and on
Windows we switch directory separators from back-slash to forward slashes.

=cut

sub UnixPath {
    my ($newpath) = @_;
    if ($^O eq 'cygwin') {
        $newpath =~ s{\\}{/}go;
        $newpath =~ s{^([a-zA-Z]):/}{/cygdrive/$1/};
    } elsif ($^O eq 'MSWin32') {
        $newpath =~ s{\\}{/}go;
    }
    return $newpath;
}

=item B<C<LocalPath($path)>>

C<LocalPath> should be used when generating pathnames for external tools or to
put into a file.  It converts paths from the Unix form that Perl understands to
any necessary external representation, and also removes automounter prefixes to
put the path into its canonical form.

Before Leopard, the Mac OS X automounter inserted a verbose prefix, and in case
anyone is still using SunOS (pre-Solaris) it added its own prefix as well.

=cut

sub LocalPath {
    my ($newpath) = @_;
    if ($^O eq 'darwin') {
        # Darwin automounter
        $newpath =~ s{^/private/var/auto\.}{/};
    } elsif ($^O eq 'sunos') {
        # SunOS automounter
        $newpath =~ s{^/tmp_mnt/}{/};
    }
    return $newpath;
}

=item B<C<AbsPath($path)>>

=item B<C<AbsPath($path, $cwd)>>

The C<abs_path()> function in Perl's F<Cwd> module doesn't like non-existent
path components other than in the final position, but EPICS tools needs to be
able to handle them in paths like F<$(TOP)/lib/$(T_A)> before the F<$(TOP)/lib>
directory has been created.

C<AbsPath> takes a path C<$path> and optionally an absolute path to a directory
that the first is relative to; if the second argument is not provided the
current working directory is used instead. The result returned has also been
filtered through C<LocalPath()> to remove any automounter prefixes.

=cut

sub AbsPath {
    my ($path, $cwd) = @_;

    $path = '.' unless defined $path;

    if (defined $cwd) {
        croak("'$cwd' is not an absolute path")
            unless $cwd =~ m[^ / ]x;
    } else {
        $cwd = getcwd();
    }

    # Move leading ./ and ../ components from $path to $cwd
    if (my ($dots, $not) = ($path =~ m[^ ( (?: \. \.? /+ )+ ) ( .* ) $]x)) {
        $cwd .= "/$dots";
        $path = $not;
    }

    # Handle any trailing .. part
    if ($path eq '..') {
        $cwd .= '/..';
        $path = '.'
    }

    # Now calculate the absolute path
    my $abs = File::Spec->rel2abs($path, abs_path($cwd));
    $abs = abs_path($abs) if -e $abs;

    return LocalPath($abs);
}

=back

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2008 UChicago Argonne LLC, as Operator of Argonne National
Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut

1;
