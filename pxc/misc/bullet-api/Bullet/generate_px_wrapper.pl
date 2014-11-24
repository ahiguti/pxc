#!/usr/bin/env perl

use strict;
use warnings;

use XML::Simple;
use Data::Dumper;
use IO::File;

my $xml_filename = "bullet-api.xml";
my $px_filename = "api.px";
my $filter_path = '/usr/include/bullet/';
my $filter_name = '(^bt|^Bvh|^Quantized|^PHY_|^IndexedMeshArray|^size_type)';

my %fundamental_typemap = (
  'int' => 'int',
  'long int' => 'long',
  'long long int' => 'longlong',
  'short int' => 'short',
  'char' => 'char',
  'unsigned int' => 'uint',
  'long unsigned int' => 'ulong',
  'long long unsigned int' => 'ulonglong',
  'short unsigned int' => 'ushort',
  'unsigned char' => 'uchar',
  'float' => 'float',
  'double' => 'double',
  'void' => 'void',
  'bool' => 'bool',
  'size_t' => 'size_t',
);

my %px_keywords = (
  'expand' => 1,
  'function' => 1,
  'import' => 1,
  'metafunction' => 1,
  'threaded' => 1,
  'multithreaded' => 1,
  'valuetype' => 1,
  'mtvaluetype' => 1,
  'tsvaluetype' => 1,
);

my %id_to_ent = ();
my %defined_types = ();
my %used_types = ();
my %pxsymbol_count = ();

my @global_functions = ();
my @method_wrappers = ();
my @function_wrappers = ();
my $out_types = '';

sub convert_keyword
{
  my $s = $_[0];
  if ($px_keywords{$s}) {
    return $s . "_";
  } else {
    return $s;
  }
}

sub export_name_filter
{
  my $s = $_[0];
  if (defined($s) && defined($filter_name)) {
    if ($s =~ $filter_name) {
      return 1;
    }
  }
  return 0;
}

sub get_namespace
{
  my $ent = $_[0];
  my $r = '';
  while (1) {
    my $ctx = $ent->{context};
    last if (!$ctx);
    $ent = $id_to_ent{$ctx};
    if ($ent->{sort} eq 'Namespace') {
      $r = get_cname($ent) . $r;
    }
  }
  return $r;
}

sub px_type_str_enc
{
  my $s = $_[0];
  if ($s =~ /^(\w+)\{(.*)\}$/) {
    my $w = $1;
    my $c = $2;
    my $post = "_$w";
    if ($w eq 'rptr') {
      $post = "_p";
    } elsif ($w eq 'crptr') {
      $post = "_cp";
    }
    return px_type_str_enc($c) . $post;
  }
  return $s;
}

sub middle_funcname
{
  my $name = $_[0];
  my $args = to_array($_[1]);
  my $sz = scalar(@$args);
  return ($name =~ /\d$/) ? "${name}_$sz" : "$name$sz";
}

sub long_funcname
{
  my $name = $_[0];
  my $args = to_array($_[1]);
  my $sz = scalar(@$args);
  for (my $i = 0; $i < $sz; ++$i) {
    my $ent = $args->[$i];
    my ($typ, $passby) = extract_passby($ent->{type});
    my $pxtyp = px_type_str_enc(px_type_str($typ));
    # $pxtyp =~ s/[\{\}]/_/g;
    $name .= "_$pxtyp";
  }
  return $name;
}

sub args_str
{
  my $args = to_array($_[0]);
  my $sz = scalar(@$args);
  my $r = '';
  for (my $i = 0; $i < $sz; ++$i) {
    my $ent = $args->[$i];
    if ($r ne '') {
      $r .= ', ';
    }
    my $argname = get_pxname($ent);
    if ($argname eq '') {
      $argname = "_$i";
    }
    $r .= passinfo_str($ent->{type}) . " " . $argname;
  }
  return $r;
}

sub dump_function
{
  my $ent = $_[0];
  my $rt = passinfo_str($ent->{returns}, "");
  my $ns = get_namespace($ent);
  my $fname = $ent->{name};
  my $fname_px = convert_keyword($fname);
  my $cn = "$ns$fname";
  my $func_ent = [$fname_px, $ent->{returns}, $ent->{Argument}, $cn, ''];
  push(@global_functions, $func_ent);
}

sub dump_enum
{
  my $ent = $_[0];
  my $name = get_pxname($ent);
  my $cname = get_cname($ent);
  my $ns = get_namespace($ent);
  $out_types .=
    qq{public tsvaluetype struct extern "$ns$cname" "extenum" $name { }\n};
  if ($ent->{EnumValue}) {
    my $arr = to_array($ent->{EnumValue});
    for my $ev (@$arr) {
      eval {
	my $e_name = $ev->{name};
	my $e_name_px = convert_keyword($e_name);
	$out_types .= qq{public extern "$e_name" $name $e_name_px;\n};
      };
      if ($@) {
	format_error();
	$out_types .= qq{/* skip enum $name : $@ */\n};
      }
    }
  }
  $pxsymbol_count{$name} = 1;
  $defined_types{$ent->{id}} = 1;
}

sub dump_typedef
{
  my $ent = $_[0];
  # return if !$ent->{name} || $ent->{name} =~ /\</;
  return if (defined($ent->{access}) && $ent->{access} ne 'public');
  my $name = get_pxname($ent);
  eval {
    my $ts = px_type_str($ent->{type});
    if ($ts ne $name) {
      $out_types .= "public metafunction $name $ts;\n";
    }
    $pxsymbol_count{$name} = 1;
    $defined_types{$ent->{id}} = 1;
  };
  if ($@) {
    format_error();
    $out_types .= "/* skip typedef $name : $@ */\n";
  }
}

sub get_cname
{
  my $ent = $_[0];
  my $name = $ent->{demangled} || $ent->{name};
  die if !$name;
  return $name;
}

sub get_pxname
{
  my $ent = $_[0];
  my $name = $ent->{demangled} || $ent->{name} || '';
  $name =~ s/\:\:/_/g;
  $name =~ s/[\*]/_p/g;
  $name =~ s/[\< \,]/_/g;
  $name =~ s/[\>\.]//g;
  $name =~ s/_+/_/g;
  return convert_keyword($name);
}

sub ent_is_public
{
  my $ent = $_[0];
  return 0 if (defined($ent->{access}) && $ent->{access} ne 'public');
  if ($ent->{context}) {
    return ent_is_public($id_to_ent{$ent->{context}});
  }
  return 1;
}

sub get_base_recursive
{
  my $ent = $_[0];
  my $base = to_array($ent->{Base});
  my @base_types = ();
  for my $bt (@$base) {
    eval {
      my $tid = $bt->{type};
      push(@base_types, [$tid, "$tid"]);
      my $cbase = get_base_recursive($id_to_ent{$tid});
      for my $c (@$cbase) {
	$c->[1] .= " $tid";
	push(@base_types, $c);
      }
    };
  }
  return \@base_types;
}

sub get_base_types
{
  my @arr = ();
  my $base_types = get_base_recursive($_[0]);
  my %tid_to_paths = ();
  for my $bt (@$base_types) {
    my ($tid, $path) = @$bt;
    $tid_to_paths{$tid} ||= [];
    push(@{$tid_to_paths{$tid}}, $path);
  }
  while (my ($k, $v) = each(%tid_to_paths)) {
    if (scalar(@$v) == 1) {
      eval {
	my $s = px_type_str($k);
	push(@arr, $k);
      };
    } else {
      # TODO: virtual base class
    }
  }
  return \@arr;
  # if (scalar(@arr) != 0) { return '{' . join(', ', @arr) . '}'; }
  # return '';
}

sub dump_struct_body
{
  my ($ent) = @_;
  my $struct_name = get_pxname($ent);
  my $struct_cname = get_cname($ent);
  my $struct_ns = get_namespace($ent);
  my $is_public = ent_is_public($ent);
  my $defcon = 0;
  my $copycon = 0;
  my $destr = 0;
  my $has_virtual = 0;
  my @constr_list = ();
  my @methods = ();
  my @fields = ();
  my @static_methods = ();
  if ($ent->{members} && $is_public) {
    for my $mid (split(/ /, $ent->{members})) {
      my $m = $id_to_ent{$mid} || die "[$mid]";
      my $msort = $m->{sort};
      eval {
	# print Dumper($m);
	if ($msort eq 'Constructor') {
	  if (!$m->{Argument} && $m->{access} eq 'public') {
	    $defcon = 1;
	    # print "defcon\n";
	  } elsif ($m->{access} eq 'public') {
	    my $is_copycon = 0;
	    my $arr = to_array($m->{Argument});
	    if (scalar(@$arr) == 1 && $m->{access} eq 'public') {
	      my $t0 = $id_to_ent{$arr->[0]->{type}};
	      # print "constr " . Dumper($t0);
	      if ($t0->{sort} eq 'ReferenceType') {
		my $t1 = $id_to_ent{$t0->{type}};
		# print "constr1 " . Dumper($t1);
		if ($t1->{sort} eq 'CvQualifiedType' && $t1->{const} &&
		  $t1->{type} eq $ent->{id}) {
		  $is_copycon = 1;
		  # print "copycon\n";
		}
	      }
	    }
	    if ($is_copycon) {
	      $copycon = 1;
	    } else {
	      push(@constr_list, $m->{Argument});
	    }
	  }
	} elsif ($msort eq 'Destructor') {
	  $destr = 1;
	  # print "destr\n";
	  if ($m->{virtual}) {
	    # print "virtual destr\n";
	  }
	} elsif ($msort eq 'Method') {
	  if ($m->{virtual}) {
	    # print "has_virtual\n" if !$has_virtual;
	    $has_virtual = 1;
	  }
	  if ($m->{access} eq 'public') {
	    my $rt = passinfo_str($m->{returns}, "");
	    my $conststr = $m->{const} ? " const" : "";
	    my $args = args_str($m->{Argument});
	    my $name_c = $m->{name};
	    my $name_px = convert_keyword($name_c);
	    if ($m->{static}) {
	      my $pn = $struct_name . "_" . $name_px;
	      my $cn = "$struct_ns${struct_cname}::$name_c";
	      my $func_ent = [$pn, $m->{returns}, $m->{Argument}, "$cn",
		'"nocdecl"'];
	      push(@static_methods, $func_ent);
	    } else {
	      my $met_ent = [$name_px, $m->{returns}, $m->{Argument},
		$name_c, $m->{const} || 0];
	      push(@methods, $met_ent);
	      # my $si = qq[$rt $name_px($args)$conststr];
	      # $bodystr .= qq{  public function extern "$name_c" $si;\n};
	    }
	  }
	} elsif ($msort eq 'Field') {
	  if ($m->{access} eq 'public') {
	    my $ts = px_type_str($m->{type});
	    my $name = get_pxname($m);
	    my $name_px = convert_keyword($name);
	    if ($ts && $name && $name eq $name_px) {
	      my $fi = passinfo_str($m->{type}, $name);
	      if ($fi !~ /\&/) {
		my $f_ent = [$name_px, $m->{type}, 0, $name, 0];
		push(@fields, $f_ent);
		# $bodystr .= qq{  public $fi;\n};
	      } else {
		# $bodystr .= qq{  /* skip field $name */\n};
	      }
	    }
	  }
	}
      };
      if ($@) {
	format_error();
	# $bodystr .= qq[  /* skip $m->{name} : $@ */\n];
      }
    }
  }
  return +{
    'is_public' => $is_public,
    'struct_name' => $struct_name,
    'struct_cname' => $struct_cname,
    'struct_ns' => $struct_ns,
    'defcon' => $defcon,
    'copycon' => $copycon,
    'destr' => $destr,
    'has_virtual' => $has_virtual,
    'constr_list' => \@constr_list,
    # 'bodystr' => $bodystr,
    'methods' => \@methods,
    'fields' => \@fields,
    'static_methods' => \@static_methods,
  };
}

sub dump_struct
{
  my $ent = $_[0];
  # return if !$ent->{name} || $ent->{name} =~ /\</;
  # return if (defined($ent->{access}) && $ent->{access} ne 'public');
  my @methods = ();
  my @fields = ();
  my $obj = dump_struct_body($ent);
  push(@methods, @{$obj->{methods}});
  push(@fields, @{$obj->{fields}});
  my $bodystr = '';
  my @constr_list = @{$obj->{constr_list}};
  {
    my $tarr = get_base_types($ent);
    if (scalar(@$tarr) != 0) {
      my $basestr = '{' . join(', ', map {px_type_str($_)} @$tarr) . '}';
      $bodystr .= qq[  public metafunction __base__ $basestr;\n];
    }
    for my $tbase (@$tarr) {
      # base must be a struct
      my $base_obj = dump_struct_body($id_to_ent{$tbase});
      push(@methods, @{$base_obj->{methods}});
      push(@fields, @{$base_obj->{fields}});
      # $bodystr .= $base_obj->{bodystr};
    }
  }
#  print STDERR "struct $ent->{name}\n";
  {
    my %sym_map = ();
    for my $mtd (@methods) {
      my ($name_px, $rt, $args, $name_c, $constness) = @$mtd;
      my $name_middle_px = middle_funcname($name_px, $args);
      my $name_long_px = long_funcname($name_px, $args);
      $sym_map{$name_px} ||= +{ };
      my $sym_1 = $sym_map{$name_px};
      $sym_1->{$name_middle_px} ||= +{ };
      my $sym_2 = $sym_1->{$name_middle_px};
      $sym_2->{$name_long_px} ||= +{ };
      my $sym_3 = $sym_2->{$name_long_px};
      $sym_3->{$constness} ||= $mtd;
    }
    while (my ($name_px, $sym_1) = each (%sym_map)) {
      my $mtdlist = [ ];
      push(@method_wrappers, [$obj->{struct_name}, $name_px, $mtdlist]);
      my $need_middle = (scalar(keys %$sym_1) > 1);
      while (my ($name_middle_px, $sym_2) = each (%$sym_1)) {
	my $need_long = (scalar(keys %$sym_2) > 1);
	while (my ($name_long_px, $sym_3) = each (%$sym_2)) {
	  my $need_constness = (scalar(keys %$sym_3) > 1);
	  while (my ($constness, $mtd) = each (%$sym_3)) {
	    my $name =
	      $need_long ? $name_long_px :
	      $need_middle ? $name_middle_px : $name_px;
	    my $pre = ($need_constness && $constness) ? "c" : "";
	    my $conststr = $constness ? " const" : "";
	    my $srt = passinfo_str($mtd->[1], "");
	    my $sargs = args_str($mtd->[2]);
	    my $name_c = $mtd->[3];
	    my $si = qq[$srt $pre$name($sargs)$conststr];
	    $bodystr .= qq{  public function extern "$name_c" $si;\n};
	    my @mtd_m = @$mtd;
	    $mtd_m[0] = $name;
	    push(@$mtdlist, \@mtd_m);
	  }
	}
      }
    }
  }
#  print STDERR "struct $ent->{name} s1\n";
  {
    my %sym_map = ();
    for my $fld (@fields) {
      $sym_map{$fld->[0]} = $fld;
    }
    while (my ($sym, $fld) = each (%sym_map)) {
      my ($name_px, $rt, $args, $name_c, $constness) = @$fld;
      my $fi = passinfo_str($rt, $name_c);
      $bodystr .= qq{  public $fi;\n};
    }
  }
  my $extfamily = '';
  my $constr_str = '';
  my $cn = "$obj->{struct_ns}$obj->{struct_cname}";
  if (!$obj->{is_public}) {
    $extfamily = ' "nodefault"';
    $constr_str = " private() "; # impossible to construct
  } elsif ($obj->{defcon}) {
    if ($obj->{copycon} && !$obj->{has_virtual}) {
      # copyable and defcon
    } else {
      $extfamily = ' "noncopyable"';
    }
  } else {
    $extfamily = ' "nodefault"';
    if (scalar(@constr_list)) {
      my $args0 = args_str($constr_list[0]);
      $constr_str = "($args0) ";
      # shift(@constr_list);
    } else {
      $constr_str = " private() "; # impossible to construct
    }
  }
  for (my $i = 0; $i < scalar(@constr_list); ++$i) {
    my $args = $constr_list[$i];
    # my $pn = $struct_name . $i;
    my $pn = $obj->{struct_name};
    my $cn = "$obj->{struct_ns}$obj->{struct_cname}";
    my $func_ent = [$pn, $ent->{id}, $args, $cn, '"nocdecl"'];
#    print STDERR "aux constr $pn\n";
    push(@global_functions, $func_ent);
  }
  if ($cn =~ '\.') {
    $out_types .= qq[/* skip $obj->{struct_name} : unnamed */\n];
  } else {
    $out_types .= qq[public threaded struct extern];
    $out_types .= qq[ "$cn"$extfamily $obj->{struct_name}$constr_str {\n];
    $out_types .= $bodystr;
    $out_types .= "}\n";
    push(@global_functions, @{$obj->{static_methods}});
# print STDERR "pxsymbol $obj->{struct_name}\n";
    $pxsymbol_count{$obj->{struct_name}} = 1;
    $defined_types{$ent->{id}} = 1;
  }
}

sub format_error
{
  $@ =~ s/ at .*//g;
  $@ =~ s/\n//g;
}

sub to_array
{
  my $m = $_[0];
  if (!$m) {
    return [];
  } elsif (!UNIVERSAL::isa($m, 'ARRAY')) {
    return [ $m ];
  } else {
    return $m;
  }
}

sub passinfo_str
{
  my ($typ, $name) = @_;
  $name ||= '';
  my $passby;
  ($typ, $passby) = extract_passby($typ);
  my $pxtyp = px_type_str($typ);
  my $r = "$pxtyp $passby $name";
  $r =~ s/\s+/ /g;
  $r =~ s/\s+$//g;
  return $r;
}

sub extract_passby
{
  my $typ = $_[0];
  my $byref = 0;
  my $const = 0;
  my $te = $id_to_ent{$typ};
  if ($te->{sort} eq 'ReferenceType') {
    $byref = 1;
    $te = $id_to_ent{$te->{type}};
  }
  if ($te->{sort} eq 'CvQualifiedType') {
    $const = $te->{const};
    $te = $id_to_ent{$te->{type}};
  }
  if ($const) {
    if ($byref) {
      return ($te->{id}, "const&");
    } else {
      return ($te->{id}, "const");
    }
  } else {
    if ($byref) {
      return ($te->{id}, "mutable&");
    } else {
      return ($te->{id}, "");
    }
  }
}

sub px_type_str
{
  my $typ = $_[0];
  my $te = $id_to_ent{$typ};
  my $sort = $te->{sort};
  if ($sort eq 'PointerType') {
    $te = $id_to_ent{$te->{type}};
    my $s = 'rptr';
    if ($te->{sort} eq 'CvQualifiedType') {
      $s = 'crptr' if $te->{const};
      $te = $id_to_ent{$te->{type}};
    }
    return $s . "{" . px_type_str($te->{id}) . "}";
  }
  if ($sort eq 'ReferenceType') {
    $te = $id_to_ent{$te->{type}};
    my $s = 'rawref';
    if ($te->{sort} eq 'CvQualifiedType') {
      $s = 'crawref' if $te->{const};
      $te = $id_to_ent{$te->{type}};
    }
    return $s . "{" . px_type_str($te->{id}) . "}";
  }
  if ($sort eq 'ArrayType') {
    my $sz = $te->{max} || 0;
    if ($sz =~ /^(\d+)u?/) {
      $sz = $1;
    }
    # print "arraytype " . Dumper($te);
    $te = $id_to_ent{$te->{type}};
    my $const = 0;
    if ($te->{sort} eq 'CvQualifiedType') {
      $const = $te->{const};
      # TODO: volatile?
      $te = $id_to_ent{$te->{type}};
    }
    my $tgt = px_type_str($te->{id});
    my $s = $const ? 'crawarray' : 'rawarray';
    $sz += 1;
    return $s . "{" . px_type_str($te->{id}) . ", $sz}";
  }
  if ($sort eq 'FunctionType') {
    my $rt = px_type_str($te->{returns});
    my @args = map { px_type_str($_->{type}) } @{to_array($te->{Argument})};
    return "meta::list{" . $rt . ", " . join(', ', @args) . "}";
  }
  if ($sort eq 'Typedef') {
    my $ft = fundamental_type($te->{name});
    return $ft if $ft; # size_t etc
    $used_types{$te->{id}} = 1;
    die "not defined: $te->{name}" if !$te->{export_px};
    return convert_keyword($te->{name});
  }
  if ($sort eq 'FundamentalType') {
    my $name = $te->{name};
    my $r = fundamental_type($name);
    die "unknown type [$name]" if !defined($r);
    return $r;
  }
  if ($sort eq 'Class' || $sort eq 'Struct' || $sort eq 'Enumeration' ||
    $sort eq 'Union') {
    $used_types{$te->{id}} = 1;
    die "not defined: $te->{name}" if !$te->{export_px};
    return get_pxname($te);
  }
  die "unknown type: " . Dumper($te);
}

sub fundamental_type {
  my $name = $_[0];
  my $r = $fundamental_typemap{$name};
  return $r;
}

# MAIN

my $xml = XMLin($xml_filename, KeyAttr => 'id');

for my $sort (keys %$xml) {
  my $m = $xml->{$sort};
  if (!UNIVERSAL::isa($m, 'HASH')) {
    # print "sort=$sort not a hashref $m\n";
    next;
  }
  # print "$sort\n";
  while (my ($name, $ent) = each(%$m)) {
    my $id = $name; # $ent->{id}; # || $name;
    die if defined $id_to_ent{$id};
    $ent->{sort} = $sort;
    $ent->{id} = $id;
    # print "$id => " . Dumper($ent);
    $id_to_ent{$id} = $ent;
  }
}

while (my ($id, $ent) = each (%id_to_ent)) {
  my $esort = $ent->{sort};
  # if (defined($ent->{access}) && $ent->{access} ne 'public') { next; }
  my $name = get_pxname($ent);
  if (!export_name_filter($name)) {
    next;
  }
  my $fent = $id_to_ent{$ent->{file}} || '';
  if (substr($fent->{name}, 0, length($filter_path)) ne $filter_path) {
    next;
  }
  if ($esort eq 'Class' || $esort eq 'Struct' || $esort eq 'Union') {
    # export even if it's not public
    $ent->{export_px} = 1;
  } elsif ($esort eq 'Typedef' || $esort eq 'Enumeration' ||
    $esort eq 'Function') {
    # export only if it's public
    if (!defined($ent->{access}) || $ent->{access} eq 'public') {
      $ent->{export_px} = 1;
    }
  }
}

for my $id (sort{get_pxname($id_to_ent{$a}) cmp get_pxname($id_to_ent{$b})}
  keys %id_to_ent)
{
  my $ent = $id_to_ent{$id};
  # my $ent = $id_to_ent{$id};
  if (!$ent->{export_px}) {
    next;
  }
  my $esort = $ent->{sort};
  if ($esort eq 'Class' || $esort eq 'Struct' || $esort eq 'Union') {
    dump_struct($ent);
  } elsif ($esort eq 'Typedef') {
    dump_typedef($ent);
  } elsif ($esort eq 'Enumeration') {
    dump_enum($ent);
  } elsif ($esort eq 'Function') {
    dump_function($ent);
  }
}

for my $k (keys %used_types) {
  if ($id_to_ent{$k}->{export_px}) {
    next;
  }
  $out_types .= "/* notdefined [$k] " . Dumper($id_to_ent{$k}) . "*/\n";
}

my $out_fp = new IO::File($px_filename, 'w');

print $out_fp qq{public namespace Bullet::api "export-unsafe";
public import common -;
public import Bullet::base -;
public import container::raw -;
public import meta m;
public import meta::vararg va;
};
print $out_fp $out_types;

{
  my %sym_map = ();
  for my $func (@global_functions)
  {
    my ($name_px, $rt, $args, $name_c, $copt) = @$func;
    my $name_middle_px = middle_funcname($name_px, $args);
    my $name_long_px = long_funcname($name_px, $args);
    $sym_map{$name_px} ||= +{ };
    my $sym_1 = $sym_map{$name_px};
    $sym_1->{$name_middle_px} ||= +{ };
    my $sym_2 = $sym_1->{$name_middle_px};
    $sym_2->{$name_long_px} ||= $func;
# print STDERR "func $name_px $name_middle_px $name_long_px\n";
  }
  while (my ($name_px, $sym_1) = each (%sym_map)) {
    my $funclist = [ ];
    push(@function_wrappers, ['', $name_px, $funclist]);
    my $need_middle = (scalar(keys %$sym_1) > 1);
    while (my ($name_middle_px, $sym_2) = each (%$sym_1)) {
      my $need_long = (scalar(keys %$sym_2) > 1);
      while (my ($name_long_px, $func) = each (%$sym_2)) {
	$need_middle = 1 if $pxsymbol_count{$name_px};
	my $name =
	  $need_long ? $name_long_px :
	  $need_middle ? $name_middle_px : $name_px;
	my $srt = passinfo_str($func->[1], "");
	my $sargs = args_str($func->[2]);
	my $name_c = $func->[3];
	my $copt = $func->[4];
	$copt = " $copt" if $copt;
	my $si = qq[$srt $name($sargs)];
	$si = qq[public threaded function extern "$name_c"$copt $si;];
	print $out_fp "$si\n";
	my @func_m = @$func;
	$func_m[0] = $name;
	push(@$funclist, \@func_m);
      }
    }
  }
}
{
  # generate generic dispatcher functions for overloaded functions/methods
  for my $mw (@method_wrappers, @function_wrappers) {
    my ($struct_name, $mtd_name, $mtdlist) = @$mw;
    # struct_name is empty if it is a non-member function
    if ($struct_name ne '') {
      if (scalar(@$mtdlist) <= 1) {
	next;
      }
    } else {
      # nonzero pxsymbol_count means it's a type constructor
      if (!$pxsymbol_count{$mtd_name} && scalar(@$mtdlist) <= 1) {
	next;
      }
    }
    my $funcname = $struct_name ne '' ? "${struct_name}_$mtd_name" : $mtd_name;
    my $ostr = '';
    $ostr .= qq[private metafunction ${funcname}__rt{tis}\n];
    $ostr .= qq[  m::cond{\n];
    for my $mtd (@$mtdlist) {
      my ($name_px, $rt, $args, $name_c, $constness) = @$mtd;
      my $self_mutable = $constness ? '0' : '1';
      $args = to_array($args);
      my @argcond = ();
      if ($struct_name ne '') {
	push(@argcond, $self_mutable);
      }
      for (my $i = 0; $i < scalar(@$args); ++$i) {
	my ($typid, $passby) = extract_passby($args->[$i]->{type});
	my $typ = px_type_str($typid);
	my $mutref = $passby eq 'mutable&' ? 1 : 0;
	push(@argcond, "{$typ,$mutref}");
      }
      my $matchstr = "match_args{tis, {" . join(',', @argcond) . "}}";
      my $lkup = $struct_name ne '' ? $struct_name : qq{"Bullet::api"};
      $ostr .= qq[    $matchstr,\n];
      $ostr .= qq[    m::ret_type{m::symbol{$lkup, "$name_px"}},\n];
    }
    my $funcname_g = $funcname . ($pxsymbol_count{$funcname} ? '_' : '');
    $ostr .= qq[    m::error{m::concat{"invalid argument: ", tis}}};\n];
    $ostr .= qq[public threaded function {tis}\n];
    $ostr .= qq[${funcname}__rt{tis}\n];
    $ostr .= qq[${funcname_g}(expand(va::arg_decls_byref{tis}))\n];
    $ostr .= qq[{\n];
    my $is_first = 1;
    for my $mtd (@$mtdlist) {
      my ($name_px, $rt, $args, $name_c, $constness) = @$mtd;
      my $self_mutable = $constness ? '0' : '1';
      $args = to_array($args);
      my @argcond = ();
      if ($struct_name ne '') {
	push(@argcond, $self_mutable);
      }
      for (my $i = 0; $i < scalar(@$args); ++$i) {
	my ($typid, $passby) = extract_passby($args->[$i]->{type});
	my $typ = px_type_str($typid);
	my $mutref = $passby eq 'mutable&' ? 1 : 0;
	push(@argcond, "{$typ,$mutref}");
      }
      my $matchstr = "match_args{tis, {" . join(',', @argcond) . "}}";
      if ($is_first) {
	$ostr .= qq[  if ($matchstr) {\n];
      } else {
	$ostr .= qq[  } else if ($matchstr) {\n];
      }
      $is_first = 0;
      my $an = '';
      for (my $i = 0; $i < scalar(@$args); ++$i) {
	if ($i != 0) {
	  $an .= ",";
	}
	if ($struct_name ne '') {
	  $an .= "a" . ($i + 1);
	} else {
	  $an .= "a" . $i;
	}
      }
      if ($struct_name ne '') {
	$ostr .= qq[    return a0.$name_px($an);\n];
      } else {
	$ostr .= qq[    return $name_px($an);\n];
      }
    }
    $ostr .= qq[  }\n];
    $ostr .= qq[}\n];
    print $out_fp $ostr;
  }
}

