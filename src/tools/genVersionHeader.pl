#!/usr/bin/env perl
#
# Generate a C header file which
# defines a macro with a string
# describing the VCS revision
#

use EPICS::Getopts;

use strict;

our($opt_v,$opt_t,$opt_N,$opt_V);

$opt_N = "MODVERSION";
$opt_V = "undefined";
$opt_t = ".";
my $foundvcs = 0;
my $result;

getopts("vt:N:V:") or 
    die "Usage: genVersionHeader.pl [-t top] [-N NAME] [-V VERSION] output.h";

my ($outfile) = @ARGV;

if(-d "$opt_t/.hg") { # Mercurial
    # v1-4-abcdef-dirty
    # is 4 commits after tag 'v1' with short hash abcdef
    # with uncommited modifications
    $result = `cd "$opt_t" && hg tip --template '{latesttag}-{latesttagdistance}-{node|short}\n'`;
    chomp($result);
    if(length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
    }
    # see if working copy has modifications, additions, removals, or missing files
    my $hasmod = `cd "$opt_t" && hg status -m -a -r -d`;
    chomp($hasmod);
    if(length($hasmod)>0) {
        $opt_V = "$opt_V-dirty";
    }
}
if(!$foundvcs && -d "$opt_t/.git") {
    # same format as Mercurial
    $result = `git --git-dir="$opt_t/.git" describe --tags --dirty`;
    chomp($result);
    if(length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
    }
}

my $output = "#ifndef $opt_N\n# define $opt_N \"$opt_V\"\n#endif\n";
print "== would\n$output" if $opt_v;

my $DST;
if(open($DST, '+<', $outfile)) {
    my $actual = join("", <$DST>);
    print "== have\n$actual" if $opt_v;

    if($actual eq $output) {
        print "keep existing $outfile\n";
        exit(0)
    }
    print "updating $outfile\n";
} else {
    print "create $outfile\n";
    open($DST, '>', $outfile) or die "Unable to open or create $outfile";
}

seek($DST,0,0);
truncate($DST,0);
print $DST $output;

close($DST);
