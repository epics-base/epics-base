package DBD::Breaktable;
use DBD::Util;
@ISA = qw(DBD::Util);

use Carp;

sub init {
	my ($this, $name) = @_;
	$this->SUPER::init($name, "breakpoint table name");
        $this->{POINTS} = [];
        return $this;
}

sub add_point {
	my ($this, $raw, $eng) = @_;
	confess "Raw value undefined!" unless defined $raw;
	$raw = unquote($raw);
	confess "Engineering value undefined!" unless defined $eng;
	$eng = unquote($eng);
	push @{$this->{POINTS}}, [$raw, $eng];
}

sub points {
	return @{shift->{POINTS}};
}

sub point {
    my ($this, $idx) = @_;
    return $this->{POINTS}[$idx];
}

1;
