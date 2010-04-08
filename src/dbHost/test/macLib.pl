#!/usr/bin/perl

use Test::More tests => 34;

use macLib;

use Data::Dumper;

my $m = macLib->new;
isa_ok $m, 'macLib';
is $m->expandString(''), '', 'Empty string';
is $m->expandString('$(undef)'), undef, 'Warning $(undef)';

$m->suppressWarning(1);
is $m->expandString('$(undef)'), '$(undef)', 'Suppressed $(undef)';

$m->putValue('a', 'foo');
is $m->expandString('$(a)'), 'foo', '$(a)';
is $m->expandString('${a}'), 'foo', '${a}';
is $m->expandString('$(a=bar)'), 'foo', '$(a=bar)';
is $m->expandString('${a=bar}'), 'foo', '${a=bar}';
is $m->expandString('$(undef)'), '$(undef)', '$(undef) again';
is $m->expandString('${undef}'), '$(undef)', '${undef} again';

$m->suppressWarning(0);
is $m->expandString('$(undef=$(a))'), 'foo', '$(undef=$(a))';
is $m->expandString('${undef=${a}}'), 'foo', '${undef=${a}}';
is $m->expandString('${undef=$(a)}'), 'foo', '${undef=$(a)}';
is $m->expandString('$(undef=${a})'), 'foo', '$(undef=${a})';
is $m->expandString('$(a=$(undef))'), 'foo', '$(a=$(undef))';

$m->putValue('b', 'baz');
is $m->expandString('$(b)'), 'baz', '$(b)';
is $m->expandString('$(a)'), 'foo', '$(a)';
is $m->expandString('$(a)$(b)'), 'foobaz', '$(a)$(b)';
is $m->expandString('$(a)/$(b)'), 'foo/baz', '$(a)/$(b)';
is $m->expandString('$(a)\$(b)'), 'foo\$(b)', '$(a)\$(b)';
is $m->expandString('$(a)$$(b)'), 'foo$baz', '$(a)$$(b)';

$m->putValue('c', '$(a)');
is $m->expandString('$(c)'), 'foo', '$(c)';
is $m->expandString('$(undef=$(c))'), 'foo', '$(undef=$(c))';

$m->putValue('d', 'c');
is $m->expandString('$(d)'), 'c', '$(d)';
is $m->expandString('$($(d))'), 'foo', '$($(d))';
is $m->expandString('$($(b)=$(a))'), 'foo', '$($(b)=$(a))';

$m->suppressWarning(1);
$m->putValue('c', undef);
is $m->expandString('$(c)'), '$(c)', '$(c) deleted';

$m->installMacros('c=fum,d');
is $m->expandString('$(c)'), 'fum', 'installMacros, $(c)';

is $m->expandString('$(d)'), '$(d)', 'installMacros deletion';

$m->pushScope;
is $m->expandString('$(a)'), 'foo', 'pushScope, $(a)';
$m->putValue('a', 'grinch');
is $m->expandString('$(a)'), 'grinch', 'new $(a) in child';

$m->putValue('b', undef);
is $m->expandString('$(b)'), '$(b)', '$(b) deleted in child';

$m->popScope;
is $m->expandString('$(a)'), 'foo', 'popScope, $(a) restored';
is $m->expandString('$(b)'), 'baz', '$(b) restored';

