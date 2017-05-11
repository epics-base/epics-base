#!/usr/bin/perl

use lib '../..';

use Test::More tests => 35;

use EPICS::macLib;

use Data::Dumper;

my $m = EPICS::macLib->new;
isa_ok $m, 'EPICS::macLib';
is $m->expandString(''), '', 'Empty string';

{
    local *STDERR;
    my $output;
    open STDERR, '>', \$output;
    is $m->expandString('$(undef)'), undef, 'Warning $(undef)';
    chomp $output;
    is $output, q/macLib: macro 'undef' is undefined (expanding string '$(undef)')/, 'macLib error message';
}

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

