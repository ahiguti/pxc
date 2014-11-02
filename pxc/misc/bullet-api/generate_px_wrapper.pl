#!/usr/bin/env perl

use strict;
use warnings;

use XML::Simple;
use Data::Dumper;
use IO::File;

my $xml_filename = "out.xml";
my $px_filename = "bullet_api.px";
my $filter_path = '/usr/include/bullet/';
my $filter_name = '(^bt|^Bvh|^Quantized|^PHY_|^IndexedMeshArray|^size_type)';

my %fundamental_typemap = (
  'int' => 'int',
  'long int' => 'int',
  'long long int' => 'clong',
  'short int' => 'short',
  'char' => 'char',
  'unsigned int' => 'uint',
  'long unsigned int' => 'uclong',
  'long long unsigned int' => 'ulong',
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

my @global_functions = ();
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

sub long_funcname
{
  my $name = $_[0];
  my $args = to_array($_[1]);
  my $sz = scalar(@$args);
  for (my $i = 0; $i < $sz; ++$i) {
    my $ent = $args->[$i];
    my ($typ, $passby) = extract_passby($ent->{type});
    my $pxtyp = px_type_str($typ);
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
  my $func_ent = [$fname_px, $rt, $ent->{Argument}, $cn, ''];
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

sub dump_struct
{
  my $ent = $_[0];
  # return if !$ent->{name} || $ent->{name} =~ /\</;
  # return if (defined($ent->{access}) && $ent->{access} ne 'public');
  my $is_public = ent_is_public($ent);
  my $struct_name = get_pxname($ent);
  my $struct_cname = get_cname($ent);
  my $struct_ns = get_namespace($ent);
  my $defcon = 0;
  my $copycon = 0;
  my $destr = 0;
  my $has_virtual = 0;
  # my @memfuncs = ();
  # my @static_memfuncs = ();
  # my @fields = ();
  my @constr_list = ();
  my %method_names = (); # checks overloading
  my $bodystr = '';
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
	      my $args = args_str($m->{Argument});
	      push(@constr_list, $args);
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
	    # my $static = $m->{static}; # TODO
	    my $args = args_str($m->{Argument});
	    my $name_c = $m->{name};
	    my $name_px = convert_keyword($name_c);
	    if ($method_names{$name_px}) {
	      my $v = $method_names{$name_px};
	      $method_names{$name_px} = $v + 1;
	      $name_px .= $v;
	    } else {
	      $method_names{$name_px} = 1;
	    }
	    if ($m->{static}) {
	      my $pn = $struct_name . "_" . $name_px;
	      my $cn = "$struct_ns${struct_cname}::$name_c";
	      my $func_ent = [$pn, $rt, $m->{Argument}, "$cn", '"nocdecl"'];
	      push(@static_methods, $func_ent);
	    } else {
	      my $si = qq[$rt $name_px($args)$conststr];
	      $bodystr .= qq{  public function extern "$name_c" $si;\n};
	    }
	  }
	} elsif ($msort eq 'Field') {
	  if ($m->{access} eq 'public') {
	    my $ts = px_type_str($m->{type});
	    my $name = get_pxname($m);
	    my $name_px = convert_keyword($name);
	    # die Dumper($m) if !$ts;
	    if ($ts && $name && $name eq $name_px) {
	      my $fi = passinfo_str($m->{type}, $name);
	      if ($fi !~ /\&/) {
		$bodystr .= qq{  public $fi;\n};
	      } else {
		$bodystr .= qq{  /* skip field $name */\n};
	      }
	    }
	  }
	}
      };
      if ($@) {
	format_error();
	$bodystr .= qq[  /* skip $m->{name} : $@ */\n];
      }
    }
  }
  {
    my $base = to_array($ent->{Base});
    my @base_types = ();
    for my $bt (@$base) {
      eval {
	my $b = px_type_str($bt->{type});
	push(@base_types, $b);
      };
      if ($@) {
	format_error();
	$bodystr .= qq[  /* skip base : $@ */\n];
      }
    }
    if (scalar(@base_types) != 0) {
      my $bs = '{' . join(', ', @base_types) . '}';
      $bodystr .= qq[  public metafunction __base__ $bs;\n];
    }
  }
  my $extfamily = '';
  my $constr_str = '';
  my $cn = "$struct_ns$struct_cname";
  if (!$is_public) {
    $extfamily = ' "nodefault"';
    $constr_str = " private() "; # impossible to construct
  } elsif ($defcon) {
    if ($copycon && !$has_virtual) {
      # copyable and defcon
    } else {
      $extfamily = ' "noncopyable"';
    }
  } else {
    $extfamily = ' "nodefault"';
    if (scalar(@constr_list)) {
      my $args = $constr_list[0];
      $constr_str = "($args) ";
    } else {
      $constr_str = " private() "; # impossible to construct
    }
  }
  if ($cn =~ '\.') {
    $out_types .= qq[/* skip $struct_name : unnamed */\n];
  } else {
    $out_types .= qq[public threaded struct extern];
    $out_types .= qq[ "$cn"$extfamily $struct_name$constr_str {\n];
    $out_types .= $bodystr;
    $out_types .= "}\n";
    push(@global_functions, @static_methods);
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

print $out_fp qq{public namespace bullet_api "export-unsafe";
public import common -;
public import bullet_base -;
public import container::raw -;
};
print $out_fp $out_types;

my %func_count = ();
for my $func_ent (@global_functions)
{
  $func_count{$func_ent->[0]} ||= 0;
  $func_count{$func_ent->[0]} += 1;
}

for my $func_ent (@global_functions)
{
  my ($name_px, $rt, $args, $name_c, $copt) = @$func_ent;
  if ($func_count{$name_px} > 1) {
    # overloaded
    $name_px = long_funcname($name_px, $args);
  }
  my $sargs = args_str($args);
  $copt = " $copt" if $copt;
  my $si =
    qq[public threaded function extern "$name_c"$copt $rt $name_px($sargs);];
  print $out_fp "$si\n";
}

