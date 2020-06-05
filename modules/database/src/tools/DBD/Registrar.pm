######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

package DBD::Registrar;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

sub init {
    return shift->SUPER::init(shift, "registrar function");
}

1;
