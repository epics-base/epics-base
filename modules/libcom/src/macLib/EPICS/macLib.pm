#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

package EPICS::macLib::entry;

sub new ($$) {
    my $class = shift;
    my $this = {
        name => shift,
        type => shift,
        raw => '',
        val => '',
        visited => 0,
        error => 0,
    };
    bless $this, $class;
    return $this;
}

sub report ($) {
    my ($this) = @_;
    return unless defined $this->{raw};
    printf "%1s %-16s %-16s %s\n",
        ($this->{error} ? '*' : ' '), $this->{name}, $this->{raw}, $this->{val};
}


package EPICS::macLib;

use Carp;

sub new ($@) {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $this = {
        dirty => 0,
        noWarn => 0,
        macros => [{}], # [0] is current scope, [1] is parent etc.
    };
    bless $this, $class;
    $this->installList(@_);
    return $this;
}

sub installList ($@) {
    # Argument is a list of strings which are arguments to installMacros
    my $this = shift;
    while (@_) {
        $this->installMacros(shift);
    }
}

sub installMacros ($$) {
    # Argument is a string: a=1,b="2",c,d='hello'
    my $this = shift;
    $_ = shift;
    until (defined pos($_) and pos($_) == length($_)) {
        m/\G \s* /xgc;    # Skip whitespace
        if (m/\G ( [A-Za-z0-9_-]+ ) \s* /xgc) {
            my ($name, $val) = ($1);
            if (m/\G = \s* /xgc) {
                # The value follows, handle quotes and escapes
                until (pos($_) == length($_)) {
                    if (m/\G , /xgc) { last; }
                    elsif (m/\G ' ( ( [^'] | \\ ' )* ) ' /xgc) { $val .= $1; }
                    elsif (m/\G " ( ( [^"] | \\ " )* ) " /xgc) { $val .= $1; }
                    elsif (m/\G \\ ( . ) /xgc) { $val .= $1; }
                    elsif (m/\G ( . ) /xgc) { $val .= $1; }
                    else { die "How did I get here?"; }
                }
                $this->putValue($name, $val);
            } elsif (m/\G , /xgc or (pos($_) == length($_))) {
                $this->putValue($name, undef);
            } else {
                warn "How did I get here?";
            }
        } elsif (m/\G ( .* )/xgc) {
            croak "Can't find a macro definition in '$1'";
        } else {
            last;
        }
    }
}

sub putValue ($$$) {
    my ($this, $name, $raw) = @_;
    if (exists $this->{macros}[0]{$name}) {
        if (!defined $raw) {
            delete $this->{macros}[0]{$name};
        } else {
            $this->{macros}[0]{$name}{raw} = $raw;
        }
    } else {
        my $entry = EPICS::macLib::entry->new($name, 'macro');
        $entry->{raw} = $raw;
        $this->{macros}[0]{$name} = $entry;
    }
    $this->{dirty} = 1;
}

sub pushScope ($) {
    my ($this) = @_;
    unshift @{$this->{macros}}, {};
}

sub popScope ($) {
    my ($this) = @_;
    shift @{$this->{macros}};
}

sub suppressWarning($$) {
    my ($this, $suppress) = @_;
    $this->{noWarn} = $suppress;
}

sub expandString($$) {
    my ($this, $src) = @_;
    $this->_expand;
    (my $name = $src) =~ s/^ (.{20}) .* $/$1.../xs;
    my $entry = EPICS::macLib::entry->new($name, 'string');
    my $result = $this->_translate($entry, 0, $src);
    return $result unless $entry->{error};
    return $this->{noWarn} ? $result : undef;
}

sub reportMacros ($) {
    my ($this) = @_;
    $this->_expand;
    print "Macro report\n============\n";
    foreach my $scope (@{$this->{macros}}) {
        foreach my $name (keys %{$scope}) {
            my $entry = $scope->{$name};
            $entry->report;
        }
    } continue {
        print " -- scope ends --\n";
    }
}


# Private routines, not intended for public use

sub _expand ($) {
    my ($this) = @_;
    return unless $this->{dirty};
    foreach my $scope (@{$this->{macros}}) {
        foreach my $name (keys %{$scope}) {
            my $entry = $scope->{$name};
            $entry->{val} = $this->_translate($entry, 1, $entry->{raw});
        }
    }
    $this->{dirty} = 0;
}

sub _lookup ($$$$$) {
    my ($this, $name) = @_;
    foreach my $scope (@{$this->{macros}}) {
        if (exists $scope->{$name}) {
            return undef   # Macro marked as deleted
                unless defined $scope->{$name}{raw};
            return $scope->{$name};
        }
    }
    return undef;
}

sub _translate ($$$$) {
    my ($this, $entry, $level, $str) = @_;
    return $this->_trans($entry, $level, '', \$str);
}

sub _trans ($$$$$) {
    my ($this, $entry, $level, $term, $R) = @_;
    return $$R
        if (!defined $$R or
            $$R =~ m/\A [^\$]* \Z/x);   # Short-circuit if no macros
    my $quote = 0;
    my $val;
    until (defined pos($$R) and pos($$R) == length($$R)) {
        if ($term and ($$R =~ m/\G (?= [$term] ) /xgc)) {
            last;
        }
        if ($$R =~ m/\G \$ ( [({] ) /xgc) {
            my $macEnd = $1;
            $macEnd =~ tr/({/)}/;
            my $name2 = $this->_trans($entry, $level+1, "=$macEnd", $R);
            my $entry2 = $this->_lookup($name2);
            if (!defined $entry2) {             # Macro not found
                if ($$R =~ m/\G = /xgc) {       # Use default value given
                    $val .= $this->_trans($entry, $level+1, $macEnd, $R);
                } else {
                    unless ($this->{noWarn}) {
                        $entry->{error} = 1;
                        printf STDERR "macLib: macro '%s' is undefined (expanding %s '%s')\n",
                            $name2, $entry->{type}, $entry->{name};
                    }
                    $val .= "\$($name2)";
                }
                $$R =~ m/\G [$macEnd] /xgc;     # Discard close bracket
            } else {                            # Macro found
                if ($entry2->{visited}) {
                    $entry->{error} = 1;
                    printf STDERR "macLib: %s '%s' is recursive (expanding %s '%s')\n",
                        $entry->{type}, $entry->{name}, $entry2->{type}, $entry2->{name};
                    $val .= "\$($name)";
                } else {
                    if ($$R =~ m/\G = /xgc) {   # Discard default value
                        local $this->{noWarn} = 1; # Temporarily kill warnings
                        $this->_trans($entry, $level+1, $macEnd, $R);
                    }
                    $$R =~ m/\G [$macEnd] /xgc; # Discard close bracket
                    if ($this->{dirty}) {       # Translate raw value
                        $entry2->{visited} = 1;
                        $val .= $this->_trans($entry, $level+1, '', \$entry2->{raw});
                        $entry2->{visited} = 0;
                    } else {
                        $val .= $entry2->{val}; # Here's one I made earlier...
                    }
                }
            }
        } elsif ($level > 0) {  # Discard quotes and escapes
            if ($quote and $$R =~ m/\G $quote /xgc) {
                $quote = 0;
            } elsif ($$R =~ m/\G ( ['"] ) /xgc) {
                $quote = $1;
            } elsif ($$R =~ m/\G \\? ( . ) /xgc) {
                $val .= $1;
            } else {
                warn "How did I get here? level=$level";
            }
        } else {                # Level 0
            if ($$R =~ m/\G \\ ( . ) /xgc) {
                $val .= "\\$1";
            } elsif ($$R =~ m/\G ( [^\\\$'")}]* ) /xgc) {
                $val .= $1;
            } elsif ($$R =~ m/\G ( . ) /xgc) {
                $val .= $1;
            } else {
                warn "How did I get here? level=$level";
            }
        }
    }
    return $val;
}

1;
