#!/usr/bin/env perl
#

use File::Basename;
use File::Spec;
use Text::Wrap;

use strict;
use warnings;

use Getopt::Std;
$Getopt::Std::STANDARD_HELP_VERSION = 1;

use Pod::Usage;

=head1 NAME

epicsMakeMemFs.pl - Generate C code file containing in-memory filesystem

=head1 SYNOPSIS

B<epicsMakeMemFs.pl> [B<-h>] [B<-z>] [B<-T> dir] [B<-P> dir] [B<-f> I<memfs> | I<imf>] output.c varname file1 file2 ...

=head1 OPTIONS

B<epicsMakeMemFs.pl> understands the following options:

=over 4

=item B<-h>

Help, display this document as text.

=item B<-z>

Enable use of zlib compression.

=item B<-T> dir

Store paths relative to this directory.  (eg. "$(TOP)")

=item B<-f> I<memfs> | I<imf>

Selects output format.  I<memfs> is the default.

=item B<-P> dir

With I<imf> format, prepend directory to all files.

=back

=cut

our ($opt_z, $opt_T, $opt_P, $opt_f, $opt_h);

$opt_T = File::Spec->curdir();
$opt_f = "memfs";

sub HELP_MESSAGE {
    pod2usage(-exitval => 2, -verbose => $opt_h ? 2 : 0);
}

HELP_MESSAGE() if !getopts('hzT:P:f:') || $opt_h || @ARGV <= 2;

if($opt_z && !eval "require Compress::Zlib; 1") {
    $opt_z = 0;
}

my $outfile = shift @ARGV;
my $varname = shift @ARGV;

# TODO: output to .tmp, then rename
open(my $DST, '>', $outfile)
  or die "Failed to open $outfile";

$Text::Wrap::break = ',';
$Text::Wrap::columns = 78;
$Text::Wrap::separator = ",\n";

if($opt_f eq "memfs") { # ================ Format 1

    my $inputs = join "\n *    ", @ARGV;
    print $DST <<EOF;
/* $outfile containing
 *    $inputs
 */

#include <epicsMemFs.h>

EOF

    my $N = 0;

    for my $fname (@ARGV) {
        my $realfname = $fname;

        # strip leading "../" "./" or "/"
        $fname =~ s(^\.{0,2}/)()g;

        my $file = basename($fname);
        my @dirs  = split('/', dirname($fname));

        print $DST "/* $realfname */\n",
            "static const char * const file_${N}_dir[] = {",
            map("\"$_\", ", @dirs), "NULL};\n",
            "static const char file_${N}_data[] = {\n",
            "  ";

        open(my $SRC, '<', $realfname)
            or die "Failed to open $realfname";
        binmode $SRC;

        my ($buf, @bufs);
        while (read($SRC, $buf, 4096)) {
            @bufs[-1] .= ',' if @bufs;  # Need ',' between buffers
            push @bufs, join(",", map(ord, split(//, $buf)));
        }
        print $DST wrap('', '  ', @bufs);

        close $SRC;

        print $DST <<EOF;

};
static const epicsMemFile file_${N} = {
  file_${N}_dir,
  \"$file\",
  file_${N}_data,
  sizeof(file_${N}_data)
};

EOF
        $N++;
    }

    my $files = join ', ', map "&file_${_}", (0 .. $N-1);

    print $DST <<EOF;
static const epicsMemFile* files[] = {
$files, NULL
};

static
const epicsMemFS ${varname}_image = {&files[0]};
const epicsMemFS * $varname = &${varname}_image;
EOF


} elsif($opt_f eq "imf") { # ================ Format 2

    my $inputs = join "\n *    ", @ARGV;
    print $DST <<EOF;
/* $outfile containing
 *    $inputs
 */

#include <epicsStdio.h>
EOF
    my $N = 0;
    my (@fencode, @alens, @clens);

    for my $fname (@ARGV) {
        my $realfname = $fname;

        $fname = File::Spec->canonpath($fname);
        $fname = File::Spec->rel2abs($fname);
        $fname = File::Spec->abs2rel($fname, $opt_T);

        # attempt Windows to unix path convert
        $fname =~ s(\\)(/)g;

        # strip leading "../" "./" or "/"
        $fname =~ s[^(\.{0,2}/)+][];

        # strip leading O.Common/
        $fname =~ s(^O\.Common/)();

        if($opt_P) {
            $fname = "${opt_P}${fname}";
        }

        open(my $SRC, '<', $realfname)
            or die "Failed to open $realfname";
        binmode $SRC;

        read $SRC, my $actual, -s $SRC;

        close $SRC;

        my $actualLen = length($actual);

        my $actualz;
        if($opt_z) {
            $actualz = Compress::Zlib::compress($actual, 9);
        }

        our ($encode, $content);

        if(defined($actualz) && (length($actualz) < $actualLen)) {
            $encode = "epicsIMFZ";
            $content = $actualz;

        } else {
            $encode = "epicsIMFRaw";
            $content = $actual;
        }

        my $contentLen = length($content);

        $content = wrap('', '        ', join(",", map(ord, split(//, $content))));

        push @fencode, $encode;
        push @alens, $actualLen;
        push @clens, $contentLen;

        print $DST <<EOF;
static const char file_${N}_name[] = "/$fname";
static const char file_${N}_content[] = {
        $content
};
EOF
        $N++;
    }
    print $DST <<EOF;
const epicsIMF ${varname}_imf[] = {
EOF

    $N = 0;
    for my $fname (@ARGV) {

        print $DST <<EOF;
    {
        file_${N}_name,
        file_${N}_content,
        $clens[$N],
        $alens[$N],
        $fencode[$N],
        1,
    },
EOF
        $N++;
    }

    print $DST <<EOF;
    {NULL, NULL, 0, 0, epicsIMFRaw, 0}
};
EOF

} else { # ================ Format Unsupported
  die "Unsupported output format $opt_f";
}

close $DST;
