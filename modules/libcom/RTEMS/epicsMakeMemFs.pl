#!/usr/bin/env perl
#

use File::Basename;

use strict;

my $outfile = shift;
my $varname = shift;

open(my $DST, '>', $outfile) or die "Failed to open $outfile";

print $DST <<EOF;
#include <epicsMemFs.h>
EOF

my $N = 0;

for my $fname (@ARGV) {
  print "<- $fname\n";
  my $realfname = $fname;

  # strip leading "../" "./" or "/"
  while ($fname =~ /^\.*\/(.*)$/) { $fname = $1; }

  my $file = basename($fname);
  my @dirs  = split('/', dirname($fname));

  print $DST "/* begin $realfname */\nstatic const char * const file_${N}_dir[] = {";
  for my $dpart (@dirs) {
    print $DST "\"$dpart\", ";
  }
  print $DST "NULL};\nstatic const char file_${N}_data[] = {\n   ";

  open(my $SRC, '<', $realfname) or die "Failed to open $realfname";
  binmode($SRC);

  my $buf;
  my $total = 0;
  while (my $num = read($SRC, $buf, 32)) {
    if($total != 0) {
      print $DST ",\n   ";
    }
    $total += $num;
    my $out = join(",",map(ord,split(//,$buf)));
    print $DST "$out";
  }
  
  close($SRC);

  print $DST <<EOF;

};
static const epicsMemFile file_${N} = {
  file_${N}_dir,
  \"$file\",
  file_${N}_data,
  $total
};
/* end $realfname */
EOF
  $N = $N + 1;
}

print $DST <<EOF;
static const epicsMemFile* files[] = {
EOF

$N = $N - 1;

for my $i (0..${N}) {
  print $DST " &file_${i},";
}

print $DST <<EOF;
 NULL
};
static
const epicsMemFS ${varname}_image = {&files[0]};
const epicsMemFS * $varname = &${varname}_image;
EOF

close($DST);
