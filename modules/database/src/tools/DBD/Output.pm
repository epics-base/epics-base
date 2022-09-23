######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
######################################################################

package DBD::Output;

use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(&OutputDBD &OutputDB);

use DBD;
use DBD::Base;
use DBD::Breaktable;
use DBD::Device;
use DBD::Driver;
use DBD::Link;
use DBD::Menu;
use DBD::Recordtype;
use DBD::Recfield;
use DBD::Record;
use DBD::Registrar;
use DBD::Function;
use DBD::Variable;

sub OutputDBD {
    my ($out, $dbd) = @_;
    OutputMenus($out, $dbd->menus);
    OutputRecordtypes($out, $dbd->recordtypes);
    OutputDrivers($out, $dbd->drivers);
    OutputLinks($out, $dbd->links);
    OutputRegistrars($out, $dbd->registrars);
    OutputFunctions($out, $dbd->functions);
    OutputVariables($out, $dbd->variables);
    OutputBreaktables($out, $dbd->breaktables);
}

sub OutputDB {
    my ($out, $dbd) = @_;
    OutputRecords($out, $dbd);
}

sub OutputMenus {
    my ($out, $menus) = @_;
    foreach my $name (sort keys %{$menus}) {
        my $menu = $menus->{$name};
        printf $out "menu(%s) {\n", $name;
        printf $out "    choice(%s, \"%s\")\n", @{$_}
            foreach $menu->choices;
        print $out "}\n";
    }
}

sub OutputRecordtypes {
    my ($out, $recordtypes) = @_;
    foreach my $name (sort keys %{$recordtypes}) {
        my $recordtype = $recordtypes->{$name};
        printf $out "recordtype(%s) {\n", $name;
        print $out "    %$_\n"
            foreach $recordtype->cdefs;
        foreach my $field ($recordtype->fields) {
            printf $out "    field(%s, %s) {\n",
                $field->name, $field->dbf_type;
            while (my ($attr, $val) = each %{$field->attributes}) {
                $val = "\"$val\""
                    if $val !~ m/^$RXname$/x
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
        foreach sort keys %{$drivers};
}

sub OutputLinks {
    my ($out, $links) = @_;
    foreach my $name (sort keys %{$links}) {
        my $link = $links->{$name};
        printf $out "link(%s, %s)\n", $link->key, $name;
    }
}

sub OutputRegistrars {
    my ($out, $registrars) = @_;
    printf $out "registrar(%s)\n", $_
        foreach sort keys %{$registrars};
}

sub OutputFunctions {
    my ($out, $functions) = @_;
    printf $out "function(%s)\n", $_
        foreach sort keys %{$functions};
}

sub OutputVariables {
    my ($out, $variables) = @_;
    foreach my $name (sort keys %{$variables}) {
        my $variable = $variables->{$name};
        printf $out "variable(%s, %s)\n", $name, $variable->var_type;
    }
}

sub OutputBreaktables {
    my ($out, $breaktables) = @_;
    foreach my $name (sort keys %{$breaktables}) {
        my $breaktable = $breaktables->{$name};
        printf $out "breaktable(\"%s\") {\n", $name;
        printf $out "    %s, %s\n", @{$_}
            foreach $breaktable->points;
        print $out "}\n";
    }
}

sub OutputRecords {
    my ($out, $dbd) = @_;
    foreach my $name ($dbd->record_names) {
        my $rec = $dbd->record($name);
        die "No record '$name'"
            unless $rec && $rec->isa('DBD::Record');
        printf $out "record(%s, \"%s\") {\n", $rec->recordtype->name, $name;
        printf $out "    alias(\"%s\")\n", $_
            foreach $rec->aliases;
        foreach my $recfield ($rec->recfields) {
            my $field_name = $recfield->name;
            my $value = $rec->get_field($field_name);
            printf $out "    field(%s, \"%s\")\n", $field_name, $value
                if defined $value;
        }
        printf $out "    info(\"%s\", \"%s\")\n", $_, $rec->info_value($_)
            foreach $rec->info_names;
        print $out "}\n";
    }
}

1;
