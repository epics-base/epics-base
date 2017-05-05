package DBD::Device;
use DBD::Base;
@ISA = qw(DBD::Base);

my %link_types = (
    CONSTANT  => qr/$RXnum/o,
    PV_LINK   => qr/$RXname \s+ [.NPCAMS ]*/ox,
    JSON_LINK => qr/\{ .* \}/ox,
    VME_IO    => qr/\# (?: \s* [CS] \s* $RXintx)* \s* (?: @ .*)?/ox,
    CAMAC_IO  => qr/\# (?: \s* [BCNAF] \s* $RXintx)* \s* (?: @ .*)?/ox,
    RF_IO     => qr/\# (?: \s* [RMDE] \s* $RXintx)*/ox,
    AB_IO     => qr/\# (?: \s* [LACS] \s* $RXintx)* \s* (?: @ .*)?/ox,
    GPIB_IO   => qr/\# (?: \s* [LA] \s* $RXintx)* \s* (?: @ .*)?/ox,
    BITBUS_IO => qr/\# (?: \s* [LNPS] \s* $RXuintx)* \s* (?: @ .*)?/ox,
    BBGPIB_IO => qr/\# (?: \s* [LBG] \s* $RXuintx)* \s* (?: @ .*)?/ox,
    VXI_IO    => qr/\# (?: \s* [VCS] \s* $RXintx)* \s* (?: @ .*)?/ox,
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
