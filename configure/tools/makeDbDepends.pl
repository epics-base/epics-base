eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeDbDepends.pl

# Called from within RULES.Db in the Db directories.
# Searches .substitutions and .template files (from the command line) for
# "file xxx {" entries to create a DEPENDS file
#  and 
# 'include "xxx"' entries to create a DEPENDS file


$target = $ARGV[0];
shift @ARGV;


foreach $file (@ARGV) {
    open(IN, "<$file") or die "Cannot open $file: $!";
    @infile = <IN>;
    close IN or die "Cannot close $file: $!";

    @depends = grep { s/^\s*file\s*(.*)\s*\{.*$/\1/ } @infile;
    chomp @depends;

    if (@depends) {
	print "$target: @depends\n";
    }

    @depends2 = grep { s/^\s*include\s*\"\s*(.*)\s*\".*$/\1/ } @infile;
    chomp @depends2;

    if (@depends2) {
	print "$target: @depends2\n";
    }
}
