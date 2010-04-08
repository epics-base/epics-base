#!/usr/bin/perl

use Test::More tests => 2;

use DBD::Base;

is unquote('"x"'), 'x', '"unquote"';
isnt unquote('x""'), 'x', 'unquote""';
