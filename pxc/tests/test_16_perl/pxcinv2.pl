#!/usr/bin/perl

use PXC::Loader;

my $pxccmd = "../../pxc --no-realpath -w .pxc -p ../common/pxc_dynamic.profile";

PXC::Loader::load($pxccmd, "./test2.px");

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
