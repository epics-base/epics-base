package Readfile;
require 5.000;
require Exporter;

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
    push @lines, <FILE>;
    push @lines, "##!END{$FILE}!##\n";
    close FILE or die "Error closing $FILE: $!\n";
    print " read ", scalar @lines, " lines\n" if $debug;
    return @lines;
}

sub macval {
    my ($macro, $Rmacros) = @_;
    if (exists $Rmacros->{$macro}) {
        return $Rmacros->{$macro};
    } else {
        warn "Warning: No value for macro \$($macro)\n";
	return undef;
    }
}

sub expandMacros {
    my ($Rmacros, @input) = @_;
    my @output;
    foreach (@input) {
	s/\$\((\w+)\)/&macval($1, $Rmacros)/eg unless /^\s*#/;
	push @output, $_;
    }
    return @output;
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
    my ($string) = @_;
    $string = $1 if $string =~ m/^"(.*)"$/o;
    return $string;
}

sub Readfile {
    my ($file, $Rmacros, $Rpath) = @_;
    print "Readfile($file)\n" if $debug;
    my @input = &expandMacros($Rmacros, &slurp($file, $Rpath));
    my @output;
    foreach (@input) {
	if (m/^ \s* include \s+ $string /ox) {
	    $arg = &unquote($1);
	    print " include $arg\n" if $debug;
	    push @output, "##! include \"$arg\"\n";
	    push @output, &Readfile($arg, $Rmacros, $Rpath);
	} elsif (m/^ \s* addpath \s+ $string /ox) {
	    $arg = &unquote($1);
	    print " addpath $arg\n" if $debug;
	    push @output, "##! addpath \"$arg\"\n";
	    push @{$Rpath}, &splitPath($arg);
	} elsif (m/^ \s* path \s+ $string /ox) {
	    $arg = &unquote($1);
	    print " path $arg\n" if $debug;
	    push @output, "##! path \"$arg\"\n";
	    @{$Rpath} = &splitPath($arg);
	} else {
	    push @output, $_;
	}
    }
    return @output;
}

1;
