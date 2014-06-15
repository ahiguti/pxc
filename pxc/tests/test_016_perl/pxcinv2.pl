#!/usr/bin/perl

use DynaLoader;

my $pxccmd =
  "../../pxc --no-realpath -w=.pxc -p=../common/pxc_dynamic.profile -g -ne"
  . " ./test2_main.px";
system("$pxccmd") == 0 or die;
my $libref = DynaLoader::dl_load_file("./test2_main.so") or die;
my $symref = DynaLoader::dl_find_symbol($libref, "pxc_library_init") or die;
my $initfn = DynaLoader::dl_install_xsub(undef, $symref) or die;
&$initfn();

my $v1 = test2::plus(10, 20);
print "v1=$v1\n";

my $a = test2::intpair_new(30, 4);
my $b = test2::intpair_new(5, 60);
my $c = test2::intpair_add($a, $b);
my $x = test2::intpair_x($c);
my $y = test2::intpair_y($c);
print "x=$x y=$y\n";

my $z = test2::fib(30);
print "z=$z\n";

