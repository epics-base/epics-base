#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

use Carp;

# Copy directories and files from a template

sub copyTree {
    my ($src, $dst, $Rnamesubs, $Rtextsubs) = @_;
    # $Rnamesubs contains substitutions for file names,
    # $Rtextsubs contains substitutions for file content.

    opendir my $FILES, $src
        or croak "opendir failed while copying $src: $!\n";
    my @entries = readdir $FILES;
    closedir $FILES;

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
            carp "\nSoft link in template, ignored:\n\t$srcName\n";
        } else {
            carp "\nUnknown file type in template, ignored:\n\t$srcName\n";
        }
    }
}

sub copyDir {
    my ($src, $dst, $Rnamesubs, $Rtextsubs) = @_;
    if (-e $dst && ! -d $dst) {
        carp "\nTarget exists but is not a directory, skipping:\n\t$dst\n";
        return;
    }
    print "Creating directory '$dst'\n" if $opt_d;
    mkdir $dst, 0777 or croak "Can't create $dst: $!\n"
        unless -d $dst;
    copyTree($src, $dst, $Rnamesubs, $Rtextsubs);
}

sub copyFile {
    my ($src, $dst, $Rtextsubs) = @_;
    return if (-e $dst);
    print "Creating file '$dst'\n" if $opt_d;
    open(my $SRC, '<', $src)
        and open(my $DST, '>', $dst)
        or croak "$! copying $src to $dst\n";
    while (<$SRC>) {
        # Substitute any @VARS@ in the text
        s{@(\w+?)@}
         {exists $Rtextsubs->{$1} ? $Rtextsubs->{$1} : "\@$1\@"}eg;
        print $DST $_;
    }
    close $DST;
    close $SRC;
}

1;
