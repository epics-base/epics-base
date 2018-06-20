package DBD::Breaktable;
use DBD::Base;
@ISA = qw(DBD::Base);

use Carp;

sub init {
    my ($this, $name) = @_;
    $this->SUPER::init($name, "breakpoint table");
    $this->{POINT_LIST} = [];
    $this->{COMMENTS} = [];
    $this->{POD} = [];
    return $this;
}

sub add_point {
    my ($this, $raw, $eng) = @_;
    confess "DBD::Breaktable::add_point: Raw value undefined!"
        unless defined $raw;
    confess "DBD::Breaktable::add_point: Engineering value undefined!"
        unless defined $eng;
    push @{$this->{POINT_LIST}}, [$raw, $eng];
}

sub points {
    return @{shift->{POINT_LIST}};
}

sub point {
    my ($this, $idx) = @_;
    return $this->{POINT_LIST}[$idx];
}

sub add_comment {
    my $this = shift;
    push @{$this->{COMMENTS}}, @_;
}

sub comments {
    return @{shift->{COMMENTS}};
}

sub add_pod {
    my $this = shift;
    push @{$this->{POD}}, @_;
}

sub pod {
    return @{shift->{POD}};
}

sub equals {
    my ($a, $b) = @_;
    return $a->SUPER::equals($b)
        && join(',', map "$_->[0]:$_->[1]", @{$a->{POINT_LIST}})
        eq join(',', map "$_->[0]:$_->[1]", @{$b->{POINT_LIST}});
}

1;
