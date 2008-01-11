eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if 0;

# Determines an absolute pathname for its argument,
# which may be either a relative or absolute path and
# might have trailing parts that don't exist yet.

use strict;
use Cwd qw(getcwd abs_path);
use File::Spec;

# Starting values
my $cwd = getcwd();
my $rel = shift;

$rel = '.' unless defined $rel;

# Move leading ./ and ../ components from $rel to $cwd
if (my ($dot, $not) = ($rel =~ m[^ ( (?: \. \.? / )+ ) ( .* ) $]x)) {
    $cwd .= "/$dot";
    $rel = $not;
}

# Handle a pure ..
if ($rel eq '..') {
    $cwd .= '/..';
    $rel = '.'
}

# NB: abs_path() doesn't like non-existent path components other than
# in the final position, which is why we use this method.

# Calculate the absolute path
my $abs = File::Spec->rel2abs($rel, abs_path($cwd));

# Remove any automounter prefixes
$abs =~ s[^ /tmp_mnt ][]x;              # SunOS, HPUX
$abs =~ s[^ /private/var/auto\. ][/]x;  # MacOS

print "$abs\n";
