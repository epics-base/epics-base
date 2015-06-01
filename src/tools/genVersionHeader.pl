#!/usr/bin/env perl
#
# Generate a C header file which
# defines a macro with a string
# describing the VCS revision
#

use EPICS::Getopts;
use POSIX qw(strftime);

use strict;

our($opt_v,$opt_t,$opt_N,$opt_V);

$opt_N = "MODVERSION";
$opt_t = ".";
my $foundvcs = 0;
my $result;

getopts("vt:N:V:") or 
    die "Usage: genVersionHeader.pl [-t top] [-N NAME] [-V VERSION] output.h";

my ($outfile) = @ARGV;

chomp($opt_V);
if(!$opt_V) {
    # RFC 8601 date+time w/ zone (eg "2014-08-29T09:42:47-0700")
    $opt_V = strftime "%Y-%m-%dT%H:%M:%S%z", localtime;
}

if(!$foundvcs && -d "$opt_t/_darcs") { # Darcs
    # v1-4-dirty
    # is tag 'v1' plus 4 patches
    # with uncommited modifications
    $result = `cd "$opt_t" && echo "\$(darcs show tags | head -1)-\$((\$(darcs changes --count --from-tag .)-1))"`;
    chomp($result);
    if(!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `darcs whatsnew --repodir="$opt_t" -l`;
        if(!$?) {
            $opt_V = "$opt_V-dirty";
        }
    }
}
if(!$foundvcs && -d "$opt_t/.hg") { # Mercurial
    # v1-4-abcdef-dirty
    # is 4 commits after tag 'v1' with short hash abcdef
    # with uncommited modifications
    $result = `hg --cwd "$opt_t" tip --template '{latesttag}-{latesttagdistance}-{node|short}\n'`;
    chomp($result);
    if(!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `hg --cwd "$opt_t" status -m -a -r -d`;
        chomp($hasmod);
        if(length($hasmod)>0) {
            $opt_V = "$opt_V-dirty";
        }
    }
}
if(!$foundvcs && -d "$opt_t/.git") {
    # same format as Mercurial
    $result = `git --git-dir="$opt_t/.git" describe --tags --dirty`;
    chomp($result);
    if(!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
    }
}
if(!$foundvcs && -d "$opt_t/.svn") {
    # 12345
    $result = `cd "$opt_t" && svn info --non-interactive`;
    chomp($result);
    if(!$? && $result =~ /^Revision:\s*(\d+)/m) {
        $opt_V = $1;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        my $hasmod = `cd "$opt_t" && svn status -q --non-interactive`;
        chomp($hasmod);
        if(length($hasmod)>0) {
            $opt_V = "$opt_V-dirty";
        }
    }
}
if(!$foundvcs && -d "$opt_t/.bzr") {
    # 12444-anj@aps.anl.gov-20131003210403-icfd8mc37g8vctpf
    $result = `cd "$opt_t" && bzr version-info -q --custom --template="{revno}-{revision_id}"`;
    chomp($result);
    print "BZR $result";
    if(!$? && length($result)>1) {
        $opt_V = $result;
        $foundvcs = 1;
        # see if working copy has modifications, additions, removals, or missing files
        # unfortunately "bzr version-info --check-clean ..." doesn't seem to work as documented
        my $hasmod = `cd "$opt_t" && bzr status -SV`;
        chomp($hasmod);
        if(length($hasmod)>0) {
            $opt_V = "$opt_V-dirty";
        }
    }
}

my $output = "#ifndef $opt_N\n# define $opt_N \"$opt_V\"\n#endif\n";
print "== would\n$output" if $opt_v;

my $DST;
if(open($DST, '+<', $outfile)) {
    my $actual = join("", <$DST>);
    print "== have\n$actual" if $opt_v;

    if($actual eq $output) {
        print "Keeping existing VCS version header $outfile with \"$opt_V\"\n";
        exit(0)
    }
    print "Updating VCS version header $outfile with \"$opt_V\"\n";
} else {
    print "Creating VCS version header $outfile with \"$opt_V\"\n";
    open($DST, '>', $outfile) or die "Unable to open or create VCS version header $outfile";
}

seek($DST,0,0);
truncate($DST,0);
print $DST $output;

close($DST);
