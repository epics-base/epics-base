eval 'exec perl -S -w  $0 ${1+"$@"}'  # -*- Mode: perl -*-
        if 0; 

use Cwd 'abs_path';
my $dir;
$dir = abs_path("$ARGV[0]");
print "$dir\n";

