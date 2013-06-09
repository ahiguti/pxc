#!/usr/bin/env perl

use strict;
use warnings;

sub fib {
  my $x = $_[0];
  return $x < 2 ? $x : fib($x - 1) + fib($x - 2);
}

print fib(30) . "\n";

