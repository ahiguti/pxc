#!/usr/bin/perl
use DynaLoader;

my $pxccmd =
  "../../pxc --no-realpath -w=.pxc -p=../common/pxc_dynamic.profile -g -ne"
  . " ./test1_main.px";
system("$pxccmd") == 0 or die;
my $libref = DynaLoader::dl_load_file("./test1_main.so") or die;
my $symref = DynaLoader::dl_find_symbol($libref, "pxc_library_init") or die;
my $initfn = DynaLoader::dl_install_xsub(undef, $symref) or die;
&$initfn();

my $o = test1::hoge::new();
$o->x(123);
my $x = $o->x();
$o->z("xyz");
my $z = $o->z();
print "$x $z\n";

my $pkg_test1_hoge = \%{test1::hoge::};
my $f = $pkg_test1_hoge->{x};
my $c = *$f{CODE};
# my $c = *{$pkg->{hoge_get_x}}{CODE};
my $x1 = &$c($o);
print "$x1\n";

