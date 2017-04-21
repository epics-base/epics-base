# Common utility functions used by the DBD components

package DBD::Base;

use strict;
use warnings;

use Carp;
require Exporter;

our @ISA = qw(Exporter);

our @EXPORT = qw(&pushContext &popContext &dieContext &warnContext &is_reserved
    &escapeCcomment &escapeCstring $RXident $RXname $RXuint $RXint $RXhex $RXoct
    $RXuintx $RXintx $RXnum $RXdqs $RXstr);


our $RXident = qr/ [a-zA-Z] [a-zA-Z0-9_]* /x;
our $RXname =  qr/ [a-zA-Z0-9_\-:.\[\]<>;]+ /x;
our $RXhex =   qr/ (?: 0 [xX] [0-9A-Fa-f]+ ) /x;
our $RXoct =   qr/ 0 [0-7]* /x;
our $RXuint =  qr/ \d+ /x;
our $RXint =   qr/ -? $RXuint /ox;
our $RXuintx = qr/ ( $RXhex | $RXoct | $RXuint ) /ox;
our $RXintx =  qr/ ( $RXhex | $RXoct | $RXint ) /ox;
our $RXnum =   qr/ -? (?: \d+ | \d* \. \d+ ) (?: [eE] [-+]? \d+ )? /x;
our $RXdqs =   qr/ " (?: [^"] | \\" )* " /x;
our $RXstr =   qr/ ( $RXname | $RXnum | $RXdqs ) /ox;

our @context;


sub pushContext {
    my ($ctxt) = @_;
    unshift @context, $ctxt;
}

sub popContext {
    my ($ctxt) = @_;
    my $pop = shift @context;
    ($ctxt ne $pop) and
        dieContext("Leaving context \"$ctxt\", found \"$pop\" instead.",
            "\tBraces must be closed in the same file they open in.");
}

sub dieContext {
    my $msg = join "\n\t", @_;
    die "$msg\nContext: ", join(' in ', @context), "\n";
}

sub warnContext {
    my $msg = join "\n\t", @_;
    print STDERR "$msg\nContext: ", join(' in ', @context), "\n";
}


# Reserved words from C++ and the DB/DBD file parser
my %reserved = map { $_ => undef } qw(and and_eq asm auto bitand bitor bool
    break case catch char class compl const const_cast continue default delete
    do double dynamic_cast else enum explicit export extern false float for
    friend goto if inline int long mutable namespace new not not_eq operator or
    or_eq private protected public register reinterpret_cast return short signed
    sizeof static static_cast struct switch template this throw true try typedef
    typeid typename union unsigned using virtual void volatile wchar_t while xor
    xor_eq addpath alias breaktable choice device driver field function grecord
    include info menu path record recordtype registrar variable);
sub is_reserved {
    my $id = shift;
    return exists $reserved{$id};
}

sub identifier {
    my ($this, $id, $what) = @_;
    confess "DBD::Base::identifier: $what undefined!"
        unless defined $id;
    $id =~ m/^$RXident$/o or dieContext("Illegal $what '$id'",
        "Identifiers are used in C code so must start with a letter, followed",
        "by letters, digits and/or underscore characters only.");
    dieContext("Illegal $what '$id'",
        "Identifier is a C++ reserved word.")
        if is_reserved($id);
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


# Base methods for the DBD component objects

sub new {
    my $class = shift;
    my $this = {};
    bless $this, $class;
    return $this->init(@_);
}

sub init {
    my ($this, $name, $what) = @_;
    $this->{NAME} = $this->identifier($name, "$what name");
    $this->{WHAT} = $what;
    return $this;
}

sub name {
    return shift->{NAME};
}

sub what {
    return shift->{WHAT};
}

sub add_comment {
    my $this = shift;
    confess "add_comment() not supported by $this->{WHAT} ($this)\n",
        "Context: ", join(' in ', @context), "\n";
}

sub add_pod {
    my $this = shift;
    warnContext "Warning: Pod text inside $this->{WHAT} will be ignored";
}

sub equals {
    my ($a, $b) = @_;
    return $a->{NAME} eq $b->{NAME}
        && $a->{WHAT} eq $b->{WHAT};
}

1;
