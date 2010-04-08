# Common utility functions used by the DBD components

package DBD::Base;

use Carp;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(&pushContext &popContext &dieContext &warnContext &identifier
    &unquote &escapeCcomment &escapeCstring $RXident $RXname $RXuint $RXint
    $RXhex $RXoct $RXuintx $RXintx $RXnum $RXdqs $RXsqs $RXstr);


our $RXident = qr/ [a-zA-Z] [a-zA-Z0-9_]* /x;
our $RXname =  qr/ [a-zA-Z0-9_\-:.<>;]+ /x;
our $RXhex =   qr/ (?: 0 [xX] [0-9A-Fa-f]+ ) /x;
our $RXoct =   qr/ 0 [0-7]* /x;
our $RXuint =  qr/ \d+ /x;
our $RXint =   qr/ -? $RXuint /ox;
our $RXuintx = qr/ ( $RXhex | $RXoct | $RXuint ) /ox;
our $RXintx =  qr/ ( $RXhex | $RXoct | $RXint ) /ox;
our $RXnum =   qr/ -? (?: \d+ | \d* \. \d+ ) (?: [eE] [-+]? \d+ )? /x;
our $RXdqs =   qr/" (?: [^"] | \\" )* " /x;
our $RXsqs =   qr/' (?: [^'] | \\' )* ' /x;
our $RXstr =   qr/ ( $RXname | $RXnum | $RXdqs | $RXsqs ) /ox;

our @context;


sub pushContext {
    my ($ctxt) = @_;
    unshift @context, $ctxt;
}

sub popContext {
    my ($ctxt) = @_;
    my ($pop) = shift @context;
    ($ctxt ne $pop) and
        dieContext("Exiting context \"$ctxt\", found \"$pop\" instead.",
            "\tBraces must close in the same file they were opened.");
}

sub dieContext {
    my ($msg) = join "\n\t", @_;
    print "$msg\n" if $msg;
    die "Context: ", join(' in ', @context), "\n";
}

sub warnContext {
    my ($msg) = join "\n\t", @_;
    print "$msg\n" if $msg;
    print "Context: ", join(' in ', @context), "\n";
}


# Input checking

sub unquote (\$) {
    my ($s) = @_;
    $$s =~ s/^"(.*)"$/$1/o;
    return $$s;
}

sub identifier {
    my ($id, $what) = @_;
    unquote $id;
    confess "$what undefined!" unless defined $id;
    $id =~ m/^$RXident$/o or dieContext("Illegal $what '$id'",
        "Identifiers are used in C code so must start with a letter, followed",
        "by letters, digits and/or underscore characters only.");
    return $id;
}


# Output filtering

sub escapeCcomment {
    ($_) = @_;
    s/\*\//**/g;
    return $_;
}

sub escapeCstring {
    ($_) = @_;
    # How to do this?
    return $_;
}


# Base class routines for the DBD component objects

sub new {
    my $class = shift;
    my $this = {};
    bless $this, $class;
    return $this->init(@_);
}

sub init {
    my ($this, $name, $what) = @_;
    $this->{NAME} = identifier($name, $what);
    return $this;
}

sub name {
    return shift->{NAME};
}

1;
