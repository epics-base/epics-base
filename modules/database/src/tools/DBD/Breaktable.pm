######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

package DBD::Breaktable;
use DBD::Base;
our @ISA = qw(DBD::Base);

use Carp;
use strict;

my $warned;

sub init {
    my ($this, $name) = @_;
    $this->SUPER::init($name, "breakpoint table");
    $this->{POINT_LIST} = [];
    $this->{COMMENTS} = [];
    $this->{POD} = [];
    return $this;
}

# Override, breaktable names don't have to be strict
sub identifier {
    my ($this, $id, $what) = @_;
    confess "DBD::Breaktable::identifier: $what undefined!"
        unless defined $id;
    if ($id !~ m/^$RXname$/) {
        my @message;
        push @message, "A $what should contain only letters, digits and these",
            "special characters: _ - : . [ ] < > ;" unless $warned++;
        warnContext("Deprecated $what '$id'", @message);
    }
    return $id;
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
