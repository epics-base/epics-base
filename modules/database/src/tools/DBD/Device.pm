package DBD::Device;
use DBD::Base;
our @ISA = qw(DBD::Base);

use strict;

my %link_types = (
    CONSTANT  => qr/$RXnum/,
    PV_LINK   => qr/$RXname \s+ [.NPCAMS ]*/x,
    JSON_LINK => qr/\{ .* \}/x,
    VME_IO    => qr/\# (?: \s* [CS] \s* $RXintx)* \s* (?: @ .*)?/x,
    CAMAC_IO  => qr/\# (?: \s* [BCNAF] \s* $RXintx)* \s* (?: @ .*)?/x,
    RF_IO     => qr/\# (?: \s* [RMDE] \s* $RXintx)*/x,
    AB_IO     => qr/\# (?: \s* [LACS] \s* $RXintx)* \s* (?: @ .*)?/x,
    GPIB_IO   => qr/\# (?: \s* [LA] \s* $RXintx)* \s* (?: @ .*)?/x,
    BITBUS_IO => qr/\# (?: \s* [LNPS] \s* $RXuintx)* \s* (?: @ .*)?/x,
    BBGPIB_IO => qr/\# (?: \s* [LBG] \s* $RXuintx)* \s* (?: @ .*)?/x,
    VXI_IO    => qr/\# (?: \s* [VCS] \s* $RXintx)* \s* (?: @ .*)?/x,
    INST_IO   => qr/@.*/
);

sub init {
    my ($this, $link_type, $dset, $choice) = @_;
    dieContext("Unknown link type '$link_type', valid types are:",
        sort keys %link_types) unless exists $link_types{$link_type};
    $this->SUPER::init($dset, "device support (dset)");
    $this->{LINK_TYPE} = $link_type;
    $this->{CHOICE} = $choice;
    return $this;
}

sub link_type {
    return shift->{LINK_TYPE};
}

sub choice {
    return shift->{CHOICE};
}

sub legal_addr {
    my ($this, $addr) = @_;
    my $rx = $link_types{$this->{LINK_TYPE}};
    return $addr =~ m/^ $rx $/x;
}

sub equals {
    my ($a, $b) = @_;
    return $a->SUPER::equals($b)
        && $a->{LINK_TYPE} eq $b->{LINK_TYPE}
        && $a->{CHOICE}    eq $b->{CHOICE};
}

1;
