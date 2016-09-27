package DBD::Output;

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(&OutputDBD);

use DBD;
use DBD::Base;
use DBD::Breaktable;
use DBD::Device;
use DBD::Driver;
use DBD::Menu;
use DBD::Recordtype;
use DBD::Recfield;
use DBD::Registrar;
use DBD::Function;
use DBD::Variable;

sub OutputDBD {
    my ($out, $dbd) = @_;
    &OutputMenus($out, $dbd->menus);
    &OutputRecordtypes($out, $dbd->recordtypes);
    &OutputDrivers($out, $dbd->drivers);
    &OutputRegistrars($out, $dbd->registrars);
    &OutputFunctions($out, $dbd->functions);
    &OutputVariables($out, $dbd->variables);
    &OutputBreaktables($out, $dbd->breaktables);
}

sub OutputMenus {
    my ($out, $menus) = @_;
    while (my ($name, $menu) = each %{$menus}) {
        printf $out "menu(%s) {\n", $name;
        printf $out "    choice(%s, \"%s\")\n", @{$_}
            foreach $menu->choices;
        print $out "}\n";
    }
}

sub OutputRecordtypes {
    my ($out, $recordtypes) = @_;
    while (my ($name, $recordtype) = each %{$recordtypes}) {
        printf $out "recordtype(%s) {\n", $name;
        print $out "    %$_\n"
            foreach $recordtype->cdefs;
        foreach $field ($recordtype->fields) {
            printf $out "    field(%s, %s) {\n",
                $field->name, $field->dbf_type;
            while (my ($attr, $val) = each %{$field->attributes}) {
                $val = "\"$val\""
                    if $val !~ m/^$RXname$/ox
                       || $attr eq 'prompt'
                       || $attr eq 'initial';
                printf $out "        %s(%s)\n", $attr, $val;
            }
            print $out "    }\n";
        }
        printf $out "}\n";
        printf $out "device(%s, %s, %s, \"%s\")\n",
            $name, $_->link_type, $_->name, $_->choice
            foreach $recordtype->devices;
    }
}

sub OutputDrivers {
    my ($out, $drivers) = @_;
    printf $out "driver(%s)\n", $_
        foreach keys %{$drivers};
}

sub OutputRegistrars {
    my ($out, $registrars) = @_;
    printf $out "registrar(%s)\n", $_
        foreach keys %{$registrars};
}

sub OutputFunctions {
    my ($out, $functions) = @_;
    printf $out "function(%s)\n", $_
        foreach keys %{$functions};
}

sub OutputVariables {
    my ($out, $variables) = @_;
    while (my ($name, $variable) = each %{$variables}) {
        printf $out "variable(%s, %s)\n", $name, $variable->var_type;
    }
}

sub OutputBreaktables {
    my ($out, $breaktables) = @_;
    while (my ($name, $breaktable) = each %{$breaktables}) {
        printf $out "breaktable(\"%s\") {\n", $name;
        printf $out "    %s, %s\n", @{$_}
            foreach $breaktable->points;
        print $out "}\n";
    }
}

1;
