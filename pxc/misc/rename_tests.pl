#!/usr/bin/env perl
use strict;
use warnings;

use IO::Dir;
my $d = IO::Dir->new(".");
my @names = ();
while (my $n = $d->read) {
  if ($n =~ /^(\w+)_(\d+)_(\w+)$/) {
    push(@names, [$1, $2, $3]);
    print "$n\n";
  }
}
for my $e (@names) {
  my $n = $e->[0] . "_" . $e->[1] . "_" . $e->[2];
  my $nn = $e->[0] . "_0" . $e->[1] . "_" . $e->[2];
  rename($n, $nn);
}
