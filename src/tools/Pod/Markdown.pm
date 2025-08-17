# vim: set ts=2 sts=2 sw=2 expandtab smarttab:
#
# This file is part of Pod-Markdown
#
# This software is copyright (c) 2011 by Randy Stauner.
#
# This is free software; you can redistribute it and/or modify it under
# the same terms as the Perl 5 programming language system itself.
# SPDX-License-Identifier: Artistic-2.0
#
use 5.008;
use strict;
use warnings;

package Pod::Markdown;
# git description: v3.300-3-gb01c18d

our $AUTHORITY = 'cpan:RWSTAUNER';
# ABSTRACT: Convert POD to Markdown
$Pod::Markdown::VERSION = '3.400';
use Pod::Simple 3.27 (); # detected_encoding and keep_encoding bug fix
use parent qw(Pod::Simple::Methody);
use Encode ();
use URI::Escape ();

our %URL_PREFIXES = (
  sco      => 'http://search.cpan.org/perldoc?',
  metacpan => 'https://metacpan.org/pod/',
  man      => 'http://man.he.net/man',
);
$URL_PREFIXES{perldoc} = $URL_PREFIXES{metacpan};

our $LOCAL_MODULE_RE = qr/^(Local::|\w*?_\w*)/;

## no critic
#{
  our $HAS_HTML_ENTITIES;

  # Stolen from Pod::Simple::XHTML 3.28. {{{

  BEGIN {
    $HAS_HTML_ENTITIES = eval "require HTML::Entities; 1";
  }

  my %entities = (
    q{>} => 'gt',
    q{<} => 'lt',
    q{'} => '#39',
    q{"} => 'quot',
    q{&} => 'amp',
  );

  sub encode_entities {
    my $self = shift;
    my $ents = $self->html_encode_chars;
    return HTML::Entities::encode_entities( $_[0], $ents ) if $HAS_HTML_ENTITIES;
    if (defined $ents) {
        $ents =~ s,(?<!\\)([]/]),\\$1,g;
        $ents =~ s,(?<!\\)\\\z,\\\\,;
    } else {
        $ents = join '', keys %entities;
    }
    my $str = $_[0];
    $str =~ s/([$ents])/'&' . ($entities{$1} || sprintf '#x%X', ord $1) . ';'/ge;
    return $str;
  }

  # }}}

  # Add a few very common ones for consistency and readability
  # (in case HTML::Entities isn't available).
  %entities = (
    # Pod::Markdown has always required 5.8 so unicode_to_native will be available.
    chr(utf8::unicode_to_native(0xA0)) => 'nbsp',
    chr(utf8::unicode_to_native(0xA9)) => 'copy',
    %entities
  );

  sub __entity_encode_ord_he {
    my $chr = chr $_[0];
    # Skip the encode_entities() logic and go straight for the substitution
    # since we already have the char we know we want replaced.
    # Both the hash and the function are documented as exportable (so should be reliable).
    return $HTML::Entities::char2entity{ $chr } || HTML::Entities::num_entity( $chr );
  }
  sub __entity_encode_ord_basic {
    return '&' . ($entities{chr $_[0]} || sprintf '#x%X', $_[0]) . ';';
  }

  # From HTML::Entities 3.69
  my $DEFAULT_ENTITY_CHARS = '^\n\r\t !\#\$%\(-;=?-~';

#}
## use critic

# Use hash for simple "exists" check in `new` (much more accurate than `->can`).
my %attributes = map { ($_ => 1) }
  qw(
    html_encode_chars
    match_encoding
    output_encoding
    local_module_re
    local_module_url_prefix
    man_url_prefix
    perldoc_url_prefix
    perldoc_fragment_format
    markdown_fragment_format
    include_meta_tags
    escape_url
  );


sub new {
  my $class = shift;
  my %args = @_;

  my $self = $class->SUPER::new();
  $self->preserve_whitespace(1);
  $self->nbsp_for_S(1);
  $self->accept_targets(qw( markdown html ));
  $self->escape_url(1);

  # Default to the global, but allow it to be overwritten in args.
  $self->local_module_re($LOCAL_MODULE_RE);

  for my $type ( qw( perldoc man ) ){
    my $attr  = $type . '_url_prefix';
    # Initialize to the alias.
    $self->$attr($type);
  }

  while( my ($attr, $val) = each %args ){
    # NOTE: Checking exists on a private var means we don't allow Pod::Simple
    # attributes to be set this way.  It's not very consistent, but I think
    # I'm ok with that for now since there probably aren't many Pod::Simple attributes
    # being changed besides `output_*` which feel like API rather than attributes.
    # We'll see.
    # This is currently backward-compatible as we previously just put the attribute
    # into the private stash so anything unknown was silently ignored.
    # We could open this up to `$self->can($attr)` in the future if that seems better
    # but it tricked me when I was testing a misspelled attribute name
    # which also happened to be a Pod::Simple method.

    exists $attributes{ $attr } or
      # Provide a more descriptive message than "Can't locate object method".
      warn("Unknown argument to ${class}->new(): '$attr'"), next;

    # Call setter.
    $self->$attr($val);
  }

  # TODO: call from the setters.
  $self->_prepare_fragment_formats;

  if(defined $self->local_module_url_prefix && $self->local_module_url_prefix eq '' && !$self->escape_url) {
    warn("turning escape_url with an empty local_module_url_prefix is not recommended as relative URLs could be confused for IPv6 addresses");
  }

  return $self;
}

for my $type ( qw( local_module perldoc man ) ){
  my $attr  = $type . '_url_prefix';
  no strict 'refs'; ## no critic
  *$attr = sub {
    my $self = shift;
    if (@_) {
      $self->{$attr} = $URL_PREFIXES{ $_[0] } || $_[0];
    }
    else {
      return $self->{$attr};
    }
  }
}

## Attribute accessors ##


sub html_encode_chars {
  my $self  = shift;
  my $stash = $self->_private;

  # Setter.
  if( @_ ){
    # If false ('', 0, undef), disable.
    if( !$_[0] ){
      delete $stash->{html_encode_chars};
      $stash->{encode_amp}  = 1;
      $stash->{encode_lt}   = 1;
    }
    else {
      # Special case boolean '1' to mean "all".
      # If we have HTML::Entities, undef will use the default.
      # Without it, we need to specify so that we use the same list (for consistency).
      $stash->{html_encode_chars} = $_[0] eq '1' ? ($HAS_HTML_ENTITIES ? undef : $DEFAULT_ENTITY_CHARS) : $_[0];

      # If [char] doesn't get encoded, we need to do it ourselves.
      $stash->{encode_amp}  = ($self->encode_entities('&') eq '&');
      $stash->{encode_lt}   = ($self->encode_entities('<') eq '<');
    }
    return;
  }

  # Getter.
  return $stash->{html_encode_chars};
}


# I prefer ro-accessors (immutability!) but it can be confusing
# to not support the same API as other Pod::Simple classes.

# NOTE: Pod::Simple::_accessorize is not a documented public API.
# Skip any that have already been defined.
__PACKAGE__->_accessorize(grep { !__PACKAGE__->can($_) } keys %attributes);

sub _prepare_fragment_formats {
  my ($self) = @_;

  foreach my $attr ( keys %attributes ){
    next unless $attr =~ /^(\w+)_fragment_format/;
    my $type = $1;
    my $format = $self->$attr;

    # If one was provided.
    if( $format ){
      # If the attribute is a coderef just use it.
      next if ref($format) eq 'CODE';
    }
    # Else determine a default.
    else {
      if( $type eq 'perldoc' ){
        # Choose a default that matches the destination url.
        my $target = $self->perldoc_url_prefix;
        foreach my $alias ( qw( metacpan sco ) ){
          if( $target eq $URL_PREFIXES{ $alias } ){
            $format = $alias;
          }
        }
        # This seems like a reasonable fallback.
        $format ||= 'pod_simple_xhtml';
      }
      else {
        $format = $type;
      }
    }

    # The short name should become a method name with the prefix prepended.
    my $prefix = 'format_fragment_';
    $format =~ s/^$prefix//;
    die "Unknown fragment format '$format'"
      unless $self->can($prefix . $format);

    # Save it.
    $self->$attr($format);
  }

  return;
}

## Backward compatible API ##

# For backward compatibility (previously based on Pod::Parser):
# While Pod::Simple provides a parse_from_file() method
# it's primarily for Pod::Parser compatibility.
# When called without an output handle it will print to STDOUT
# but the old Pod::Markdown never printed to a handle
# so we don't want to start now.
sub parse_from_file {
  my ($self, $file) = @_;

  # TODO: Check that all dependent cpan modules use the Pod::Simple API
  # then add a deprecation warning here to avoid confusion.

  $self->output_string(\($self->{_as_markdown_}));
  $self->parse_file($file);
}

# Likewise, though Pod::Simple doesn't define this method at all.
sub parse_from_filehandle { shift->parse_from_file(@_) }


## Document state ##

sub _private {
  my ($self) = @_;
  $self->{_Pod_Markdown_} ||= {
    indent      => 0,
    stacks      => [],
    states      => [{}],
    link        => [],
    encode_amp  => 1,
    encode_lt   => 1,
  };
}

sub _increase_indent {
  ++$_[0]->_private->{indent} >= 1
    or die 'Invalid state: indent < 0';
}
sub _decrease_indent {
  --$_[0]->_private->{indent} >= 0
    or die 'Invalid state: indent < 0';
}

sub _new_stack {
  push @{ $_[0]->_private->{stacks} }, [];
  push @{ $_[0]->_private->{states} }, {};
}

sub _last_string {
  $_[0]->_private->{stacks}->[-1][-1];
}

sub _pop_stack_text {
  $_[0]->_private->{last_state} = pop @{ $_[0]->_private->{states} };
  join '', @{ pop @{ $_[0]->_private->{stacks} } };
}

sub _stack_state {
  $_[0]->_private->{states}->[-1];
}

sub _save {
  my ($self, $text) = @_;
  push @{ $self->_private->{stacks}->[-1] }, $text;
  # return $text; # DEBUG
}

sub _save_line {
  my ($self, $text) = @_;

  $text = $self->_process_escapes($text);

  $self->_save($text . $/);
}

# For paragraphs, etc.
sub _save_block {
  my ($self, $text) = @_;

  $self->_stack_state->{blocks}++;

  $self->_save_line($self->_indent($text) . $/);
}

## Formatting ##

sub _chomp_all {
  my ($self, $text) = @_;
  1 while chomp $text;
  return $text;
}

sub _indent {
  my ($self, $text) = @_;
  my $level = $self->_private->{indent};

  if( $level ){
    my $indent = ' ' x ($level * 4);

    # Capture text on the line so that we don't indent blank lines (/^\x20{4}$/).
    $text =~ s/^(.+)/$indent$1/mg;
  }

  return $text;
}

# as_markdown() exists solely for backward compatibility
# and requires having called parse_from_file() to be useful.


sub as_markdown {
    my ($parser, %args) = @_;
    my @header;
    # Don't add meta tags again if we've already done it.
    if( $args{with_meta} && !$parser->include_meta_tags ){
        @header = $parser->_build_markdown_head;
    }
    return join("\n" x 2, @header, $parser->{_as_markdown_});
}

sub _build_markdown_head {
    my $parser    = shift;
    my $data      = $parser->_private;
    return join "\n",
        map  { qq![[meta \l$_="$data->{$_}"]]! }
        grep { defined $data->{$_} }
        qw( Title Author );
}

## Escaping ##

# http://daringfireball.net/projects/markdown/syntax#backslash
# Markdown provides backslash escapes for the following characters:
#
# \   backslash
# `   backtick
# *   asterisk
# _   underscore
# {}  curly braces
# []  square brackets
# ()  parentheses
# #   hash mark
# +   plus sign
# -   minus sign (hyphen)
# .   dot
# !   exclamation mark

# However some of those only need to be escaped in certain places:
# * Backslashes *do* need to be escaped or they may be swallowed by markdown.
# * Word-surrounding characters (/[`*_]/) *do* need to be escaped mid-word
# because the markdown spec explicitly allows mid-word em*pha*sis.
# * I don't actually see anything that curly braces are used for.
# * Escaping square brackets is enough to avoid accidentally
# creating links and images (so we don't need to escape plain parentheses
# or exclamation points as that would generate a lot of unnecesary noise).
# Parentheses will be escaped in urls (&end_L) to avoid premature termination.
# * We don't need a backslash for every hash mark or every hyphen found mid-word,
# just the ones that start a line (likewise for plus and dot).
# (Those will all be handled by _escape_paragraph_markdown).


# Backslash escape markdown characters to avoid having them interpreted.
sub _escape_inline_markdown {
  local $_ = $_[1];

# s/([\\`*_{}\[\]()#+-.!])/\\$1/g; # See comments above.
  s/([\\`*_\[\]])/\\$1/g;

  return $_;
}

# Escape markdown characters that would be interpreted
# at the start of a line.
sub _escape_paragraph_markdown {
    local $_ = $_[1];

    # Escape headings, horizontal rules, (unordered) lists, and blockquotes.
    s/^([-+#>])/\\$1/mg;

    # Markdown doesn't support backslash escapes for equal signs
    # even though they can be used to underline a header.
    # So use html to escape them to avoid having them interpreted.
    s/^([=])/sprintf '&#x%x;', ord($1)/mge;

    # Escape the dots that would wrongfully create numbered lists.
    s/^( (?:>\s+)? \d+ ) (\.\x20)/$1\\$2/xgm;

    return $_;
}


# Additionally Markdown allows inline html so we need to escape things that look like it.
# While _some_ Markdown processors handle backslash-escaped html,
# [Daring Fireball](http://daringfireball.net/projects/markdown/syntax) states distinctly:
# > In HTML, there are two characters that demand special treatment: < and &...
# > If you want to use them as literal characters, you must escape them as entities, e.g. &lt;, and &amp;.

# It goes on to say:
# > Markdown allows you to use these characters naturally,
# > taking care of all the necessary escaping for you.
# > If you use an ampersand as part of an HTML entity,
# > it remains unchanged; otherwise it will be translated into &amp;.
# > Similarly, because Markdown supports inline HTML,
# > if you use angle brackets as delimiters for HTML tags, Markdown will treat them as such.

# In order to only encode the occurrences that require it (something that
# could be interpreted as an entity) we escape them all so that we can do the
# suffix test later after the string is complete (since we don't know what
# strings might come after this one).

my %_escape =
  map {
    my ($k, $v) = split /:/;
    # Put the "code" marker before the char instead of after so that it doesn't
    # get confused as the $2 (which is what requires us to entity-encode it).
    # ( "XsX", "XcsX", "X(c?)sX" )
    my ($s, $code, $re) = map { "\0$_$v\0" } '', map { ($_, '('.$_.'?)') } 'c';

    (
      $k         => $s,
      $k.'_code' => $code,
      $k.'_re'   => qr/$re/,
    )
  }
    qw( amp:& lt:< );

# Make the values of this private var available to the tests.
sub __escape_sequences { %_escape }


# HTML-entity encode any characters configured by the user.
# If that doesn't include [&<] then we escape those chars so we can decide
# later if we will entity-encode them or put them back verbatim.
sub _encode_or_escape_entities {
  my $self  = $_[0];
  my $stash = $self->_private;
  local $_  = $_[1];

  if( $stash->{encode_amp} ){
    if( exists($stash->{html_encode_chars}) ){
      # Escape all amps for later processing.
      # Pass intermediate strings to entity encoder so that it doesn't
      # process any of the characters of our escape sequences.
      # Use -1 to get "as many fields as possible" so that we keep leading and
      # trailing (possibly empty) fields.
      $_ = join $_escape{amp}, map { $self->encode_entities($_) } split /&/, $_, -1;
    }
    else {
      s/&/$_escape{amp}/g;
    }
  }
  elsif( exists($stash->{html_encode_chars}) ){
    $_ = $self->encode_entities($_);
  }

  s/</$_escape{lt}/g
    if $stash->{encode_lt};

  return $_;
}

# From Markdown.pl version 1.0.1 line 1172 (_DoAutoLinks).
my $EMAIL_MARKER = qr{
#   <                  # Opening token is in parent regexp.
        (?:mailto:)?
    (
      [-.\w]+
      \@
      [-a-z0-9]+(\.[-a-z0-9]+)*\.[a-z]+
    )
    >
}x;

# Process any escapes we put in the text earlier,
# now that the text is complete (end of a block).
sub _process_escapes {
  my $self  = $_[0];
  my $stash = $self->_private;
  local $_  = $_[1];

  # The patterns below are taken from Markdown.pl 1.0.1 _EncodeAmpsAndAngles().
  # In this case we only want to encode the ones that Markdown won't.
  # This is overkill but produces nicer looking text (less escaped entities).
  # If it proves insufficent then we'll just encode them all.

  # $1: If the escape was in a code sequence, simply replace the original.
  # $2: If the unescaped value would be followed by characters
  #     that could be interpreted as html, entity-encode it.
  # else: The character is safe to leave bare.

  # Neither currently allows $2 to contain '0' so bool tests are sufficient.

  if( $stash->{encode_amp} ){
    # Encode & if succeeded by chars that look like an html entity.
    s,$_escape{amp_re}((?:#?[xX]?(?:[0-9a-fA-F]+|\w+);)?),
      $1 ? '&'.$2 : $2 ? '&amp;'.$2 : '&',egos;
  }

  if( $stash->{encode_lt} ){
    # Encode < if succeeded by chars that look like an html tag.
    # Leave email addresses (<foo@bar.com>) for Markdown to process.
    s,$_escape{lt_re}((?=$EMAIL_MARKER)|(?:[a-z/?\$!])?),
      $1 ? '<'.$2 : $2 ?  '&lt;'.$2 : '<',egos;
  }

  return $_;
}


## Parsing ##

sub handle_text {
  my $self  = $_[0];
  my $stash = $self->_private;
  local $_  = $_[1];

  # Unless we're in a code span, verbatim block, or formatted region.
  unless( $stash->{no_escape} ){

    # We could, in theory, alter what gets escaped according to context
    # (for example, escape square brackets (but not parens) inside link text).
    # The markdown produced might look slightly nicer but either way you're
    # at the whim of the markdown processor to interpret things correctly.
    # For now just escape everything.

    # Don't let literal characters be interpreted as markdown.
    $_ = $self->_escape_inline_markdown($_);

    # Entity-encode (or escape for later processing) necessary/desired chars.
    $_ = $self->_encode_or_escape_entities($_);

  }
  # If this _is_ a code section, do limited/specific handling.
  else {
    # Always escaping these chars ensures that we won't mangle the text
    # in the unlikely event that a sequence matching our escape occurred in the
    # input stream (since we're going to escape it and then unescape it).
    s/&/$_escape{amp_code}/gos if $stash->{encode_amp};
    s/</$_escape{lt_code}/gos  if $stash->{encode_lt};
  }

  $self->_save($_);
}

sub start_Document {
  my ($self) = @_;
  $self->_new_stack;
}

sub   end_Document {
  my ($self) = @_;
  $self->_check_search_header;
  my $end = pop @{ $self->_private->{stacks} };

  @{ $self->_private->{stacks} } == 0
    or die 'Document ended with stacks remaining';

  my @doc = $self->_chomp_all(join('', @$end)) . $/;

  if( $self->include_meta_tags ){
    unshift @doc, $self->_build_markdown_head, ($/ x 2);
  }

  if( my $encoding = $self->_get_output_encoding ){
    # Do the check outside the loop(s) for efficiency.
    my $ents = $HAS_HTML_ENTITIES ? \&__entity_encode_ord_he : \&__entity_encode_ord_basic;
    # Iterate indices to avoid copying large strings.
    for my $i ( 0 .. $#doc ){
      print { $self->{output_fh} } Encode::encode($encoding, $doc[$i], $ents);
    }
  }
  else {
    print { $self->{output_fh} } @doc;
  }
}

sub _get_output_encoding {
  my ($self) = @_;

  # If 'match_encoding' is set we need to return an encoding.
  # If pod has no =encoding, Pod::Simple will guess if it sees a high-bit char.
  # If there are no high-bit chars, encoding is undef.
  # Use detected_encoding() rather than encoding() because if Pod::Simple
  # can't use whatever encoding was specified, we probably can't either.
  # Fallback to 'o_e' if no match is found.  This gives the user the choice,
  # since otherwise there would be no reason to specify 'o_e' *and* 'm_e'.
  # Fallback to UTF-8 since it is a reasonable default these days.

  return $self->detected_encoding || $self->output_encoding || 'UTF-8'
    if $self->match_encoding;

  # If output encoding wasn't specified, return false.
  return $self->output_encoding;
}

## Blocks ##

sub start_Verbatim {
  my ($self) = @_;
  $self->_new_stack;
  $self->_private->{no_escape} = 1;
}

sub end_Verbatim {
  my ($self) = @_;

  my $text = $self->_pop_stack_text;

  $text = $self->_indent_verbatim($text);

  $self->_private->{no_escape} = 0;

  # Verbatim blocks do not generate a separate "Para" event.
  $self->_save_block($text);
}

sub _indent_verbatim {
  my ($self, $paragraph) = @_;

    # NOTE: Pod::Simple expands the tabs for us (as suggested by perlpodspec).
    # Pod::Simple also has a 'strip_verbatim_indent' attribute
    # but it doesn't sound like it gains us anything over this method.

    # POD verbatim can start with any number of spaces (or tabs)
    # markdown should be 4 spaces (or a tab)
    # so indent any paragraphs so that all lines start with at least 4 spaces
    my @lines = split /\n/, $paragraph;
    my $indent = ' ' x 4;
    foreach my $line ( @lines ){
        next unless $line =~ m/^( +)/;
        # find the smallest indentation
        $indent = $1 if length($1) < length($indent);
    }
    if( (my $smallest = length($indent)) < 4 ){
        # invert to get what needs to be prepended
        $indent = ' ' x (4 - $smallest);

        # Prepend indent to each line.
        # We could check /\S/ to only indent non-blank lines,
        # but it's backward compatible to respect the whitespace.
        # Additionally, both pod and markdown say they ignore blank lines
        # so it shouldn't hurt to leave them in.
        $paragraph = join "\n", map { length($_) ? $indent . $_ : '' } @lines;
    }

  return $paragraph;
}

sub start_Para {
  $_[0]->_new_stack;
}

sub   end_Para {
  my ($self) = @_;
  my $text = $self->_pop_stack_text;

  $text = $self->_escape_paragraph_markdown($text);

  $self->_save_block($text);
}


## Headings ##

sub start_head1 { $_[0]->_start_head(1) }
sub   end_head1 { $_[0]->_end_head(1) }
sub start_head2 { $_[0]->_start_head(2) }
sub   end_head2 { $_[0]->_end_head(2) }
sub start_head3 { $_[0]->_start_head(3) }
sub   end_head3 { $_[0]->_end_head(3) }
sub start_head4 { $_[0]->_start_head(4) }
sub   end_head4 { $_[0]->_end_head(4) }

sub _check_search_header {
  my ($self) = @_;
  # Save the text since the last heading if we want it for metadata.
  if( my $last = $self->_private->{search_header} ){
    for( $self->_private->{$last} = $self->_last_string ){
      s/\A\s+//;
      s/\s+\z//;
    }
  }
}
sub _start_head {
  my ($self) = @_;
  $self->_check_search_header;
  $self->_new_stack;
}

sub   _end_head {
  my ($self, $num) = @_;
  my $h = '#' x $num;

  my $text = $self->_pop_stack_text;
  $self->_private->{search_header} =
      $text =~ /NAME/   ? 'Title'
    : $text =~ /AUTHOR/ ? 'Author'
    : undef;

  # TODO: option for $h suffix
  # TODO: put a name="" if $self->{embed_anchor_tags}; ?
  # https://rt.cpan.org/Ticket/Display.html?id=57776
  $self->_save_block(join(' ', $h, $text));
}

## Lists ##

# With Pod::Simple->parse_empty_lists(1) there could be an over_empty event,
# but what would you do with that?

sub _start_list {
  my ($self) = @_;
  $self->_new_stack;

  # Nest again b/c start_item will pop this to look for preceding content.
  $self->_increase_indent;
  $self->_new_stack;
}

sub   _end_list {
  my ($self) = @_;
  $self->_handle_between_item_content;

  # Finish the list.

  # All the child elements should be blocks,
  # but don't end with a double newline.
  my $text = $self->_chomp_all($self->_pop_stack_text);

  $_[0]->_save_line($text . $/);
}

sub _handle_between_item_content {
  my ($self) = @_;

  # This might be empty (if the list item had no additional content).
  if( my $text = $self->_pop_stack_text ){
    # Else it's a sub-document.
    # If there are blocks we need to separate with blank lines.
    if( $self->_private->{last_state}->{blocks} ){
      $text = $/ . $text;
    }
    # If not, we can condense the text.
    # In this module's history there was a patch contributed to specifically
    # produce "huddled" lists so we'll try to maintain that functionality.
    else {
      $text = $self->_chomp_all($text) . $/;
    }
    $self->_save($text)
  }

  $self->_decrease_indent;
}

sub _start_item {
  my ($self) = @_;
  $self->_handle_between_item_content;
  $self->_new_stack;
}

sub   _end_item {
  my ($self, $marker) = @_;
  my $text = $self->_pop_stack_text;
  $self->_save_line($self->_indent($marker .
    # Add a space only if there is text after the marker.
    (defined($text) && length($text) ? ' ' . $text : '')
  ));

  # Store any possible contents in a new stack (like a sub-document).
  $self->_increase_indent;
  $self->_new_stack;
}

sub start_over_bullet { $_[0]->_start_list }
sub   end_over_bullet { $_[0]->_end_list }

sub start_item_bullet { $_[0]->_start_item }
sub   end_item_bullet { $_[0]->_end_item('-') }

sub start_over_number { $_[0]->_start_list }
sub   end_over_number { $_[0]->_end_list }

sub start_item_number {
  $_[0]->_start_item;
  # It seems like this should be a stack,
  # but from testing it appears that the corresponding 'end' event
  # comes right after the text (it doesn't surround any embedded content).
  # See t/nested.t which shows start-item, text, end-item, para, start-item....
  $_[0]->_private->{item_number} = $_[1]->{number};
}

sub   end_item_number {
  my ($self) = @_;
  $self->_end_item($self->_private->{item_number} . '.');
}

# Markdown doesn't support definition lists
# so do regular (unordered) lists with indented paragraphs.
sub start_over_text { $_[0]->_start_list }
sub   end_over_text { $_[0]->_end_list }

sub start_item_text { $_[0]->_start_item }
sub   end_item_text { $_[0]->_end_item('-')}


# perlpodspec equates an over/back region with no items to a blockquote.
sub start_over_block {
  # NOTE: We don't actually need to indent for a blockquote.
  $_[0]->_new_stack;
}

sub   end_over_block {
  my ($self) = @_;

  # Chomp first to avoid prefixing a blank line with a `>`.
  my $text = $self->_chomp_all($self->_pop_stack_text);

  # NOTE: Paragraphs will already be escaped.

  # I don't really like either of these implementations
  # but the join/map/split seems a little better and benches a little faster.
  # You would lose the last newline but we've already chomped.
  #$text =~ s{^(.)?}{'>' . (defined($1) && length($1) ? (' ' . $1) : '')}mge;
  $text = join $/, map { length($_) ? '> ' . $_ : '>' } split qr-$/-, $text;

  $self->_save_block($text);
}

## Custom Formats ##

sub start_for {
  my ($self, $attr) = @_;
  $self->_new_stack;

  if( $attr->{target} eq 'html' ){
    # Use another stack so we can indent
    # (not syntactily necessary but seems appropriate).
    $self->_new_stack;
    $self->_increase_indent;
    $self->_private->{no_escape} = 1;
    # Mark this so we know to undo it.
    $self->_stack_state->{for_html} = 1;
  }
}

sub end_for {
  my ($self) = @_;
  # Data gets saved as a block (which will handle indents),
  # but if there was html we'll alter this, so chomp and save a block again.
  my $text = $self->_chomp_all($self->_pop_stack_text);

  if( $self->_private->{last_state}->{for_html} ){
    $self->_private->{no_escape} = 0;
    # Save it to the next stack up so we can pop it again (we made two stacks).
    $self->_save($text);
    $self->_decrease_indent;
    $text = join "\n", '<div>', $self->_chomp_all($self->_pop_stack_text), '</div>';
  }

  $self->_save_block($text);
}

# Data events will be emitted for any formatted regions that have been enabled
# (by default, `markdown` and `html`).

sub start_Data {
  my ($self) = @_;
  # TODO: limit this to what's in attr?
  $self->_private->{no_escape}++;
  $self->_new_stack;
}

sub   end_Data {
  my ($self) = @_;
  my $text = $self->_pop_stack_text;
  $self->_private->{no_escape}--;
  $self->_save_block($text);
}

## Codes ##

sub start_B { $_[0]->_save('**') }
sub   end_B { $_[0]->start_B()   }

sub start_I { $_[0]->_save('_') }
sub   end_I { $_[0]->start_I()  }

sub start_C {
  my ($self) = @_;
  $self->_new_stack;
  $self->_private->{no_escape}++;
}

sub   end_C {
  my ($self) = @_;
  $self->_private->{no_escape}--;
  $self->_save( $self->_wrap_code_span($self->_pop_stack_text) );
}

# Use code spans for F<>.
sub start_F { shift->start_C(@_); }
sub   end_F { shift  ->end_C(@_); }

sub start_L {
  my ($self, $flags) = @_;
  $self->_new_stack;
  push @{ $self->_private->{link} }, $flags;
}

sub   end_L {
  my ($self) = @_;
  my $flags = pop @{ $self->_private->{link} }
    or die 'Invalid state: link end with no link start';

  my ($type, $to, $section) = @{$flags}{qw( type to section )};

  my $url = (
    $type eq 'url' ? $to
      : $type eq 'man' ? $self->format_man_url($to, $section)
      : $type eq 'pod' ? $self->format_perldoc_url($to, $section)
      :                  undef
  );

  my $text = $self->_pop_stack_text;

  # NOTE: I don't think the perlpodspec says what to do with L<|blah>
  # but it seems like a blank link text just doesn't make sense
  if( !length($text) ){
    $text =
      $section ?
        $to ? sprintf('"%s" in %s', $section, $to)
        : ('"' . $section . '"')
      : $to;
  }

  # FIXME: What does Pod::Simple::X?HTML do for this?
  # if we don't know how to handle the url just print the pod back out
  if (!$url) {
    $self->_save(sprintf 'L<%s>', $flags->{raw});
    return;
  }

  # In the url we need to escape quotes and parentheses lest markdown
  # break the url (cut it short and/or wrongfully interpret a title).

  # Backslash escapes do not work for the space and quotes.
  # URL-encoding the space is not sufficient
  # (the quotes confuse some parsers and produce invalid html).
  # I've arbitratily chosen HTML encoding to hide them from markdown
  # while mangling the url as litle as possible.
  $url =~ s/([ '"])/sprintf '&#x%x;', ord($1)/ge;

  # We also need to double any backslashes that may be present
  # (lest they be swallowed up) and stop parens from breaking the url.
  $url =~ s/([\\()])/\\$1/g;

  # TODO: put section name in title if not the same as $text
  $self->_save('[' . $text . '](' . $url . ')');
}

sub start_X {
  $_[0]->_new_stack;
}

sub   end_X {
  my ($self) = @_;
  my $text = $self->_pop_stack_text;
  # TODO: mangle $text?
  # TODO: put <a name="$text"> if configured
}

# A code span can be delimited by multiple backticks (and a space)
# similar to pod codes (C<< code >>), so ensure we use a big enough
# delimiter to not have it broken by embedded backticks.
sub _wrap_code_span {
  my ($self, $arg) = @_;
  my $longest = 0;
  while( $arg =~ /([`]+)/g ){
    my $len = length($1);
    $longest = $len if $longest < $len;
  }
  my $delim = '`' x ($longest + 1);
  my $pad = $longest > 0 ? ' ' : '';
  return $delim . $pad . $arg . $pad . $delim;
}

## Link Formatting (TODO: Move this to another module) ##


sub format_man_url {
    my ($self, $to) = @_;
    my ($page, $part) = ($to =~ /^ ([^(]+) (?: \( (\S+) \) )? /x);
    return $self->man_url_prefix . ($part || 1) . '/' . ($page || $to);
}


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
  if ($name) {
    $url = $url_prefix . ($self->escape_url ? URI::Escape::uri_escape($name) : $name);
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


# TODO: simple, pandoc, etc?

sub format_fragment_markdown {
  my ($self, $section) = @_;

  # If this is an internal link (to another section in this doc)
  # we can't be sure what the heading id's will look like
  # (it depends on what is rendering the markdown to html)
  # but we can try to follow popular conventions.

  # http://johnmacfarlane.net/pandoc/demo/example9/pandocs-markdown.html#header-identifiers-in-html-latex-and-context
  #$section =~ s/(?![-_.])[[:punct:]]//g;
  #$section =~ s/\s+/-/g;
  $section =~ s/\W+/-/g;
  $section =~ s/-+$//;
  $section =~ s/^-+//;
  $section = lc $section;
  #$section =~ s/^[^a-z]+//;
  $section ||= 'section';

  return $section;
}


{
  # From Pod::Simple::XHTML 3.28.
  # The strings gets passed through encode_entities() before idify().
  # If we don't do it here the substitutions below won't operate consistently.

  sub format_fragment_pod_simple_xhtml {
    my ($self, $t) = @_;

    # encode_entities {
      # We need to use the defaults in case html_encode_chars has been customized
      # (since the purpose is to match what external sources are doing).

      local $self->_private->{html_encode_chars};
      $t = $self->encode_entities($t);
    # }

    # idify {
      for ($t) {
          s/<[^>]+>//g;            # Strip HTML.
          s/&[^;]+;//g;            # Strip entities.
          s/^\s+//; s/\s+$//;      # Strip white space.
          s/^([^a-zA-Z]+)$/pod$1/; # Prepend "pod" if no valid chars.
          s/^[^a-zA-Z]+//;         # First char must be a letter.
          s/[^-a-zA-Z0-9_:.]+/-/g; # All other chars must be valid.
          s/[-:.]+$//;             # Strip trailing punctuation.
      }
    # }

    return $t;
  }
}


sub format_fragment_pod_simple_html {
  my ($self, $section) = @_;

  # From Pod::Simple::HTML 3.28.

  # section_name_tidy {
    $section =~ s/^\s+//;
    $section =~ s/\s+$//;
    $section =~ tr/ /_/;
    $section =~ tr/\x00-\x1F\x80-\x9F//d if 'A' eq chr(65); # drop crazy characters

    #$section = $self->unicode_escape_url($section);
      # unicode_escape_url {
      $section =~ s/([^\x00-\xFF])/'('.ord($1).')'/eg;
        #  Turn char 1234 into "(1234)"
      # }

    $section = '_' unless length $section;
    return $section;
  # }
}


sub format_fragment_metacpan { shift->format_fragment_pod_simple_xhtml(@_); }
sub format_fragment_sco      { shift->format_fragment_pod_simple_html(@_);  }


sub is_local_module {
  my ($self, $name) = @_;

  return ($name =~ $self->local_module_re);
}

1;

__END__

=pod

=encoding UTF-8

=for :stopwords Marcel Gruenauer Victor Moral Ryan C. Thompson <rct at thompsonclan d0t
org> Aristotle Pagaltzis Randy Stauner ACKNOWLEDGEMENTS html cpan
testmatrix url bugtracker rt cpants kwalitee diff irc mailto metadata
placeholders metacpan

=head1 NAME

Pod::Markdown - Convert POD to Markdown

=head1 VERSION

version 3.400

=for test_synopsis my ($pod_string);

=head1 SYNOPSIS

  # Pod::Simple API is supported.

  # Command line usage: Parse a pod file and print to STDOUT:
  # $ perl -MPod::Markdown -e 'Pod::Markdown->new->filter(@ARGV)' path/to/POD/file > README.md

  # Work with strings:
  my $markdown;
  my $parser = Pod::Markdown->new;
  $parser->output_string(\$markdown);
  $parser->parse_string_document($pod_string);

  # See Pod::Simple docs for more.

=head1 DESCRIPTION

This module uses L<Pod::Simple> to convert POD to Markdown.

Literal characters in Pod that are special in Markdown
(like *asterisks*) are backslash-escaped when appropriate.

By default C<markdown> and C<html> formatted regions are accepted.
Regions of C<markdown> will be passed through unchanged.
Regions of C<html> will be placed inside a C<< E<lt>divE<gt> >> tag
so that markdown characters won't be processed.
Regions of C<:markdown> or C<:html> will be processed as POD and included.
To change which regions are accepted use the L<Pod::Simple> API:

  my $parser = Pod::Markdown->new;
  $parser->unaccept_targets(qw( markdown html ));

=head2 A note on encoding and escaping

The common L<Pod::Simple> API returns a character string.
If you want Pod::Markdown to return encoded octets, there are two attributes
to assist: L</match_encoding> and L</output_encoding>.

When an output encoding is requested any characters that are not valid
for that encoding will be escaped as HTML entities.

This is not 100% safe, however.

Markdown escapes all ampersands inside of code spans, so escaping a character
as an HTML entity inside of a code span will not be correct.
However, with pod's C<S> and C<E> sequences it is possible
to end up with high-bit characters inside of code spans.

So, while C<< output_encoding => 'ascii' >> can work, it is not recommended.
For these reasons (and more), C<UTF-8> is the default, fallback encoding (when one is required).

If you prefer HTML entities over literal characters you can use
L</html_encode_chars> which will only operate outside of code spans (where it is safe).

=head1 METHODS

=head2 new

  Pod::Markdown->new(%options);

The constructor accepts the following named arguments:

=over 4

=item *

C<local_module_url_prefix>

Alters the perldoc urls that are created from C<< LE<lt>E<gt> >> codes
when the module is a "local" module (C<"Local::*"> or C<"Foo_Corp::*"> (see L<perlmodlib>)).

The default is to use C<perldoc_url_prefix>.

=item *

C<local_module_re>

Alternate regular expression for determining "local" modules.
Default is C<< our $LOCAL_MODULE_RE = qr/^(Local::|\w*?_\w*)/ >>.

=item *

C<man_url_prefix>

Alters the man page urls that are created from C<< LE<lt>E<gt> >> codes.

The default is C<http://man.he.net/man>.

=item *

C<perldoc_url_prefix>

Alters the perldoc urls that are created from C<< LE<lt>E<gt> >> codes.
Can be:

=over 4

=item *

C<metacpan> (shortcut for C<https://metacpan.org/pod/>)

=item *

C<sco> (shortcut for C<http://search.cpan.org/perldoc?>)

=item *

any url

=back

The default is C<metacpan>.

    Pod::Markdown->new(perldoc_url_prefix => 'http://localhost/perl/pod');

=item *

C<perldoc_fragment_format>

Alters the format of the url fragment for any C<< LE<lt>E<gt> >> links
that point to a section of an external document (C<< L<name/section> >>).
The default will be chosen according to the destination L</perldoc_url_prefix>.
Alternatively you can specify one of the following:

=over 4

=item *

C<metacpan>

=item *

C<sco>

=item *

C<pod_simple_xhtml>

=item *

C<pod_simple_html>

=item *

A code ref

=back

The code ref can expect to receive two arguments:
the parser object (C<$self>) and the section text.
For convenience the topic variable (C<$_>) is also set to the section text:

  perldoc_fragment_format => sub { s/\W+/-/g; }

=item *

C<markdown_fragment_format>

Alters the format of the url fragment for any C<< LE<lt>E<gt> >> links
that point to an internal section of this document (C<< L</section> >>).

Unfortunately the format of the id attributes produced
by whatever system translates the markdown into html is unknown at the time
the markdown is generated so we do some simple clean up.

B<Note:> C<markdown_fragment_format> and C<perldoc_fragment_format> accept
the same values: a (shortcut to a) method name or a code ref.

=item *

C<include_meta_tags>

Specifies whether or not to print author/title meta tags at the top of the document.
Default is false.

=item *

C<escape_url>

Specifies whether or not to escape URLs.  Default is true.  It is not recommended
to turn this off with an empty local_module_url_prefix, as the resulting local
module URLs can be confused with IPv6 addresses by web browsers.

=back

=head2 html_encode_chars

A string of characters to encode as html entities
(using L<HTML::Entities/encode_entities> if available, falling back to numeric entities if not).

Possible values:

=over 4

=item *

A value of C<1> will use the default set of characters from L<HTML::Entities> (control chars, high-bit chars, and C<< <&>"' >>).

=item *

A false value will disable.

=item *

Any other value is used as a string of characters (like a regular expression character class).

=back

By default this is disabled and literal characters will be in the output stream.
If you specify a desired L</output_encoding> any characters not valid for that encoding will be HTML entity encoded.

B<Note> that Markdown requires ampersands (C<< & >>) and left angle brackets (C<< < >>)
to be entity-encoded if they could otherwise be interpreted as html entities.
If this attribute is configured to encode those characters, they will always be encoded.
If not, the module will make an effort to only encode the ones required,
so there will be less html noise in the output.

=head2 match_encoding

Boolean: If true, use the C<< =encoding >> of the input pod
as the encoding for the output.

If no encoding is specified, L<Pod::Simple> will guess the encoding
if it sees a high-bit character.

If no encoding is guessed (or the specified encoding is unusable),
L</output_encoding> will be used if it was specified.
Otherwise C<UTF-8> will be used.

This attribute is not recommended
but is provided for consistency with other pod converters.

Defaults to false.

=head2 output_encoding

The encoding to use when writing to the output file handle.

If neither this nor L</match_encoding> are specified,
a character string will be returned in whatever L<Pod::Simple> output method you specified.

=head2 local_module_re

Returns the regular expression used to determine local modules.

=head2 local_module_url_prefix

Returns the url prefix in use for local modules.

=head2 man_url_prefix

Returns the url prefix in use for man pages.

=head2 perldoc_url_prefix

Returns the url prefix in use (after resolving shortcuts to urls).

=head2 perldoc_fragment_format

Returns the coderef or format name used to format a url fragment
to a section in an external document.

=head2 markdown_fragment_format

Returns the coderef or format name used to format a url fragment
to an internal section in this document.

=head2 include_meta_tags

Returns the boolean value indicating
whether or not meta tags will be printed.

=head2 escape_url

Returns the boolean value indicating
whether or not URLs should be escaped.

=head2 format_man_url

Used internally to create a url (using L</man_url_prefix>)
from a string like C<man(1)>.

=head2 format_perldoc_url

    # With $name and section being the two parts of L<name/section>.
    my $url = $parser->format_perldoc_url($name, $section);

Used internally to create a url from
the name (of a module or script)
and a possible section (heading).

The format of the url fragment (when pointing to a section in a document)
varies depending on the destination url
so L</perldoc_fragment_format> is used (which can be customized).

If the module name portion of the link is blank
then the section is treated as an internal fragment link
(to a section of the generated markdown document)
and L</markdown_fragment_format> is used (which can be customized).

=head2 format_fragment_markdown

Format url fragment for an internal link
by replacing non-word characters with dashes.

=head2 format_fragment_pod_simple_xhtml

Format url fragment like L<Pod::Simple::XHTML/idify>.

=head2 format_fragment_pod_simple_html

Format url fragment like L<Pod::Simple::HTML/section_name_tidy>.

=head2 format_fragment_metacpan

Format fragment for L<metacpan.org>
(uses L</format_fragment_pod_simple_xhtml>).

=head2 format_fragment_sco

Format fragment for L<search.cpan.org>
(uses L</format_fragment_pod_simple_html>).

=head2 is_local_module

Uses C<local_module_re> to determine if passed module is a "local" module.

=for Pod::Coverage parse_from_file
parse_from_filehandle

=for Pod::Coverage as_markdown

=for Pod::Coverage handle_text
end_.+
start_.+
encode_entities

=head1 SEE ALSO

=over 4

=item *

L<pod2markdown> - script included for command line usage

=item *

L<Pod::Simple> - Super class that handles Pod parsing

=item *

L<perlpod> - For writing POD

=item *

L<perlpodspec> - For parsing POD

=item *

L<http://daringfireball.net/projects/markdown/syntax> - Markdown spec

=back

=head1 SUPPORT

=head2 Perldoc

You can find documentation for this module with the perldoc command.

  perldoc Pod::Markdown

=head2 Websites

The following websites have more information about this module, and may be of help to you. As always,
in addition to those websites please use your favorite search engine to discover more resources.

=over 4

=item *

MetaCPAN

A modern, open-source CPAN search engine, useful to view POD in HTML format.

L<https://metacpan.org/release/Pod-Markdown>

=back

=head2 Bugs / Feature Requests

Please report any bugs or feature requests by email to C<bug-pod-markdown at rt.cpan.org>, or through
the web interface at L<https://rt.cpan.org/Public/Bug/Report.html?Queue=Pod-Markdown>. You will be automatically notified of any
progress on the request by the system.

=head2 Source Code


L<https://github.com/rwstauner/Pod-Markdown>

  git clone https://github.com/rwstauner/Pod-Markdown.git

=head1 AUTHORS

=over 4

=item *

Marcel Gruenauer <marcel@cpan.org>

=item *

Victor Moral <victor@taquiones.net>

=item *

Ryan C. Thompson <rct at thompsonclan d0t org>

=item *

Aristotle Pagaltzis <pagaltzis@gmx.de>

=item *

Randy Stauner <rwstauner@cpan.org>

=back

=head1 CONTRIBUTORS

=for stopwords Aristotle Pagaltzis Cindy Wang (CindyLinz) Graham Ollis Johannes Schauer Marin Rodrigues Mike Covington motemen moznion Peter Vereshagin Ryan C. Thompson Yasutaka ATARASHI

=over 4

=item *

Aristotle Pagaltzis <aristotle@cpan.org>

=item *

Cindy Wang (CindyLinz) <cindylinz@gmail.com>

=item *

Graham Ollis <plicease@cpan.org>

=item *

Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>

=item *

Mike Covington <mfcovington@gmail.com>

=item *

motemen <motemen@cpan.org>

=item *

moznion <moznion@cpan.org>

=item *

Peter Vereshagin <veresc@cpan.org>

=item *

Ryan C. Thompson <rthompson@cpan.org>

=item *

Yasutaka ATARASHI <yakex@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Randy Stauner.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
