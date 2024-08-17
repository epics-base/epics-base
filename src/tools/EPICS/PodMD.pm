# SPDX-License-Identifier: EPICS

package EPICS::PodMD;

use strict;
use warnings;

use base 'Pod::Markdown';

# Translate L<link text|filename/Section name>
# into <a href="filename.md#Section-name">link text</a>#
# This is for Sphinx processing on Readthedocs. Sphinx converts
# links to .md into .html automatically in the processing.

sub format_perldoc_url {
  my ($self, $name, $section) = @_;

  my $url_prefix = $self->perldoc_url_prefix;
  if (
    defined($name)
    && $self->is_local_module($name)
    && defined($self->local_module_url_prefix)
  ) {
    $url_prefix = $self->local_module_url_prefix;
  }

  my $url = '';

  # If the link is to another module (external link).
  if (defined($name)) {
    $name .= ".md" unless $name =~ m/\.md$/; 
    $url = $url_prefix .
      ($self->escape_url ? URI::Escape::uri_escape($name) : $name);
  }

  # See https://rt.cpan.org/Ticket/Display.html?id=57776
  # for a discussion on the need to mangle the section.
  if ($section){

    my $method = $url
      # If we already have a prefix on the url it's external.
      ? $self->perldoc_fragment_format
      # Else an internal link points to this markdown doc.
      : $self->markdown_fragment_format;

    $method = 'format_fragment_' . $method
      unless ref($method);

    {
      # Set topic to enable code refs to be simple.
      local $_ = $section;
      $section = $self->$method($section);
    }

    $url .= '#' . $section;
  }

  return $url;
}

1;
