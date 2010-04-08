package DBD::Breaktable;
use DBD::Base;
@ISA = qw(DBD::Base);

use Carp;

sub init {
	my ($this, $name) = @_;
	$this->SUPER::init($name, "breakpoint table name");
        $this->{POINT_LIST} = [];
        return $this;
}

sub add_point {
	my ($this, $raw, $eng) = @_;
	confess "Raw value undefined!" unless defined $raw;
	$raw = unquote($raw);
	confess "Engineering value undefined!" unless defined $eng;
	$eng = unquote($eng);
	push @{$this->{POINT_LIST}}, [$raw, $eng];
}

sub points {
	return @{shift->{POINT_LIST}};
}

sub point {
    my ($this, $idx) = @_;
    return $this->{POINT_LIST}[$idx];
}

1;
