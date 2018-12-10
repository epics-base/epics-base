package DBD::Parser;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(&ParseDBD);

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

our $debug=0;

sub ParseDBD {
    (my $dbd, $_) = @_;
    while (1) {
        parseCommon($dbd);
        if (m/\G menu \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Menu: $1\n" if $debug;
            parse_menu($dbd, $1);
        }
        elsif (m/\G driver \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Driver: $1\n" if $debug;
            $dbd->add(DBD::Driver->new($1));
        }
        elsif (m/\G registrar \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Registrar: $1\n" if $debug;
            $dbd->add(DBD::Registrar->new($1));
        }
        elsif (m/\G function \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Function: $1\n" if $debug;
            $dbd->add(DBD::Function->new($1));
        }
        elsif (m/\G breaktable \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Breaktable: $1\n" if $debug;
            parse_breaktable($dbd, $1);
        }
        elsif (m/\G recordtype \s* \( \s* $RXstr \s* \) \s* \{/oxgc) {
            print "Recordtype: $1\n" if $debug;
            parse_recordtype($dbd, $1);
        }
        elsif (m/\G variable \s* \( \s* $RXstr \s* \)/oxgc) {
            print "Variable: $1\n" if $debug;
            $dbd->add(DBD::Variable->new($1));
        }
        elsif (m/\G variable \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print "Variable: $1, $2\n" if $debug;
            $dbd->add(DBD::Variable->new($1, $2));
        }
        elsif (m/\G device \s* \( \s* $RXstr \s* , \s* $RXstr \s* ,
                          \s* $RXstr \s* , \s*$RXstr \s* \)/oxgc) {
            print "Device: $1, $2, $3, $4\n" if $debug;
            my $rtyp = $dbd->recordtype($1);
            if (!defined $rtyp) {
                $rtyp = DBD::Recordtype->new($1);
                warn "Device using unknown record type '$1', declaration created\n";
                $dbd->add($rtyp);
            }
            $rtyp->add_device(DBD::Device->new($2, $3, $4));
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
        if (m/\G ( = [a-zA-Z] )/xgc) {
	    # The above regex was split from the one below for performance.
	    # Using m/\G ( = [a-zA-Z] .* ) \n/ is slow in Perl 5.20 and later.
	    my $directive = $1;
	    m/\G ( .* ) \n/xgc;
	    $directive .= $1;
            $obj->add_pod($directive, &parsePod);
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
    my ($dbd, $name) = @_;
    pushContext("menu($name)");
    my $menu = DBD::Menu->new($name);
    while(1) {
        parseCommon($menu);
        if (m/\G choice \s* \( \s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Menu-Choice: $1, $2\n" if $debug;
            $menu->add_choice($1, $2);
        }
        elsif (m/\G \}/oxgc) {
            print " Menu-End:\n" if $debug;
            $dbd->add($menu);
            popContext("menu($name)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_breaktable {
    my ($dbd, $name) = @_;
    pushContext("breaktable($name)");
    my $bt = DBD::Breaktable->new($name);
    while(1) {
        parseCommon($bt);
        if (m/\G point\s* \(\s* $RXstr \s* , \s* $RXstr \s* \)/oxgc) {
            print " Breaktable-Point: $1, $2\n" if $debug;
            $bt->add_point($1, $2);
        }
        elsif (m/\G $RXstr \s* (?: , \s*)? $RXstr (?: \s* ,)?/oxgc) {
            print " Breaktable-Data: $1, $2\n" if $debug;
            $bt->add_point($1, $2);
        }
        elsif (m/\G \}/oxgc) {
            print " Breaktable-End:\n" if $debug;
            $dbd->add($bt);
            popContext("breaktable($name)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_recordtype {
    my ($dbd, $name) = @_;
    pushContext("recordtype($name)");
    # Re-use a matching declaration record type if one exists
    my $rtyp = $dbd->recordtype($name);
    if (!defined($rtyp) || $rtyp->fields) {
        # Earlier record type is not a declaration, don't re-use it
        $rtyp = DBD::Recordtype->new($name);
    }
    while(1) {
        parseCommon($rtyp);
        if (m/\G field \s* \( \s* $RXstr \s* , \s* $RXstr \s* \) \s* \{/oxgc) {
            print " Recordtype-Field: $1, $2\n" if $debug;
            parse_field($rtyp, $1, $2);
        }
        elsif (m/\G \}/oxgc) {
            print " Recordtype-End:\n" if $debug;
            $dbd->add($rtyp);
            popContext("recordtype($name)");
            return;
        }
        elsif (m/\G % (.*) \n/oxgc) {
            print " Recordtype-Cdef: $1\n" if $debug;
            $rtyp->add_cdef($1);
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

sub parse_field {
    my ($rtyp, $name, $field_type) = @_;
    my $fld = DBD::Recfield->new($name, $field_type);
    pushContext("field($name, $field_type)");
    while(1) {
        parseCommon($fld);
        if (m/\G (\w+) \s* \( \s* $RXstr \s* \)/oxgc) {
            print "  Field-Attribute: $1, $2\n" if $debug;
            $fld->add_attribute($1, $2);
        }
        elsif (m/\G \}/oxgc) {
            print "  Field-End:\n" if $debug;
            $rtyp->add_field($fld);
            popContext("field($name, $field_type)");
            return;
        } else {
            m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
            dieContext("Syntax error in '$1'");
        }
    }
}

1;
