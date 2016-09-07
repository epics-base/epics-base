#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2015 ITER Organization.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use strict;
use warnings;

use Getopt::Std;
use Sys::Hostname;
use File::Basename;
use Data::Dumper;

our ($opt_o, $opt_d, $opt_m, $opt_i, $opt_M);

$Getopt::Std::OUTPUT_HELP_VERSION = 1;
&HELP_MESSAGE if !getopts('M:i:m:o:d') || @ARGV == 0;

my $out;
my $dep;
my %snippets;
my $ipattern;

my $datetime = localtime();
my $user = $ENV{LOGNAME} || $ENV{USER} || $ENV{USERNAME};
my $host = hostname;
my %replacements = (
    _DATETIME_ => $datetime,
    _USERNAME_ => $user,
    _HOST_ => $host,
);

if ($opt_o) {
    open $out, '>', $opt_o or
        die "Can't create $opt_o: $!\n";
    print STDERR "opened file $opt_o for output\n" if $opt_d;
    $replacements{_OUTPUTFILE_} = $opt_o;
} else {
    open $out, '>&', STDOUT;
    print STDERR "using STDOUT for output\n" if $opt_d;
    $replacements{_OUTPUTFILE_} = 'STDERR';
}

if ($opt_m) {
    foreach my $r (split /,/, $opt_m) {
        (my $k, my $v) = split /=/, $r;
        $replacements{$k} = $v;
    }
}

if ($opt_M) {
    open $dep, '>', $opt_M or
        die "Can't create $opt_M: $!\n";
    print STDERR "opened dependency file $opt_M for output\n" if $opt_d;
    print $dep basename($opt_o), ":";
}

if ($opt_i) {
    $ipattern = qr($opt_i);
}

# %snippets is a hash {rank}
#                  of hashes {name-after-rank}
#                      of arrays[] [files...]
#                          of arrays[2] [filename, command]
print STDERR "reading input files\n" if $opt_d;
foreach (@ARGV) {
    my $name = basename($_);
    if ($opt_i and not $name =~ /$ipattern/) {
        print STDERR "  snippet $_ does not match input pattern $opt_i - ignoring\n" if $opt_d;
        next;
    }
    if ($name =~ /\A([ARD]?)([0-9]+)(.*[^~])\z/) {
        print STDERR "  considering snippet $_\n" if $opt_d;
        if (exists $snippets{$2}) {
            my %rank = %{$snippets{$2}};
            my @files = @{ $rank{(keys %rank)[0]} };
            my $existcmd = $files[0]->[1];
            if ($1 eq "D" and $existcmd ne "D") {
                print STDERR "    ignoring 'D' default for existing rank $2\n" if $opt_d;
                next;
            } elsif ($1 eq "R") {
                print STDERR "    'R' command - deleting existing rank $2 snippets\n" if $opt_d;
                $snippets{$2} = {};
            } elsif ($existcmd eq "D") {
                print STDERR "    deleting existing rank $2 default snippet\n" if $opt_d;
                $snippets{$2} = {};
            }
        }
        if ($opt_d) {
            print STDERR "    adding snippet ";
            print STDERR "marked as default " if $1 eq "D";
            print STDERR "to rank $2\n";
        }
        $snippets{$2}{$3} = () if (not exists $snippets{$2}{$3});
        push @{$snippets{$2}{$3}}, [ $_, $1 ];
    }
}

if ($opt_d) {
    print STDERR "finished reading input files\n";
    print STDERR "dumping the final snippet structure\n";
    print STDERR Dumper(\%snippets);
    print STDERR "dumping the macro replacements\n";
    print STDERR Dumper(\%replacements);
    print STDERR "creating output\n";
}

foreach my $r (sort {$a<=>$b} keys %snippets) {
    print STDERR "  working on rank $r\n" if $opt_d;
    foreach my $n (sort keys %{$snippets{$r}}) {
        foreach my $s (@{$snippets{$r}{$n}}) {
            my $in;
            my $f = $s->[0];
            print STDERR "    snippet $n from file $f\n" if $opt_d;
            open $in, '<', $f or die "Can't open $f: $!\n";
            $replacements{_SNIPPETFILE_} = $f;
            print $dep " \\\n $f" if $opt_M;
            while (<$in>) {
                chomp;
                foreach my $k (keys %replacements) {
                    s/$k/$replacements{$k}/g;
                }
                print $out $_, "\n";
            }
            close $in;
        }
    }
}

print STDERR "finished creating output, closing\n" if $opt_d;
if ($opt_M) {
    print $dep "\n";
    close $dep;
}
close $out;

sub HELP_MESSAGE {
    print STDERR "Usage: assembleSnippets.pl [options] snippets ...\n";
    print STDERR "Options:\n";
    print STDERR " -o file     output file [STDOUT]\n";
    print STDERR " -d          debug mode [no]\n";
    print STDERR " -m macros   list of macro replacements as \"key=val,key=val\"\n";
    print STDERR " -i pattern  pattern for input files to match\n";
    print STDERR " -M file     write file with dependency rule suitable for make\n";
    exit 2;
}
