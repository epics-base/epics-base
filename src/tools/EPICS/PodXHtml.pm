package EPICS::PodXHtml;

use strict;
use warnings;

use base 'Pod::Simple::XHTML';

# Translate L<link text|filename/Section name>
# into <a href="filename.html#Section-name">link text</a>

sub resolve_pod_page_link {
    my ($self, $to, $section) = @_;

    my $ret = $to ? "$to.html" : '';
    $ret .= '#' . $self->idify($self->encode_entities($section), 1)
        if $section;

    return $ret;
}

1;
