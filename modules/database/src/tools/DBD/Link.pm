######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

package DBD::Link;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

sub init {
    my ($this, $name, $jlif) = @_;
    $this->SUPER::init($jlif, "link support (jlif)");
    $this->{KEY} = $name;
    return $this;
}

sub key {
    return shift->{KEY};
}

sub equals {
    my ($a, $b) = @_;
    return $a->SUPER::equals($b)
        && $a->{KEY} eq $b->{KEY};
}

1;
