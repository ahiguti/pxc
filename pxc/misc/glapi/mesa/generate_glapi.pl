#!/usr/bin/perl

use strict;
use warnings;

my $cond_level = $ARGV[0] || 1;
my $verbose = $ARGV[1] || 0;

my ($rettype, $fsym, $argstr) = ('', '', '');
my $cond_count = 0;
while (my $line = <STDIN>) {
  chomp($line);
  if ($cond_count > 0) {
    if ($line =~ /#endif/) {
      --$cond_count;
      print "COND_END $cond_count $line\n" if $verbose;
      next;
    }
  }
  if ($line =~ /#if/) {
    ++$cond_count;
    print "COND $cond_count $line\n" if $verbose;
    next;
  }
  if ($cond_count > $cond_level) {
    next;
  }
  if ($fsym) { # cont'd line
    $argstr .= $line;
    if ($argstr =~ /(.+)\);/) {
      $argstr = $1;
      found_line();
    }
  } elsif ($line =~ /^GLAPI\s+(.+)(GL)?APIENTRY\s+(\w+)\s*\(\s*(.*)/) {
    ($rettype, $fsym, $argstr) = ($1, $3, $4);
    $rettype =~ s/GL$//;
    $rettype =~ s/\s+$//;
    if ($argstr =~ /(.+)\);/) {
      $argstr = $1;
      found_line();
    }
  } elsif ($line =~ /^GLAPI/) {
    print STDERR "UNKNOWN: $line\n";
  }
}

sub found_line {
  # print "$rettype\t$fsym\t$argstr\n";
  my @args = map { arg_type($_); } split(/,/, $argstr);
  $rettype = normalize_type_string($rettype);
  print "$rettype\t$fsym\t" . join("\t", @args) . "\n";
  ($rettype, $fsym, $argstr) = ('', '', '');
}

sub normalize_type_string {
  my $s = $_[0];
  $s =~ s/^\s+//g;
  $s =~ s/\s+$//g;
  $s =~ s/\s+/ /g;
  return $s;
}
sub arg_type {
  my $s = normalize_type_string($_[0]);
  $s =~ s/\s*\w+$//g;
  return $s;
}

