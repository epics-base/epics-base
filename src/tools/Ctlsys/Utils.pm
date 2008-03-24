# Parse a configure/RELEASE file.
#
# This code is from base/configure/tools/convertRelease.pl

sub readRelease {
    my ($file, $Rmacros, $Rapps) = @_;
    # $Rmacros is a reference to a hash, $Rapps a ref to an array
    
    local *IN;
    open(IN, $file) or die "Can't open $file: $!\n";
    while (<IN>) {
	chomp;
	s/\r$//;		# Shouldn't need this, but sometimes...
	s/\s*#.*$//;		# Remove trailing comments
	next if m/^\s*$/;	# Skip blank lines
	
	# Expand all already-defined macros in the line:
	while (my ($pre,$var,$post) = m/(.*)\$\((\w+)\)(.*)/) {
	    last unless exists $Rmacros->{$var};
	    $_ = $pre . $Rmacros->{$var} . $post;
	}
	
	# Handle "<macro> = <path>"
	my ($macro, $path) = m/^\s*(\w+)\s*=\s*(.*)/;
	if ($macro ne "") {
	    $Rmacros->{$macro} = $path;
	    push @$Rapps, $macro;
	    next;
	}
	# Handle "include <path>" syntax
	($path) = m/^\s*include\s+(.*)/;
	&readRelease($path, $Rmacros, $Rapps) if (-r $path);
    }
    close IN;
}

sub expandRelease {
    my ($Rmacros) = @_;
    # $Rmacros is a reference to a hash
    
    # Expand any (possibly nested) macros that were defined after use
    while (my ($macro, $path) = each %$Rmacros) {
	while (my ($pre,$var,$post) = $path =~ m/(.*)\$\((\w+?)\)(.*)/) {
	    $path = $pre . $Rmacros->{$var} . $post;
	    $Rmacros->{$macro} = $path;
	}
    }
}


# Copy directories and files from the template

sub copyTree {
    my ($src, $dst, $Rnamesubs, $Rtextsubs) = @_;
    # $Rnamesubs contains substitutions for file names,
    # $Rtextsubs contains substitutions for file content.
    
    opendir FILES, $src or die "opendir failed while copying $src: $!\n";
    my @entries = readdir FILES;
    closedir FILES;
    
    foreach (@entries) {
	next if m/^\.\.?$/;  # ignore . and ..
	next if m/^CVS$/;   # Shouldn't exist, but...
	
	my $srcName = "$src/$_";
	
	# Substitute any _VARS_ in the name
	s/@(\w+?)@/$Rnamesubs->{$1} || "@$1@"/eg;
	my $dstName = "$dst/$_";
	
	if (-d $srcName) {
	    print ":" unless $opt_d;
	    copyDir($srcName, $dstName, $Rnamesubs, $Rtextsubs);
	} elsif (-f $srcName) {
	    print "." unless $opt_d;
	    copyFile($srcName, $dstName, $Rtextsubs);
	} elsif (-l $srcName) {
	    warn "\nSoft link in template, ignored:\n\t$srcName\n";
	} else {
	    warn "\nUnknown file type in template, ignored:\n\t$srcName\n";
	}
    }
}

sub copyFile {
    my ($src, $dst, $Rtextsubs) = @_;
    return if (-e $dst);
    print "Creating file '$dst'\n" if $opt_d;
    open(SRC, "<$src") and open(DST, ">$dst") or die "$! copying $src to $dst\n";
    while (<SRC>) {
	# Substitute any _VARS_ in the text
	s/@(\w+?)@/$Rtextsubs->{$1} || "@$1@"/eg;
	print DST;
    }
    close DST;
    close SRC;
}

sub copyDir {
    my ($src, $dst, $Rnamesubs, $Rtextsubs) = @_;
    if (-e $dst && ! -d $dst) {
	warn "\nTarget exists but is not a directory, skipping:\n\t$dst\n";
	return;
    }
    print "Creating directory '$dst'\n" if $opt_d;
    mkdir $dst, 0777 or die "Can't create $dst: $!\n"
	unless -d $dst;
    copyTree($src, $dst, $Rnamesubs, $Rtextsubs);
}

1;
