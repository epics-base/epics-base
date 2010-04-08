# Common utility functions used by the DBD components

package DBD::Base;

use Carp;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(&pushContext &popContext &dieContext &warnContext
    &identifier &unquote &escapeCcomment &escapeCstring
    $RXident $RXname $RXuint $RXint $RXdex $RXnum $RXdqs $RXsqs $RXstr);


our $RXident = qr/[a-zA-Z][a-zA-Z0-9_]*/;
our $RXname = qr/[a-zA-Z0-9_\-:.<>;]+/;
our $RXhex = qr/ (?: 0 [xX] [0-9A-Fa-f]+ ) /x;
our $RXuint = qr/ \d+ /x;
our $RXint = qr/ -? $RXuint /ox;
our $RXdex = qr/ ( $RXhex | $RXuint ) /x;
our $RXnum = qr/-? (?: \d+ ) | (?: \d* \. \d+ ) (?: [eE] [-+]? \d+ )?/x;
our $RXdqs = qr/" (?: [^"] | \\" )* "/x;
our $RXsqs = qr/' (?: [^'] | \\' )* '/x;
our $RXstr = qr/ ( $RXname | $RXnum | $RXdqs | $RXsqs ) /ox;

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

sub unquote {
    my ($string) = @_;
    $string =~ m/^"(.*)"$/o and $string = $1;
    return $string;
}

sub identifier {
    my $id = unquote(shift);
    my $what = shift;
    confess "$what undefined!" unless defined $id;
    $id =~ m/^$RXident$/o or dieContext("Illegal $what '$id'",
        "Identifiers are used in C code so must start with a letter, followed",
        "by letters, digits and/or underscore characters only.");
    return $id;
}


# Output filtering

sub escapeCcomment {
    $_ = shift;
    s/\*\//**/;
    return $_;
}

sub escapeCstring {
}


# Base class routines for the DBD component objects

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
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
