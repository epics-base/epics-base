# $Id$ 

eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeIocCdCommands.pl

use Cwd;

$arch = $ARGV[0];
$outfile = $ARGV[1];
$top = $ARGV[2];

unlink("${outfile}");
open(OUT,">${outfile}") or die "$! opening ${outfile}";
print OUT "#Do not modify thie file.\n";
print OUT "#This file is created during the build.\n";

@files =();
push(@files,"$top/config/RELEASE");
push(@files,"$top/config/RELEASE.${arch}");
foreach $file (@files) {
  if (-r "$file") {
    open(IN, "$file") or die "Cannot open $file\n";
    while ($line = <IN>) {
        next if ( $line =~ /\s*#/ );
	chomp($line);
        $_ = $line;
        #the following looks for
        # prefix = $(macro)post
        ($prefix,$macro,$post) = /(.*)\s*=\s*\$\((.*)\)(.*)/;
        if ($macro eq "") { # true if no macro is present
            # the following looks for
            # prefix = post
            ($prefix,$post) = /(.*)\s*=\s*(.*)/;
        } else {
            $base = $applications{$macro};
            if ($base eq "") {
                #print "error: $macro was not previously defined\n";
            } else {
                $post = $base . $post;
            }
        }
        $applications{$prefix} = $post;
        if ( -d "$post") { #check that directory exists
            print OUT "\n";
            if ( -d "$post/bin/$arch") { #check that directory exists
                print OUT "${prefix}_BIN = $post/bin/${arch}\n";
            }
            if ( -d "$post/lib/$arch") { #check that directory exists
                print OUT "${prefix}_LIB = $post/lib/${arch}\n";
            }
            if ( -d "$post/include") { #check that directory exists
                print OUT "USR_INCLUDES += -I$post/include\n";
            }
            if ( -d "$post/dbd") { #check that directory exists
                print OUT "USER_DBDFLAGS += -I$post/dbd\n";
            }
        }
    }
    close IN;
  }
}
close OUT;

