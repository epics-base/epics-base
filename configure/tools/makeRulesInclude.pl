# $Id$ 

eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeConfigAppInclude.pl

use Cwd;

$hostarch = $ARGV[0];
$arch = $ARGV[1];
$outfile = $ARGV[2];
$top = $ARGV[3];

unlink("${outfile}");
open(OUT,">${outfile}") or die "$! opening ${outfile}";
print OUT "#Do not modify this file.\n";
print OUT "#This file is created during the build.\n";

@files =();
push(@files,"$top/configure/RELEASE");
push(@files,"$top/configure/RELEASE.${hostarch}");
foreach $file (@files) {
  if (-r "$file") {
    open(IN, "$file") or die "Cannot open $file\n";
    while ($line = <IN>) {
        next if ( $line =~ /\s*#/ );
	chomp($line);
		$line =~ s/[ 	]//g; # remove blanks and tabs
        next if ( $line =~ /^$/ ); # skip empty lines
        $_ = $line;
        #the following looks for
        # prefix = $(macro)post
        ($prefix,$macro,$post) = /(.*)\s*=\s*\$\((.*)\)(.*)/;
        if ($macro eq "") { # true if no macro is present
            # the following looks for
            # prefix = post
            ($prefix,$post) = /(.*)\s*=\s*(.*)/;
            next if ( $prefix eq "EPICS_BASE" ); # skip epics base
            next if ( $prefix eq "TEMPLATE_TOP" ); # skip epics base
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
            if ( -d "$post/configure") { #check that directory exists
                print OUT "-include $post/configure/RULES_BUILD\n";
            }
        }
    }
    close IN;
  }
}
