package DBD::Parser;

use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(&ParseDBD);

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

our $debug=0;

sub ParseDBD {
    (my $dbd, $_) = @_;
    while (1) {
        parseCommon($dbd);
        if (m/\G menu \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Menu: $1\n" if $debug;
            my ($menu_name) = unquote($1);
            parse_menu($dbd, $menu_name);
        }
        elsif (m/\G driver \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Driver: $1\n" if $debug;
            my ($driver_name) = unquote($1);
            $dbd->add(DBD::Driver->new($driver_name));
        }
        elsif (m/\G link \s* \( \s* $RXstr \s*, \s* $RXstr \s* \)/oxgc) {
            print "Link $1, $2\n" if $debug;
            my ($key, $lset) = unquote($1, $2);
            $dbd->add(DBD::Link->new($key, $lset));
        }
        elsif (m/\G registrar \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Registrar: $1\n" if $debug;
            my ($registrar_name) = unquote($1);
            $dbd->add(DBD::Registrar->new($registrar_name));
        }
        elsif (m/\G function \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Function: $1\n" if $debug;
            my ($function_name) = unquote($1);
            $dbd->add(DBD::Function->new($function_name));
        }
        elsif (m/\G breaktable \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Breaktable: $1\n" if $debug;
            my ($breaktable_name) = unquote($1);
            parse_breaktable($dbd, $breaktable_name);
        }
        elsif (m/\G recordtype \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Recordtype: $1\n" if $debug;
            my ($recordtype_name) = unquote($1);
            parse_recordtype($dbd, $recordtype_name);
        }
        elsif (m/\G g?record \s* \( \s* $RXstr \s*, \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Record: $1, $2\n" if $debug;
            my ($record_type, $record_name) = unquote($1, $2);
            parse_record($dbd, $record_type, $record_name);
        }
        elsif (m/\G alias \s* \( \s* $RXstr \s*, \s* $RXstr \s* \)/oxgc) {
            print "Alias: $1, $2\n" if $debug;
            my ($record_name, $alias) = unquote($1, $2);
            my $rec = $dbd->record($record_name);
            dieContext("Alias '$alias' refers to unknown record '$record_name'")
                unless defined $rec;
            dieContext("Can't create alias '$alias', name already used")
                if defined $dbd->record($alias);
            $rec->add_alias($alias);
            $dbd->add($rec, $alias);
        }
        elsif (m/\G variable \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Variable: $1\n" if $debug;
            my ($variable_name) = unquote($1);
            $dbd->add(DBD::Variable->new($variable_name));
        }
        elsif (m/\G variable \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print "Variable: $1, $2\n" if $debug;
            my ($variable_name, $variable_type) = unquote($1, $2);
            $dbd->add(DBD::Variable->new($variable_name, $variable_type));
        }
        elsif (m/\G device \s* \( \s* $RXstr \s* , \s* $RXstr \s* ,
                          \s* $RXstr \s* , \s*$RXstr \s* \)/oxgc) {
            print "Device: $1, $2, $3, $4\n" if $debug;
            my ($record_type, $link_type, $dset, $choice) =
                unquote($1, $2, $3, $4);
            my $rtyp = $dbd->recordtype($record_type);
            if (!defined $rtyp) {
                $rtyp = DBD::Recordtype->new($record_type);
                warn "Device using undefined record type '$record_type', place-holder created\n";
                $dbd->add($rtyp);
            }
            $rtyp->add_device(DBD::Device->new($link_type, $dset, $choice));
        } else {
            last unless m/\G (.*) $/moxgc;
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parseCommon {
    my ($obj) = @_;
    while (1) {
        # Skip leading whitespace
        m/\G \s* /oxgc;

        # Extract POD
        if (m/\G ( = [a-zA-Z] .* ) \n/oxgc) {
            $obj->add_pod($1, parsePod());
        }
        elsif (m/\G \# /oxgc) {
            if (m/\G \# ! BEGIN \{ ( [^}]* ) \} ! \# \# \n/oxgc) {
                print "File-Begin: $1\n" if $debug;
                pushContext("file '$1'");
            }
            elsif (m/\G \# ! END \{ ( [^}]* ) \} ! \# \# \n?/oxgc) {
                print "File-End: $1\n" if $debug;
                popContext("file '$1'");
            }
            else {
                m/\G (.*) \n/oxgc;
                $obj->add_comment($1);
                print "Comment: $1\n" if $debug;
            }
        } else {
            return;
        }
    }
}

sub unquote {
    return map { m/^ ("?) (.*) \1 $/ox; $2 } @_;
}

sub parsePod {
    pushContext("Pod markup");
    my @pod;
    while (1) {
        if (m/\G ( =cut .* ) \n?/oxgc) {
            popContext("Pod markup");
            return @pod;
        }
        elsif (m/\G ( .* ) $/oxgc) {
            dieContext("Unexpected end of input file, Pod block not closed");
        }
        elsif (m/\G ( .* ) \n/oxgc) {
            push @pod, $1
        }
    }
}

sub parse_menu {
    my ($dbd, $menu_name) = @_;
    pushContext("menu($menu_name)");
    my $menu = DBD::Menu->new($menu_name);
    while(1) {
        parseCommon($menu);
        if (m/\G choice \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Menu-Choice: $1, $2\n" if $debug;
            my ($choice_name, $value) = unquote($1, $2);
            $menu->add_choice($choice_name, $value);
        }
        elsif (m/\G \}/oxgc) {
            print " Menu-End:\n" if $debug;
            $dbd->add($menu);
            popContext("menu($menu_name)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_breaktable {
    my ($dbd, $breaktable_name) = @_;
    pushContext("breaktable($breaktable_name)");
    my $bt = DBD::Breaktable->new($breaktable_name);
    while(1) {
        parseCommon($bt);
        if (m/\G point\s* \(\s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Breaktable-Point: $1, $2\n" if $debug;
            my ($raw, $eng) = unquote($1, $2);
            $bt->add_point($raw, $eng);
        }
        elsif (m/\G $RXstr \s* (?: , \s*)? $RXstr (?: \s* ,)?/oxgc) {
            print " Breaktable-Data: $1, $2\n" if $debug;
            my ($raw, $eng) = unquote($1, $2);
            $bt->add_point($raw, $eng);
        }
        elsif (m/\G \}/oxgc) {
            print " Breaktable-End:\n" if $debug;
            $dbd->add($bt);
            popContext("breaktable($breaktable_name)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_recordtype {
    my ($dbd, $record_type) = @_;
    pushContext("recordtype($record_type)");
    my $rtyp = DBD::Recordtype->new($record_type);
    while(1) {
        parseCommon($rtyp);
        if (m/\G field \s* \( \s* $RXstr \s* , \s* $RXstr \s* \) \s* \{/oxgc) {
            print " Recordtype-Field: $1, $2\n" if $debug;
            my ($field_name, $field_type) = unquote($1, $2);
            parse_field($rtyp, $field_name, $field_type);
        }
        elsif (m/\G % (.*) \n/oxgc) {
            print " Recordtype-Cdef: $1\n" if $debug;
            $rtyp->add_cdef($1);
        }
        elsif (m/\G \}/oxgc) {
            print " Recordtype-End:\n" if $debug;
            $dbd->add($rtyp);
            popContext("recordtype($record_type)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_record {
    my ($dbd, $record_type, $record_name) = @_;
    pushContext("record($record_type, $record_name)");
    my $rtyp = $dbd->recordtype($record_type);
    my $rec = $dbd->record($record_name);
    if (defined $rec) {
        my $otyp = $rec->recordtype;
        my $otyp_name = $otyp->name;
        $rtyp = $otyp if $record_type eq '*';
        dieContext("A(n) $otyp_name record '$record_name' already exists")
            unless $otyp == $rtyp;
    } else {
        dieContext("No record exists named '$record_name'")
            if $record_type eq '*';
        dieContext("No recordtype exists named '$record_type'")
            unless defined $rtyp;
        $rec = DBD::Record->new($rtyp, $record_name);
    }
    while (1) {
        parseCommon($rec);
        if (m/\G field \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Record-Field: $1, $2\n" if $debug;
            my ($field_name, $value) = unquote($1, $2);
            $rec->put_field($field_name, $value);
        }
        elsif (m/\G info \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Record-Info: $1, $2\n" if $debug;
            my ($info_name, $value) = unquote($1, $2);
            $rec->add_info($info_name, $value);
        }
        elsif (m/\G alias \s* \( \s* $RXstr \s* \)/oxgc) {
            print " Record-Alias: $1\n" if $debug;
            my ($alias) = unquote($1);
            dieContext("Can't create alias '$alias', name in use")
                if defined $dbd->record($1);
            $rec->add_alias($alias);
            $dbd->add($rec, $alias);
        }
        elsif (m/\G \}/oxgc) {
            print " Record-End:\n" if $debug;
            $dbd->add($rec);
            popContext("record($record_type, $record_name)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_field {
    my ($rtyp, $field_name, $field_type) = @_;
    my $fld = DBD::Recfield->new($field_name, $field_type);
    pushContext("field($field_name, $field_type)");
    while(1) {
        parseCommon($fld);
        if (m/\G (\w+) \s* \( \s* $RXstr \s* \)/oxgc) {
            print "  Field-Attribute: $1, $2\n" if $debug;
            my ($attr, $value) = unquote($1, $2);
            $fld->add_attribute($attr, $value);
        }
        elsif (m/\G \}/oxgc) {
            print "  Field-End:\n" if $debug;
            $rtyp->add_field($fld);
            popContext("field($field_name, $field_type)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

1;
