#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

package EPICS::Readfile;
require 5.000;
require Exporter;

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

sub Readfile {
    my ($file, $macros, $Rpath) = @_;
    print "Readfile($file)\n" if $debug;
    my $input = &expandMacros($macros, &slurp($file, $Rpath));
    my @input = split /\n/, $input;
    my @output;
    foreach (@input) {
        if (m/^ \s* include \s+ $string /ox) {
            $arg = &unquote($1);
            print " include $arg\n" if $debug;
            push @output, "##! include \"$arg\"";
            push @output, &Readfile($arg, $macros, $Rpath);
        } elsif (m/^ \s* addpath \s+ $string /ox) {
            $arg = &unquote($1);
            print " addpath $arg\n" if $debug;
            push @output, "##! addpath \"$arg\"";
            push @{$Rpath}, &splitPath($arg);
        } elsif (m/^ \s* path \s+ $string /ox) {
            $arg = &unquote($1);
            print " path $arg\n" if $debug;
            push @output, "##! path \"$arg\"";
            @{$Rpath} = &splitPath($arg);
        } else {
            push @output, $_;
        }
    }
    return join "\n", @output;
}

1;
