package DBD::Menu;
use DBD::Base;
@ISA = qw(DBD::Base);

sub init {
    my ($this, $name) = @_;
    $this->SUPER::init($name, "menu name");
    $this->{CHOICE_LIST} = [];
    $this->{CHOICE_INDEX} = {};
    return $this;
}

sub add_choice {
    my ($this, $name, $value) = @_;
    $name = identifier($name, "Choice name");
    unquote $value;
    foreach $pair ($this->choices) {
    	dieContext("Duplicate choice name") if ($pair->[0] eq $name);
    	dieContext("Duplicate choice string") if ($pair->[1] eq $value);
    }
    push @{$this->{CHOICE_LIST}}, [$name, $value];
    $this->{CHOICE_INDEX}->{$value} = $name;
}

sub choices {
    return @{shift->{CHOICE_LIST}};
}

sub choice {
    my ($this, $idx) = @_;
    return $this->{CHOICE_LIST}[$idx];
}

sub legal_choice {
    my ($this, $value) = @_;
    unquote $value;
    return exists $this->{CHOICE_INDEX}->{$value};
}

sub toDeclaration {
    my $this = shift;
    my @choices = map {
        "\t" . @{$_}[0] . "\t/* " . escapeCcomment(@{$_}[1]) . " */"
    } $this->choices;
    return "typedef enum {\n" .
               join(",\n", @choices) . 
           "\n} " . $this->name . ";\n";
}

1;
