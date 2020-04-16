package EPICS::PodXHtml;

use strict;
use warnings;

use base 'Pod::Simple::XHTML';

BEGIN {
    if ($Pod::Simple::XHTML::VERSION < '3.16') {
        # encode_entities() wasn't a method, add it
        our *encode_entities = sub {
            my ($self, $str) = @_;
            my %entities = (
                q{>} => 'gt',
                q{<} => 'lt',
                q{'} => '#39',
                q{"} => 'quot',
                q{&} => 'amp'
            );
            my $ents = join '', keys %entities;
            $str =~ s/([$ents])/'&' . $entities{$1} . ';'/ge;
            return $str;
        }
    }
}

# Translate L<link text|filename/Section name>
# into <a href="filename.html#Section-name">link text</a>

sub resolve_pod_page_link {
    my ($self, $to, $section) = @_;

    my $ret = defined $to ? "$to.html" : '';
    $ret .= '#' . $self->idify($self->encode_entities($section), 1)
        if defined $section;

    return $ret;
}

1;
