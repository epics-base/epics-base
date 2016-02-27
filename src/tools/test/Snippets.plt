#!/usr/bin/env perl

use File::Path;
use Sys::Hostname;

use Test::More tests => 29;

my $user = $ENV{LOGNAME} || $ENV{USER} || $ENV{USERNAME};
my $host = hostname;

mkdir "a$$";
mkdir "b$$";

sub mksnip {
    my ($dir, $file, $line) = @_;
    open(my $fh, '>', "$dir$$/$file") or die "can't open $dir$$/$file : $!";
    print $fh $line;
    close $fh;
}

sub assemble {
    my @cmd = ( 'perl', '../../assembleSnippets.pl', '-o', "out$$" );
    push @cmd, @_;
    system(@cmd);
    open(my $fh, '<', "out$$") or die "can't open out$$ : $!";
    chomp(my @result = <$fh>);
    close $fh;
    return join (' ', @result);
}

# Adding two snippets of same rank, sorting alphabetically
mksnip('a', '10_a', '10');
mksnip('b', '10_c', '12');
is assemble("a$$/10_a", "b$$/10_c"), '10 12', "adding same rank; ordered";
is assemble("b$$/10_c", "a$$/10_a"), '10 12', "adding same rank; reverse order";

# Same, with 'A' cmd
mksnip('a', 'A10_a', '10');
mksnip('b', 'A10_c', '12');
is assemble("a$$/10_a", "b$$/A10_c"), '10 12', "'A' add same rank; ordered";
is assemble("b$$/10_c", "a$$/A10_a"), '10 12', "'A' add same rank; reverse order";

# Same name does not create issues
mksnip('b', '10_a', '10x');
is assemble("a$$/10_a", "b$$/10_a"), '10 10x', "adding same name twice; order a-b";
is assemble("b$$/10_a", "a$$/10_a"), '10x 10', "adding same name twice; order b-a";

# Backup files (trailing ~) and hidden files (leading '.') get ignored
mksnip('b', '10_c~', '12~');
mksnip('b', '.10_c', '.12');
is assemble("b$$/10_c", "b$$/10_c~"), '12', "backup file (trailing ~) gets ignored";
is assemble("b$$/10_c", "b$$/.10_c"), '12', "hidden file (leading .) gets ignored";

# Non-numeric filenames get ignored
mksnip('a', 'foo10_a', 'foo10');
is assemble("b$$/10_c", "a$$/foo10_a"), '12', "file starting with [^ADR0-9] gets ignored";

# 'R' command replaces existing snippets of same rank
mksnip('a', 'R10_b', '11');
is assemble("a$$/10_a", "b$$/10_c", "a$$/R10_b"), '11', "'R' cmd; replace all";
is assemble("a$$/10_a", "a$$/R10_b", "b$$/10_c"), '11 12', "'R' cmd; replace one (ordered)";
is assemble("b$$/10_c", "a$$/R10_b", "a$$/10_a"), '10 11', "'R' cmd; replace one (reverse order)";

# 'D' command establishes default that gets overwritten or ignored
mksnip('a', 'D10_a', 'D10');
mksnip('b', 'D10_c', 'D12');
is assemble("a$$/D10_a", "b$$/10_c"),  '12',  "'D' default; replaced by regular";
is assemble("a$$/D10_a", "b$$/D10_c"), 'D12', "'D' default; replaced by new default (ordered)";
is assemble("b$$/D10_c", "a$$/D10_a"), 'D10', "'D' default; replaced by new default (reverse order)";
is assemble("a$$/D10_a", "a$$/R10_b"), '11',  "'D' default; replaced by 'R' cmd";
is assemble("b$$/10_c", "a$$/D10_a"),  '12',  "'D' default; ignored when regular exists";

# Ranks are sorted numerically
mksnip('b', '09_a', '09');
mksnip('a', '15_a', '15');
mksnip('b', '2_a', '2');
is assemble("a$$/10_a", "b$$/2_a", "a$$/15_a", "b$$/09_a"), '2 09 10 15',  "ranks are sorted numerically";

# Builtin macros
mksnip('a', '30_a', '_USERNAME_');
mksnip('a', '30_b', '_OUTPUTFILE_');
mksnip('a', '30_c', '_SNIPPETFILE_');
mksnip('a', '30_d', '_HOST_');
is assemble("a$$/30_a"), "$user", "builtin macro _USERNAME_";
is assemble("a$$/30_b"), "out$$", "builtin macro _OUTPUTFILE_";
is assemble("a$$/30_c"), "a$$/30_c", "builtin macro _SNIPPETFILE_";
is assemble("a$$/30_d"), "$host", "builtin macro _HOST_";

# User macros
mksnip('b', '35_a', 'Line _M1_');
mksnip('b', '35_b', 'Line _M1_ with _M2_');
mksnip('b', '35_c', 'Line _M2_ with _M2_');
is assemble("-m", "_M1_=REP1", "b$$/35_a"), "Line REP1", "single user macro; single occurrence";
is assemble("-m", "_M1_=REP1,_M2_=REP2", "b$$/35_b"), "Line REP1 with REP2", "multiple user macros";
is assemble("-m", "_M2_=REP2", "b$$/35_c"), "Line REP2 with REP2", "single user macro; multiple occurrences";

# Input pattern
mksnip('a', '10_a.cmd', '10cmd');
is assemble("-i", "\.cmd", "a$$/10_a", "b$$/10_c", "a$$/R10_b", "a$$/10_a.cmd"), '10cmd', "input pattern";

# Dependency file generation
assemble("-M", "./dep$$", "a$$/10_a", "b$$/10_c");
open(my $fh, '<', "dep$$") or die "can't open dep$$ : $!";
chomp(my @result = <$fh>);
close $fh;
is "$result[0]", "out$$: \\", "dependency file (line 1)";
is "$result[1]", " a$$/10_a \\", "dependency file (line 2)";
is "$result[2]", " b$$/10_c", "dependency file (line 3)";

rmtree([ "a$$", "b$$", "out$$", "dep$$" ]);
