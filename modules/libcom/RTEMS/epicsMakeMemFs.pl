#!/usr/bin/env perl
#

use File::Basename;
use Text::Wrap;

use strict;

my $outfile = shift;
my $varname = shift;

open(my $DST, '>', $outfile)
  or die "Failed to open $outfile";

my $inputs = join "\n *    ", @ARGV;
print $DST <<EOF;
/* $outfile containing
 *    $inputs
 */

#include <epicsMemFs.h>

EOF

my $N = 0;

$Text::Wrap::break = ',';
$Text::Wrap::columns = 78;
$Text::Wrap::separator = ",\n";

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

close $DST;
