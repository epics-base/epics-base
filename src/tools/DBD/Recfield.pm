package DBD::Recfield;
use DBD::Base;
@ISA = qw(DBD::Base);

# The hash value is a regexp that matches all legal values of this field
# NB: The regexps are not currently used, and are wrong for some types.
our %field_types = (
    DBF_STRING   => qr/.{0,40}/,
    DBF_CHAR     => $RXintx,
    DBF_UCHAR    => $RXuintx,
    DBF_SHORT    => $RXintx,
    DBF_USHORT   => $RXuintx,
    DBF_LONG     => $RXintx,
    DBF_ULONG    => $RXuintx,
    DBF_INT64    => $RXintx,
    DBF_UINT64   => $RXuintx,
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
    asl         => qr/^ASL[01]$/,
    initial     => qr/^.*$/,
    promptgroup => qr/^.*$/,
    prompt      => qr/^.*$/,
    special     => qr/^(?:SPC_\w+|\d{3,})$/,
    pp          => qr/^(?:TRUE|FALSE)$/,
    interest    => qr/^\d+$/,
    base        => qr/^(?:DECIMAL|HEX)$/,
    size        => qr/^\d+$/,
    extra       => qr/^.*$/,
    menu        => qr/^$RXident$/o,
    prop        => qr/^(?:YES|NO)$/
);

# Convert old promptgroups into new-style
my %promptgroupMap = (
    GUI_COMMON   => '10 - Common',
    GUI_ALARMS   => '70 - Alarm',
    GUI_BITS1    => '41 - Bits (1)',
    GUI_BITS2    => '42 - Bits (2)',
    GUI_CALC     => '30 - Action',
    GUI_CLOCK    => '30 - Action',
    GUI_COMPRESS => '30 - Action',
    GUI_CONVERT  => '60 - Convert',
    GUI_DISPLAY  => '80 - Display',
    GUI_HIST     => '30 - Action',
    GUI_INPUTS   => '40 - Input',
    GUI_LINKS    => '40 - Link',
    GUI_MBB      => '30 - Action',
    GUI_MOTOR    => '30 - Action',
    GUI_OUTPUT   => '50 - Output',
    GUI_PID      => '30 - Action',
    GUI_PULSE    => '30 - Action',
    GUI_SELECT   => '40 - Input',
    GUI_SEQ1     => '51 - Output (1)',
    GUI_SEQ2     => '52 - Output (2)',
    GUI_SEQ3     => '53 - Output (3)',
    GUI_SUB      => '30 - Action',
    GUI_TIMER    => '30 - Action',
    GUI_WAVE     => '30 - Action',
    GUI_SCAN     => '20 - Scan',
);

sub new {
    my ($class, $name, $type) = @_;
    dieContext("Illegal field type '$type', valid field types are:",
        sort keys %field_types) unless exists $field_types{$type};
    my $this = {};
    bless $this, "${class}::${type}";
    return $this->init($name, $type);
}

sub init {
    my ($this, $name, $type) = @_;
    $this->SUPER::init($name, "record field");
    dieContext("Illegal field type '$type', valid field types are:",
        sort keys %field_types) unless exists $field_types{$type};
    $this->{DBF_TYPE} = $type;
    $this->{ATTR_INDEX} = {};
    $this->{COMMENTS} = [];
    return $this;
}

sub dbf_type {
    return shift->{DBF_TYPE};
}

sub set_number {
    my ($this, $number) = @_;
    $this->{NUMBER} = $number;
}

sub number {
    return shift->{NUMBER};
}

sub add_attribute {
    my ($this, $attr, $value) = @_;
    $value = $promptgroupMap{$value}
        if $attr eq 'promptgroup' && exists $promptgroupMap{$value};
    my $match = $field_attrs{$attr};
    if (defined $match) {
        dieContext("Bad value '$value' for field attribute '$attr'")
            unless $value =~ m/$match/;
    }
    else {
        warnContext("Unknown field attribute '$attr' with value '$value'; " .
            "known attributes are:",
            join(", ", sort keys %field_attrs));
    }
    $this->{ATTR_INDEX}->{$attr} = $value;
}

sub attributes {
    return shift->{ATTR_INDEX};
}

sub attribute {
    my ($this, $attr) = @_;
    return $this->attributes->{$attr};
}

sub equals {
    dieContext("Record field objects are not comparable");
}

sub check_valid {
    my ($this) = @_;
    my $name = $this->name;
    my $default = $this->attribute("initial");
    dieContext("Default value '$default' is invalid for field '$name'")
        if (defined($default) and !$this->legal_value($default));
}

sub add_comment {
    my $this = shift;
    push @{$this->{COMMENTS}}, @_;
}

sub comments {
    return @{shift->{COMMENTS}};
}


# The C structure member name is usually the field name converted to
# lower-case.  However if that is a reserved word, use the original.
sub C_name {
    my ($this) = @_;
    my $name = lc $this->name;
    $name = $this->name
        if is_reserved($name);
    return $name;
}

sub toDeclaration {
    my ($this, $ctype) = @_;
    my $name = $this->C_name;
    my $result = sprintf "    %-19s %-12s", $ctype, "$name;";
    my $prompt = $this->attribute('prompt');
    $result .= "/* $prompt */" if defined $prompt;
    return $result;
}


################################################################################

package DBD::Recfield::DBF_STRING;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    return (length $value < $this->attribute('size'));
    # NB - we use '<' to allow space for the terminating nil byte
}

sub check_valid {
    my ($this) = @_;
    dieContext("Size missing for DBF_STRING field '$name'")
        unless exists $this->attributes->{'size'};
    $this->SUPER::check_valid;
}

sub toDeclaration {
    my ($this) = @_;
    my $name = lc $this->name;
    my $size = $this->attribute('size');
    my $result = sprintf "    %-19s %-12s", 'char', "${name}[${size}];";
    my $prompt = $this->attribute('prompt');
    $result .= "/* $prompt */" if defined $prompt;
    return $result;
}


################################################################################

package DBD::Recfield::DBF_CHAR;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXint $/x and
            $value >= -128 and
            $value <= 127);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsInt8");
}


################################################################################

package DBD::Recfield::DBF_UCHAR;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXuint $/x and
            $value >= 0 and
            $value <= 255);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsUInt8");
}


################################################################################

package DBD::Recfield::DBF_SHORT;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXint $/x and
            $value >= -32768 and
            $value <= 32767);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsInt16");
}


################################################################################

package DBD::Recfield::DBF_USHORT;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXuint $/x and
            $value >= 0 and
            $value <= 65535);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsUInt16");
}


################################################################################

package DBD::Recfield::DBF_LONG;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXint $/x);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsInt32");
}


################################################################################

package DBD::Recfield::DBF_ULONG;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXuint $/x and
            $value >= 0);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsUInt32");
}


################################################################################

package DBD::Recfield::DBF_INT64;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXint $/x);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsInt64");
}


################################################################################

package DBD::Recfield::DBF_UINT64;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    $value =~ s/^ ( $RXhex | $RXoct ) $/ oct($1) /xe;
    return ($value =~ m/^ $RXuint $/x and
            $value >= 0);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsUInt64");
}


################################################################################

package DBD::Recfield::DBF_FLOAT;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    return ($value =~ m/^ $RXnum $/x);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsFloat32");
}


################################################################################

package DBD::Recfield::DBF_DOUBLE;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    return ($value =~ m/^ $RXnum $/x);
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsFloat64");
}


################################################################################

package DBD::Recfield::DBF_ENUM;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    return 1;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsEnum16");
}


################################################################################

package DBD::Recfield::DBF_MENU;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    # FIXME: If we know the menu name and the menu exists, check further
    return 1;
}

sub check_valid {
    my ($this) = @_;
    dieContext("Menu name missing for DBF_MENU field '$name'")
        unless defined($this->attribute("menu"));
    $this->SUPER::check_valid;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsEnum16");
}


################################################################################

package DBD::Recfield::DBF_DEVICE;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    return 1;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("epicsEnum16");
}


################################################################################

package DBD::Recfield::DBF_INLINK;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    return 1;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("DBLINK");
}


################################################################################

package DBD::Recfield::DBF_OUTLINK;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    return 1;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("DBLINK");
}


################################################################################

package DBD::Recfield::DBF_FWDLINK;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    return 1;
}

sub toDeclaration {
    return shift->SUPER::toDeclaration("DBLINK");
}


################################################################################

package DBD::Recfield::DBF_NOACCESS;

use DBD::Base;
@ISA = qw(DBD::Recfield);

sub legal_value {
    my ($this, $value) = @_;
    return ($value eq '');
}

sub check_valid {
    my ($this) = @_;
    dieContext("Type information missing for DBF_NOACCESS field '$name'")
        unless defined($this->attribute("extra"));
    $this->SUPER::check_valid;
}

sub toDeclaration {
    my ($this) = @_;
    my $extra = $this->attribute('extra');
    my $result = sprintf "    %-31s ", "$extra;";
    my $prompt = $this->attribute('prompt');
    $result .= "/* $prompt */" if defined $prompt;
    return $result;
}

1;
