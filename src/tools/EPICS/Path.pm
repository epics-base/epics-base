#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use Carp;
use Cwd qw(getcwd abs_path);
use File::Spec;

=head1 EPICS::Path

EPICS::Path - Path-handling utilities for EPICS tools

=head1 SYNOPSIS

  use lib '@EPICS_BASE@/lib/perl';
  use EPICS::Path;

  my $dir = UnixPath('C:\Program Files\EPICS');
  print LocalPath($dir), "\n";
  print AbsPath('../lib', $dir);

=head1 DESCRIPTION

C<EPICS::Path> provides functions for processing pathnames that are commonly
needed by EPICS tools.  Windows is not the only culprit, some older automount
daemons insert strange prefixes into absolute directory paths that we have to
remove before storing the result for use later.


=head1 FUNCTIONS

=over 4

=item UnixPath( I<PATH> )

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

=item LocalPath( I<PATH> )

C<LocalPath> should be used when generating pathnames for external tools or to
put into a file.  It converts paths from the Unix form that Perl understands to
any necessary external representation, and also removes automounter prefixes to
put the path into its canonical form.

Before Leopard, the Mac OS X automounter inserted a verbose prefix, and in case
anyone is still using SunOS it adds its own prefix as well.

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

=item AbsPath( I<PATH> )

=item AbsPath( I<PATH>, I<CWD> )

The C<abs_path()> function in Perl's C<Cwd> module doesn't like non-existent
path components other than in the final position, but EPICS tools needs to be
able to handle them in paths like F<$(TOP)/lib/$(T_A)> before the F<$(TOP)/lib>
directory has been created.

C<AbsPath> takes a path I<PATH> and optionally an absolute path to a directory
that first is relative to; if the second argument is not provided the current
working directory is used.  The result returned has been filtered through
C<LocalPath()> to remove any automounter prefixes.

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
