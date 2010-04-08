package macLib::entry;

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


package macLib;

use Carp;

sub new ($%) {
    my ($proto, %values) = @_;
    my $class = ref($proto) || $proto;
    my $this = {
        dirty => 0,
        noWarn => 0,
        macros => [{}], # [0] is current scope, [1] is parent etc.
    };
    bless $this, $class;
    $this->installHash(%values);
    return $this;
}

sub suppressWarning($$) {
    my ($this, $suppress) = @_;
    $this->{noWarn} = $suppress;
}

sub expandString($$) {
    my ($this, $src) = @_;
    $this->_expand;
    my $entry = macLib::entry->new($src, 'string');
    my $result = $this->_translate($entry, 0, $src);
    return $result unless $entry->{error};
    return $this->{noWarn} ? $result : undef;
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
        my $entry = macLib::entry->new($name, 'macro');
        $entry->{raw} = $raw;
        $this->{macros}[0]{$name} = $entry;
    }
    $this->{dirty} = 1;
}

sub installHash ($%) {
    my ($this, %values) = @_;
    foreach $key (keys %values) {
        $this->putValue($key, $values{$key});
    }
}

sub installMacros ($$) {
    my $this = shift;
    $_ = shift;
    my $eos = 0;
    until ($eos ||= m/\G \z/xgc) {
        m/\G \s* /xgc;    # Skip whitespace
        if (m/\G ( \w+ ) \s* /xgc) {
            my ($name, $val) = ($1);
            if (m/\G = \s* /xgc) {
                # The value follows, handle quotes and escapes
                until ($eos ||= m/\G \z/xgc) {
                    if (m/\G , /xgc) { last; }
                    elsif (m/\G ' ( ( [^'] | \\ ' )* ) ' /xgc) { $val .= $1; }
                    elsif (m/\G " ( ( [^"] | \\ " )* ) " /xgc) { $val .= $1; }
                    elsif (m/\G \\ ( . ) /xgc) { $val .= $1; }
                    elsif (m/\G ( . ) /xgc) { $val .= $1; }
                    else { die "How did I get here?"; }
                }
                $this->putValue($name, $val);
            } elsif (m/\G , /xgc or ($eos ||= m/\G \z/xgc)) {
                $this->putValue($name, undef);
            } else {
                die "How did I get here?";
            }
        } elsif (m/\G ( .* )/xgc) {
            croak "Can't find a macro definition in '$1'";
        } else {
            last;
        }
    }
}

sub pushScope ($) {
    my ($this) = @_;
    push @{$this->{macros}}, {};
}

sub popScope ($) {
    my ($this) = @_;
    pop @{$this->{macros}};
}

sub reportMacros ($) {
    my ($this) = @_;
    $this->_expand;
    print "Macro report\n";
    foreach my $scope (@{$this->{macros}}) {
        foreach my $name (keys %{$scope}) {
            my $entry = $scope->{$name};
            $entry->report;
        }
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
        if ($$R =~ m/\A [^\$]* \Z/x);   # Short-circuit if no macros
    my $quote = 0;
    my $val;
    until ($$R =~ m/\G \z/xgc) {
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
                die "How did I get here?";
            }
        } else {                # Level 0
            if ($$R =~ m/\G \\ ( . ) /xgc) {
                $val .= "\\$1";
            } elsif ($$R =~ m/\G ( [^\\\$'")}]* ) /xgc) {
                $val .= $1;
            } elsif ($$R =~ m/\G ( . ) /xgc) {
                $val .= $1;
            } else {
                die "How did I get here?";
            }
        }
    }
    return $val;
}

1;
