package EPICS::PodXHtml;

use strict;
use warnings;

use base 'Pod::Simple::XHTML';

our $VERSION = '1.2.1';

BEGIN {
    if ($Pod::Simple::XHTML::VERSION < '3.16') {
        # Add encode_entities() as a method
        sub encode_entities {
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

sub _end_head {
    my $h = delete $_[0]{in_head};

    my $add = $_[0]->html_h_level;
    $add = 1 unless defined $add;
    $h += $add - 1;

    my $id = $_[0]->idify($_[0]{htext});
    my $text = $_[0]{scratch};
    my $hid = qq{<h$h id="$id">};
    my $link = qq{ <a class='sect' href="#$id">&sect;</a>};
    $_[0]{'scratch'} = $_[0]->backlink && ($h - $add == 0)
                         # backlinks enabled && =head1
                         ? qq{$hid<a href="#_podtop_">$text</a> $link</h$h>}
                         : qq{$hid$text $link</h$h>};
    $_[0]->emit;
    push @{ $_[0]{'to_index'} }, [$h, $id, delete $_[0]{'htext'}];
}

1;
