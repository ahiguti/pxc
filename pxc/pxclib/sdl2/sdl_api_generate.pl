#!/usr/bin/env perl
use strict;
use warnings;
use IO::File;

my $SDL_INCLUDE_PATH = $ENV{"SDL_INCLUDE_PATH"}
#  || "/cygdrive/c/build/SDL2/include:/cygdrive/c/build/SDL2_image:/cygdrive/c/build/SDL2_ttf";
#  || "/opt/SDL/include:/opt/SDL_image:/opt/SDL_ttf";
#  || "/opt/SDL2/include:/opt/SDL2_image:/opt/SDL2_ttf";
  || "/usr/include/SDL2";
my $SDL_NAMESPACE = $ENV{"SDL_NAMESPACE"} || "sdl2";

# tested: 2.0.20 ubuntu
# tested: 2.28.0 cygwin

my %type_ucount = ();
my %struct_defs = ();
my %enum_defs = ();
my %function_defs = ();
my %type_defs = ();
my @unnamed_enum_defs = ();
my %macro_defs = ();
my %defined_types = ();
my %integral_types = ();

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
  $type_ucount{$a} ||= 0; $type_ucount{$a} += 1;
  for my $t (split(/ /, $a)) {
    $type_ucount{$t} ||= 0; $type_ucount{$t} += 1;
  }
  return $a;
}

sub parse_arg {
  my $a = drop_space($_[0]);
  if ($a =~ /^(.+\W)(\w+)\s*\[\s*(\d+)\s*\]$/) {
    my $t = $1 . '[' . $3 . ']';
    return [ normalize_type($t), drop_space($2) ];
  } elsif ($a =~ /^(.+\W)(\w+)\s*\[\s*(\w+)\s*\]$/) {
    my $t = $1 . '[]';
    return [ normalize_type($t), drop_space($2) ];
  } elsif ($a =~ /^(.+\W)(\w+)$/) {
    return [ normalize_type($1), drop_space($2) ];
  } else {
    return [ normalize_type($a), '' ];
    # die "failed to parse argdecl: [$a]\n";
  }
}

sub drop_comments {
  my $lines = $_[0];
  my $nlines = scalar(@$lines);
  my $comment_flag = 0;
  for (my $i = 0; $i < $nlines; ++$i) {
    my $line = $lines->[$i];
    # print "$comment_flag LINE $line\n";
    my $prev_char = '';
    my $len = length($line);
    for (my $j = 0; $j < $len; ++$j) {
      my $char = substr($line, $j, 1);
      if ($comment_flag) {
	substr($lines->[$i], $j, 1) = ' ';
	if ($prev_char eq '*' && $char eq '/') {
	  # print "END COMMENT $line\n";
	  $comment_flag = 0;
	}
      } else {
	if ($prev_char eq '/' && $char eq '*') {
	  # print "COMMENT $line\n";
	  $comment_flag = 1;
	  substr($lines->[$i], $j - 1, 2) = "  ";
	}
      }
      $prev_char = $char;
    }
    my $l = $lines->[$i];
    # print "MOD LINE $l\n";
  }
}

sub push_field_if {
  my ($aref, $v) = @_;
  # if ($v =~ /\(/ || $v =~ /\[/) {
  if ($v =~ /\(/) {
    # print "  SKIP $v\n";
  } elsif ($v =~ /([^,]+),(.+)/) {
    # int x, y, z;
    my ($a, $rem) = ($1, $2);
    $a =~ /(.+\W)(\w+)/ or die;
    my ($ts, $first) = ($1, $2);
    $ts = normalize_type($ts);
    $first = drop_space($first);
    push(@$aref, [$ts, $first]);
    for my $e (split(/,/, $rem)) {
      if ($e =~ '\[' || $e =~ '\*') {
	# int x, y[10] --- not supported
	next;
      }
      push(@$aref, [$ts, drop_space($e)]);
    }
  } else {
    my $e = parse_arg($v);
    if ($e->[1] eq 'function') {
      # 'function' is a reserved word
    } else {
      push(@$aref, $e);
    }
  }
}

sub pxc_type_string {
  my $tstr = $_[0];
  my @a = split(/ /, $tstr);
  my $r = '';
  my $const_flag = 0;
  for (my $i = 0; $i < scalar(@a); ++$i) {
    my $s = $a[$i];
    if ($s eq '*') {
      $r = ($r eq 'char')
	? $const_flag ? "ccharptr" : "charptr"
	: $const_flag ? "crawptr{$r}" : "rawptr{$r}";
      $const_flag = 0;
    } elsif ($s =~ /\[(\d+)\]/) {
      $r = "rawarray{$r, $1}";
      die "rawarray type: $r";
      $const_flag = 0;
    } elsif ($s =~ /\[\]/) {
      $r = "rawarray{$r, 0}";
      $const_flag = 0;
    } elsif ($s eq 'const') {
      $const_flag = 1;
    } elsif ($s eq 'struct') {
      ;
    } elsif ($r eq '' || $r eq 'unsigned') {
      $r = $r eq '' ? $s : "u$s";
      if (!defined($defined_types{$r})) {
	die "unknown type: [$r]";
      }
    } else {
      die "invalid type: [$tstr]";
    }
  }
  die "invalid type: [$tstr]" if (!$r);
  return $r;
}

# READ

use IO::Dir;
for my $ipath (split(/:/, $SDL_INCLUDE_PATH)) {
  # print "IPATH $ipath\n";
  my $d = new IO::Dir($ipath) or die;
  while (1) {
    my $de = $d->read();
    last if !defined($de);
    if ($de !~ /.+\.h$/) {
      next;
    }
    #if ($de =~ /SDL_stdinc\.h$/) {
    #  next;
    #}
    print "reading $ipath/$de\n";
    read_header_file("$ipath/$de");
  }
}

sub read_header_file {
  my $fn = $_[0]; # "$SDL_INCLUDE_PATH/$de";
  my $f = new IO::File($fn);
#for my $fn (@ARGV) {
#  my $f = new IO::File($fn);
  my @lines = ();
  while (my $line = <$f>) {
    chomp($line);
    $line = ' ' if !$line;
    # print "L: $line\n";
    push(@lines, $line);
  }
  drop_comments(\@lines);
  my $nlines = scalar(@lines);
  # print "nlines = $nlines\n";
  for (my $i = 0; $i < $nlines; ++$i) {
    my $line = $lines[$i];
    # print "L: $i $line\n";
    if ($line =~ /extern\s+DECLSPEC\s+(.+)SDLCALL\s+(.+)/) {
      my ($rettype, $proto) = ($1, $2);
      $rettype = normalize_type($rettype);
      if ($proto !~ /;/) {
	while (1) {
	  my $cline = $lines[++$i] or die "FUNCDEF $proto";
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
	  # print "SKIP [$proto]\n";
	} else {
	  # print "[$rettype], [$funcname], [$args_str]\n";
	  my @args = map { parse_arg($_) } split(/,/, $args_str);
	  # print "  ARG " . join(',', map { join(':', @$_) } @args) . "\n";
	  if (scalar(@args) == 1 && $args[0]->[0] eq 'void') {
	    @args = ();
	  }
	  for (my $i = 0; $i < scalar(@args); ++$i) {
	    my $e = $args[$i];
	    if ($e->[1] eq '') {
	      $e->[1] = "_$i";
	    }
	  }
	  $function_defs{$funcname} = [$rettype, \@args];
	}
	print "FUNCDECL [$proto]\n";
      } else {
	print "can not parse funcdecl: [$proto]\n";
      }
    # } elsif ($line =~ /typedef\s+(struct|union)\s*([^\{\;]+)\s*;/) {
    } elsif ($line =~ /typedef\s+([\w\s\*]+);/) {
      # typedef without struct body
      my $p = parse_arg($1);
      my ($tds, $tdn) = ($p->[0], $p->[1]);
      print "TYPEDEF STRUCT [$tds] [$tdn]\n";
      $type_defs{$tdn} = [$tds];
      $defined_types{$tdn} |= 1;
    } elsif ($line !~ /\#define/ && $line =~ /typedef\s+(struct|union)/) {
      # typedef with struct body
      my @vals = ();
      my $cline = $line;
      while (1) {
	if ($cline =~ /\{/) {
	  last;
	}
	$cline = $lines[++$i] or return; # die "STRUCT $line";
      }
      my $brace_cnt = 0;
      while (1) {
	if ($cline =~ /\}/) {
	  if ($brace_cnt <= 1) {
	    if ($cline =~ /\{\s*([^\;\}]+)\s*;\s*\}/) {
	      # single line struct definition
	      push_field_if(\@vals, $1);
	    }
	    last;
	  } else {
	    --$brace_cnt;
	  }
	} elsif ($brace_cnt == 1 && $cline =~ /^\s*([^\;\)]+);\s*$/) {
	  # print "FIELD [$1] $cline\n";
	  push_field_if(\@vals, $1);
	} elsif ($cline =~ /\{/) {
	  ++$brace_cnt;
	} else {
	  # print "STRUCT content skip [$cline] $brace_cnt\n";
	}
	$cline = $lines[++$i] or die "STRUCT $line [$brace_cnt]";
      }
      my $tdn = '';
      if ($cline =~ /\}\s*(.+);/) {
	my @words = split(/\s+/, $1);
	$tdn = $words[-1];
      } else {
	die "can not parse typedef struct: [$cline]\n";
      }
      print "TYPEDEF STRUCT ", join(',', map { join(':', @$_) } @vals)
        . " [$tdn]\n";
      $struct_defs{$tdn} = [$tdn, \@vals];
      $defined_types{$tdn} |= 1;
    } elsif ($line =~ /(typedef\s+)?\benum/) {
      my @vals = ();
      my $cline = $line;
      while (1) {
	if ($cline =~ /\{/) {
	  last;
	}
	# print "FIND { $cline\n";
	$cline = $lines[++$i] or die;
      }
      while (1) {
	# print "FIND } $cline\n";
	if ($cline =~ /\}/) {
	  if ($cline =~ /\{\s*(\w+)\s*\}/) {
	    push(@vals, $1);
	  }
	  last;
	} elsif ($cline =~ /\{/) {
	  ;
	} elsif ($cline =~ /^\s*(\w+)(.*)$/) {
	  my ($w, $rem) = ($1, $2);
	  # die "$fn $i $cline" if ($w eq 'typedef');
	  if ($cline !~ /[\(\)]/ || $rem =~ /=/) {
	    push(@vals, $w);
	  }
	  while (1) {
	    if ($cline =~ /,\s*$/) {
	      last;
	    } elsif ($cline =~ /}/) {
	      --$i;
	      last;
	    }
	    $cline = $lines[++$i] or die "$fn $$i";
	  }
	}
	$cline = $lines[++$i] or die "$line " . join(',', @vals);
      }
      my $tdn = '';
      if ($cline =~ /\}\s*(\w*)/) {
	$tdn = $1;
      } else {
	die "can not parse typedef enum: [$cline] $fn\n";
      }
      {
        # unique
        my %hash = map{$_, 1} @vals;
        @vals = sort keys %hash;
      }
      print "TYPEDEF ENUM {", join(',', @vals), "} [$tdn]\n";
      if ($tdn) {
	$enum_defs{$tdn} = [\@vals];
	$defined_types{$tdn} |= 1;
      } else {
	push(@unnamed_enum_defs, \@vals);
      }
    } elsif ($line =~ /^\s*\#/) {
      print "PREPROCESSOR 0 $line\n";
      if ($line =~ /^\s*\#if/) {
	if ($line =~ /(__WIN32__|__WINRT__|__IPHONEOS__|ANDROID)/) {
	  my $nest = 1;
	  while ($nest > 0) {
	    my $cline = $lines[++$i] or die "endif notfound";
	    if ($cline =~ /^\s*\#endif/) {
	      --$nest;
	    } elsif ($cline =~ /^\s*#if/) {
	      ++$nest;
	    }
	  }
	}
      } elsif ($line =~ /^\s*\#define\s+(\w+)(.*)/) {
	my ($n, $rem) = ($1, $2);
	if ($rem =~ /^\(/) {
	  print "skip macro $line\n";
	} else {
	  print "MACRO $n\n";
	  if ($n =~ /^SDL_INIT_/ || $n =~ /^SDL_WINDOWPOS_/) {
	    $macro_defs{$n} = 1;
	  }
	}
      }
    }
  }
}

#

for my $s (qw{void uint uchar int char ulong long float double unsigned}) {
  $defined_types{$s} |= 1;
}

for my $s (qw{
  int8_t int16_t int32_t int64_t
  Sint8 Sint16 Sint32 Sint64
}) {
  $integral_types{$s} = 'extint'; # signed
}
for my $s (qw{
  uint8_t uint16_t uint32_t uint64_t
  Uint8 Uint16 Uint32 Uint64
  uintptr_t size_t
}) {
  $integral_types{$s} = 'extuint'; # unsigned
}

for my $s (keys %integral_types) {
  delete $type_defs{$s};
  delete $struct_defs{$s};
}

for my $s (qw{
  SDL_GameControllerMappingForGUID
  SDL_MemoryBarrierAcquire
  SDL_MemoryBarrierRelease
}) {
  delete $function_defs{$s};
}

# OUT

my $fpt = new IO::File("api_types.px", "w");
my $fpf = new IO::File("api_functions.px", "w");

print $fpt qq{/* This file is generated by sdl_px_apigen.pl */
public threaded namespace ${SDL_NAMESPACE}::api_types "use-unsafe";
public import core::common -;
private import core::pointer::raw -;
public import core::container::raw -;
public import ${SDL_NAMESPACE}::api_base -;
public import core::meta m;
};
print $fpf qq{/* This file is generated by sdl_px_apigen.pl */
public threaded namespace ${SDL_NAMESPACE}::api_functions "export-unsafe";
public import core::common -;
public import core::pointer::raw -;
public import core::container::raw -;
public import ${SDL_NAMESPACE}::api_base -;
public import ${SDL_NAMESPACE}::api_types -;
public import core::meta m;
};

foreach my $tdn (sort keys %type_defs) {
  my $v = $type_defs{$tdn};
  my $tds = $v->[0];
  if ($tds !~ /\*$/) {
    if ($integral_types{$tds}) {
      # tdn is typedef'ed to an integral type
      my $ext = $integral_types{$tds};
      print $fpt qq[public pure multithreaded struct extern "%" "$ext" $tdn { }\n];
    } else {
      if ($tdn eq "SDL_bool") {
        next;
      }
      # tdn is a struct
      print $fpt qq[public pure multithreaded struct extern "%" $tdn { }\n];
    }
  } else {
    # tdn is a pointer type. define it as extenum so that operator ==
    # is defined.
    print $fpt qq[public pure multithreaded struct extern "%" "extenum" $tdn { }\n];
  }
}

foreach my $tdn (sort keys %struct_defs) {
  my $v = $struct_defs{$tdn};
  print $fpt qq[public pure multithreaded struct extern "%"\n$tdn {\n];
  for my $e (@{$v->[1]}) {
    my $t = '';
    eval {
      $t = pxc_type_string($e->[0]);
    };
    if ($@) {
      print $fpt "  /* public " . $t . " " . $e->[1] . "; */\n";
    } else {
      print $fpt "  public " . $t . " " . $e->[1] . ";\n";
    }
  }
  print $fpt qq[}\n];
}

foreach my $tdn (sort keys %enum_defs) {
  my $v = $enum_defs{$tdn};
  my $is_typed_enum = defined($type_ucount{$tdn});
  my $et = $is_typed_enum ? $tdn : "SDL_Enum";
  if ($is_typed_enum) {
    print $fpt qq[public pure multithreaded struct extern "int" "extenum" $tdn { }\n];
  } else {
    print $fpt qq{/* enum $tdn */\n};
  }
  for my $e (@{$v->[0]}) {
    print $fpt qq{public extern "%" $et $e;\n};
  }
}
for my $v (@unnamed_enum_defs) {
  print $fpt qq{/* unnamed enum */\n};
  for my $e (@$v) {
    print $fpt qq{public extern "%" SDL_Enum $e;\n};
  }
}
for my $v (sort keys %macro_defs) {
  print $fpt qq{public extern "%" SDL_Enum $v; /* macro */\n};
}

my @threaded_funcs = ('^SDL_RW', '^SDL_FreeRW', '^SDL_Surface', '^IMG_Load',
  '^SDL_FreeSurface', '^SDL_CreateRGBSurface', '^SDL_UpperBlit', '^TTF_');

foreach my $fnc (sort keys %function_defs) {
  my $v = $function_defs{$fnc};
  my $retts = ''; 
  my $argstr = '';
  eval {
    $retts = pxc_type_string($v->[0]);
    $argstr = join(', ',
      map { pxc_type_string($_->[0]) . ($_->[1] ? ' ' : '') . $_->[1] }
      @{$v->[1]});
  };
  if ($@) {
    # print "$@";
    print $fpf qq{/* public function extern "%" $retts $fnc($argstr); */\n};
  } else {
    if ($fnc =~ '^SDL_GDK') {
      # コンパイル通らないのでskip
      next;
    }
    my $fattr = 'public';
    foreach my $tf (@threaded_funcs) {
      if ($fnc =~ $tf) {
        $fattr = 'public threaded'
      }
    }
    print $fpf qq{$fattr function extern "%" $retts $fnc($argstr);\n};
  }
}

while (my ($k, $v) = each %type_ucount) {
  # print "TYPE $k $v\n";
}

