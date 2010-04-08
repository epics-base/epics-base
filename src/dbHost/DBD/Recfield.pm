package DBD::Recfield;
use DBD::Base;
@ISA = qw(DBD::Base);

# The hash value is a regexp that matches all legal values of this field
our %field_types = (
    DBF_STRING   => qr/.{0,40}/,
    DBF_CHAR     => $RXint,
    DBF_UCHAR    => $RXuint,
    DBF_SHORT    => $RXint,
    DBF_USHORT   => $RXuint,
    DBF_LONG     => $RXint,
    DBF_ULONG    => $RXuint,
    DBF_FLOAT    => $RXnum,
    DBF_DOUBLE   => $RXnum,
    DBF_ENUM     => qr/.*/,
    DBF_MENU     => qr/.*/,
    DBF_DEVICE   => qr/.*/,
    DBF_INLINK   => qr/.*/,
    DBF_OUTLINK  => qr/.*/,
    DBF_FWDLINK  => qr/.*/,
    DBF_NOACCESS => qr//
);

# The hash value is a regexp that matches all legal values of this attribute
our %field_attrs = (
    asl         => qr/ASL[01]/,
    initial     => qr/.*/,
    promptgroup => qr/GUI_\w+/,
    prompt      => qr/.*/,
    special     => qr/(?:SPC_\w+|\d{3,})/,
    pp          => qr/(?:YES|NO|TRUE|FALSE)/,
    interest    => qr/\d+/,
    base        => qr/(?:DECIMAL|HEX)/,
    size        => qr/\d+/,
    extra       => qr/.*/,
    menu        => qr/$RXident/o
);

sub init {
    my $this = shift;
    my $name = shift;
    my $type = unquote(shift);
    $this->SUPER::init($name, "record field name");
    exists $field_types{$type} or dieContext("Illegal field type '$type', ".
        "valid field types are:", sort keys %field_types);
    $this->{DBF_TYPE} = $type;
    $this->{ATTR_INDEX} = {};
    return $this;
}

sub dbf_type {
    return shift->{DBF_TYPE};
}

sub add_attribute {
    my $this = shift;
    my $attr = shift;
    my $value = unquote(shift);
    dieContext("Unknown field attribute '$1', valid attributes are:",
           sort keys %field_attrs)
        unless exists $field_attrs{$attr};
    dieContext("Bad value '$value' for field '$attr' attribute")
        unless $value =~ m/^ $field_attrs{$attr} $/x;
    $this->{ATTR_INDEX}->{$attr} = $value;
}

sub attributes {
    return shift->{ATTR_INDEX};
}

sub attribute {
    my ($this, $attr) = @_;
    return $this->attributes->{$attr};
}

sub legal_value {
    my ($this, $value) = @_;
    my $dbf_type = $this->dbf_type;
    return $value =~ m/^ $field_types{$dbf_type} $/x;
}

sub check_valid {
    # Internal validity checks of the field definition
    my $this = shift;
    my $name = $this->name;
    my $default = $this->attribute("initial");
    dieContext("Default value '$default' is invalid for field '$name'")
        if (defined($default) and !$this->legal_value($default));
    dieContext("Menu name not defined for field '$name'")
        if ($this->dbf_type eq "DBF_MENU"
            and !defined($this->attribute("menu")));
    # FIXME: Add more checks here?
}

1;
