eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
        if 0; 

#
# Determine fullpathname if argument starts with "."
# else return argument value.
#

use Cwd 'abs_path';
my $dir;
if( $ARGV[0] ) {
  if( $ARGV[0] =~ /^\./ )
  {
    $dir = abs_path("$ARGV[0]");
	$dir =~ s/\/tmp_mnt//;     
    print "$dir\n";
  } else  {
    print "$ARGV[0]\n";
  }
}
