#!/usr/bin/env perl
use strict;
use warnings;
use IO::File;

my %type_ucount = ();

for my $fn (@ARGV) {
  my $f = new IO::File($fn);
  my @lines = ();
  while (my $line = <$f>) {
    chomp($line);
    # print "L: $line\n";
    push(@lines, $line);
  }
  my $nlines = scalar(@lines);
  for (my $i = 0; $i < $nlines; ++$i) {
    my $line = $lines[$i];
    if ($line =~ /extern\s+DECLSPEC\s+(\S+)\s+SDLCALL\s+(.+)/) {
      my ($rettype, $proto) = ($1, $2);
      $rettype = normalize_type($rettype);
      if ($proto !~ /;/) {
	while (1) {
	  my $cline = $lines[++$i] or die;
	  $proto .= $cline;
	  if ($cline =~ /;/) {
	    last;
	  }
	}
      }
      $proto =~ s/\n/ /g;
      $proto =~ s/\s+/ /g;
      if ($proto =~ /(\w+)\s*\(([^\)]*)\)/) {
	my ($funcname, $args_str) = ($1, $2);
	if ($args_str =~ /\(/ || $args_str =~ /\./) {
	  print "SKIP [$proto]\n";
	} else {
	  print "[$rettype], [$funcname], [$args_str]\n";
	  my @args = map { parse_arg($_) } split(/,/, $args_str);
	  print "  ARG " . join(',', map { join(':', @$_) } @args) . "\n";
	}
      } else {
	die "failed to parse funcdecl: [$proto]\n";
      }
    } elsif ($line =~ /typedef\s+struct\s*(\w+)\s+(\w+);/) {
      my ($tds, $tdn) = ($1, $2);
      print "TYPEDEF [$tdn] [$tds]\n";
    }
  }
}

while (my ($k, $v) = each %type_ucount) {
  print "TYPE $k $v\n";
}

sub drop_space {
  my $a = $_[0];
  $a =~ s/^\s+//;
  $a =~ s/\s+$//;
  return $a;
}

sub normalize_type {
  my $a = drop_space($_[0]);
  $a =~ s/\*/ * /g;
  $a =~ s/\s+/ /g;
  $a = drop_space($a);
  # map { $type_ucount{$_} ||= 0; $type_ucount{$_} += 1; } split(/ /, $a);
  $type_ucount{$a} ||= 0; $type_ucount{$a} += 1;
  return $a;
}

sub parse_arg {
  my $a = $_[0];
  if ($a =~ /(.+\W)(\w+)/) {
    return [ normalize_type($1), drop_space($2) ];
  } else {
    return [ normalize_type($a), '' ];
    # die "failed to parse argdecl: [$a]\n";
  }
}

