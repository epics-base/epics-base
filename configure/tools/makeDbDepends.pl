eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeDbDepends.pl

# Called from within the object directory.
# Searches the .substitutions file (from the command line) for
# "file xxx {" entries to create a dependancy output file

sub Usage
{
    my ($txt) = @_;

    print "Usage:\n";
    print "\makeDbDepends dbfile substitutionfile dependsfile\n";
    print "\nError: $txt\n" if $txt;

    exit 2;
}

# need at least three args: ARGV[2]
Usage("need three args") if $#ARGV < 2;

$dbfile = $ARGV[0];
$subfile = $ARGV[1];
$outfile = $ARGV[2];

open(OUT, "> $outfile") or die "Cannot open output file $outfile: $!";
open(IN, "<$subfile") or die "Cannot open $subfile: $!";
@substfile = <IN>;
close IN or die "Cannot close $subfile: $!";

@depends = grep { s/^\s*file\s*(.*)\s*\{.*$/\1/ } @substfile;
chomp @depends;

if (@depends) {
	$file =~ s/\.substitutions/\.db/;
	print OUT "${dbfile}:: @depends\n";
}
close OUT or die "Cannot close $outfile: $!";
