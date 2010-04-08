#!/usr/bin/perl

use Test::More tests => 2;

use DBD::Util;

is unquote('"x"'), 'x', '"unquote"';
isnt unquote('x""'), 'x', 'unquote""';
