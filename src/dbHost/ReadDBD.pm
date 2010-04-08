package ReadDBD;
require 5.000;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(%breaktables %devices %drivers %menus %recordtypes
	     %registrars %functions %variables &ParseDBD);

my $RXnam = qr/[a-zA-Z0-9_\-:.<>;]+/o;
my $RXnum = qr/-? (?: \d+ ) | (?: \d* \. \d+ ) (?: [eE] [-+]? \d+ )?/ox;
my $RXdqs = qr/" (?: [^"] | \\" )* "/ox;
my $RXsqs = qr/' (?: [^'] | \\' )* '/ox;
my $string = qr/ ( $RXnam | $RXnum | $RXdqs | $RXsqs ) /ox;

our $debug=0;
our @context;

our %breaktables;	# hash{name} = ref array(array(raw,eng))
our %devices;		# hash{rtyp}{name} = array(linktype,dset)
our %drivers;		# hash{name} = name
our %menus;		# hash{name} = ref array(array(ident,choice))
our %recordtypes;	# hash{name} = ref array(array(fname,ref hash{attr}))
our %registrars;	# hash{name} = name
our %functions; 	# hash{name} = name
our %variables; 	# hash{name} = type

# The hash value is not currently used
my %field_types = (
	DBF_STRING   => 1,
	DBF_CHAR     => 1,
	DBF_UCHAR    => 1,
	DBF_SHORT    => 1,
	DBF_USHORT   => 1,
	DBF_LONG     => 1,
	DBF_ULONG    => 1,
	DBF_FLOAT    => 1,
	DBF_DOUBLE   => 1,
	DBF_ENUM     => 1,
	DBF_MENU     => 1,
	DBF_DEVICE   => 1,
	DBF_INLINK   => 1,
	DBF_OUTLINK  => 1,
	DBF_FWDLINK  => 1,
	DBF_NOACCESS => 1
);

# The hash value is a regexp that matches all legal values of this attribute
my %field_attrs = (
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
	menu        => qr/[a-zA-Z][a-zA-Z0-9_]*/
);

sub ParseDBD {
    $_ = join '', @_;
    while (1) {
	if (parseCommon()) {}
        elsif (m/\G menu \s* \( \s* $string \s* \) \s* \{/oxgc) {
            print "Menu: $1\n" if $debug;
            parse_menu($1);
        }
        elsif (m/\G driver \s* \( \s* $string \s* \)/oxgc) {
            print "Driver: $1\n" if $debug;
            add_driver($1);
        }
        elsif (m/\G registrar \s* \( \s* $string \s* \)/oxgc) {
            print "Registrar: $1\n" if $debug;
	    add_registrar($1);
        }
        elsif (m/\G function \s* \( \s* $string \s* \)/oxgc) {
            print "Function: $1\n" if $debug;
	    add_function($1);
        }
        elsif (m/\G breaktable \s* \( \s* $string \s* \) \s* \{/oxgc) {
            print "Breaktable: $1\n" if $debug;
            parse_breaktable($1);
        }
        elsif (m/\G recordtype \s* \( \s* $string \s* \) \s* \{/oxgc) {
            print "Recordtype: $1\n" if $debug;
            parse_recordtype($1);
        }
        elsif (m/\G variable \s* \( \s* $string \s* \)/oxgc) {
            print "Variable: $1\n" if $debug;
            add_variable($1, 'int');
        }
        elsif (m/\G variable \s* \( \s* $string \s* , \s* $string \s* \)/oxgc) {
            print "Variable: $1, $2\n" if $debug;
            add_variable($1, $2);
        }
        elsif (m/\G device \s* \( \s* $string \s* , \s* $string \s* ,
                          \s* $string \s* , \s*$string \s* \)/oxgc) {
            print "Device: $1, $2, $3, $4\n" if $debug;
	    add_device($1, $2, $3, $4);
        } else {
            last unless m/\G (.*) $/moxgc;
	    dieContext("Syntax error in '$1'");
        }
    }
}

sub parseCommon {
    # Skip leading whitespace
    m/\G \s* /oxgc;

    if (m/\G \#\#!BEGIN\{ ( [^}]* ) \}!\#\# \n/oxgc) {
        print "File-Begin: $1\n" if $debug;
	pushContext("file '$1'");
    }
    elsif (m/\G \#\#!END\{ ( [^}]* ) \}!\#\# \n/oxgc) {
        print "File-End: $1\n" if $debug;
	popContext("file '$1'");
    }
    elsif (m/\G \# (.*) \n/oxgc) {
        print "Comment: $1\n" if $debug;
    }
    else {
	return 0;
    }
    return 1;
}

sub pushContext {
    my ($ctxt) = @_;
    unshift @context, $ctxt;
}

sub popContext {
    my ($ctxt) = @_;
    my ($pop) = shift @context;
    ($ctxt ne $pop) and
	dieContext("Exiting context \"$ctxt\", found \"$pop\" instead.",
	    "\tBraces must close in the same file they were opened.");
}

sub dieContext {
    my ($msg) = join "\n\t", @_;
    print "$msg\n" if $msg;
    die "Context: ", join(' in ', @context), "\n";
}

sub parse_menu {
    my ($name) = @_;
    pushContext("menu($name)");
    my @menu;
    while(1) {
	if (parseCommon()) {}
	elsif (m/\G choice \s* \( \s* $string \s* ,
	                  \s* $string \s* \)/oxgc) {
            print " Menu-Choice: $1, $2\n" if $debug;
	    new_choice(\@menu, $1, $2);
	}
	elsif (m/\G \}/oxgc) {
            print " Menu-End:\n" if $debug;
	    add_menu($name, @menu);
	    popContext("menu($name)");
	    return;
	} else {
	    m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
	    dieContext("Syntax error in '$1'");
	}
    }
}

sub new_choice {
    my ($Rmenu, $choice_name, $choice_val) = @_;
    $choice_name = identifier($choice_name);
    $choice_val = unquote($choice_val);
    push @{$Rmenu}, [$choice_name, $choice_val];
}

sub identifier {
    my ($id) = @_;
    $id =~ m/^"(.*)"$/ and $id = $1;
    $id !~ m/[a-zA-Z][a-zA-Z0-9_]*/o and dieContext("Illegal identifier '$id'",
	"Identifiers are used in C code so must start with a letter, followed",
	"by letters, digits and/or underscore characters only.");
    return $id;
}

sub unquote {
    my ($string) = @_;
    $string =~ m/^"(.*)"$/o and $string = $1;
    return $string;
}

sub add_menu {
    my ($name, @menu) = @_;
    $name = identifier($name);
    $menus{$name} = \@menu unless exists $menus{$name};
}

sub add_driver {
    my ($name) = @_;
    $name = identifier($name);
    $drivers{$name} = $name unless exists $drivers{$name};
}

sub add_registrar {
    my ($reg_name) = @_;
    $reg_name = identifier($reg_name);
    $registrars{$reg_name} = $reg_name unless exists $registrars{$reg_name};
}

sub add_function {
    my ($func_name) = @_;
    $func_name = identifier($func_name);
    $functions{$func_name} = $func_name unless exists $functions{$func_name};
}

sub parse_breaktable {
    my ($name) = @_;
    pushContext("breaktable($name)");
    my @breaktable;
    while(1) {
	if (parseCommon()) {}
	elsif (m/\G point\s* \(\s* $string \s* , \s* $string \s* \)/oxgc) {
            print " Breaktable-Point: $1, $2\n" if $debug;
	    new_point(\@breaktable, $1, $2);
	}
	elsif (m/\G $string \s* (?: , \s*)? $string (?: \s* ,)?/oxgc) {
            print " Breaktable-Data: $1, $2\n" if $debug;
	    new_point(\@breaktable, $1, $2);
	}
	elsif (m/\G \}/oxgc) {
            print " Breaktable-End:\n" if $debug;
	    add_breaktable($name, @breaktable);
	    popContext("breaktable($name)");
	    return;
	} else {
	    m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
	    dieContext("Syntax error in '$1'");
	}
    }
}

sub new_point {
    my ($Rbreaktable, $raw_val, $eng_val) = @_;
    push @{$Rbreaktable}, [$raw_val, $eng_val];
}

sub add_breaktable {
    my ($name, @brktbl) = @_;
    $name = unquote($name);
    $breaktables{$name} = \@brktbl
        unless exists $breaktables{$name};
}

sub parse_recordtype {
    my ($name) = @_;
    pushContext("recordtype($name)");
    my @rtype;
    while(1) {
	if (parseCommon()) {}
	elsif (m/\G field \s* \( \s* $string \s* ,
	                  \s* $string \s* \) \s* \{/oxgc) {
            print " Recordtype-Field: $1, $2\n" if $debug;
	    parse_field(\@rtype, $1, $2);
	}
	elsif (m/\G \}/oxgc) {
            print " Recordtype-End:\n" if $debug;
	    add_recordtype($name, @rtype);
	    popContext("recordtype($name)");
	    return;
	} else {
	    m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
	    dieContext("Syntax error in '$1'");
	}
    }
}

sub parse_field {
    my ($Rrtype, $name, $field_type) = @_;
    $name = identifier($name);
    pushContext("field($name)");
    my %field = (type => DBF_type($field_type));
    while(1) {
	if (parseCommon()) {}
	elsif (m/\G (\w+) \s* \( \s* $string \s* \)/oxgc) {
            print "  Field-Attribute: $1, $2\n" if $debug;
	    exists $field_attrs{$1} or dieContext("Unknown field attribute ".
		"'$1', valid attributes are:", sort keys %field_attrs);
	    $attr = $1;
	    $value = unquote($2);
	    $value =~ m/^$field_attrs{$attr}$/ or dieContext("Bad value '$value' ".
	        "for field '$attr' attribute");
	    $field{$attr} = $value;
	}
	elsif (m/\G \}/oxgc) {
            print "  Field-End:\n" if $debug;
	    new_field($Rrtype, $name, %field);
	    popContext("field($name)");
	    return;
	} else {
	    m/\G (.*) $/moxgc or dieContext("Unexpected end of input");
	    dieContext("Syntax error in '$1'");
	}
    }
}

sub DBF_type {
    my ($type) = @_;
    $type =~ m/^"(.*)"$/o and $type = $1;
    exists $field_types{$type} or dieContext("Illegal field type '$type', ".
	"valid field types are:", sort keys %field_types);
    return $type;
}

sub new_field {
    my ($Rrtype, $name, %field) = @_;
    push @{$Rrtype}, [$name, \%field];
}

sub add_recordtype {
    my ($name, @rtype) = @_;
    $name = identifier($name);
    $recordtypes{$name} = \@rtype
	unless exists $recordtypes{$name};
}

sub add_variable {
    my ($var_name, $var_type) = @_;
    $var_name = identifier($var_name);
    $var_type = unquote($var_type);
    $variables{$var_name} = $var_type
	unless exists $variables{$var_name};
}

sub add_device {
    my ($record_type, $link_type, $dset, $dev_name) = @_;
    $record_type = unquote($record_type);
    $link_type = unquote($link_type);
    $dset = identifier($dset);
    $dev_name = unquote($dev_name);
    if (!exists($recordtypes{$record_type})) {
    	dieContext("Device support for unknown record type '$record_type'",
	    "device($record_type, $link_type, $dset, \"$dev_name\")");
    }
    $devices{$record_type}{$dev_name} = [$link_type, $dset]
	unless exists $devices{$record_type}{$dev_name};
}

1;
