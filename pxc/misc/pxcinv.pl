#!/usr/bin/perl

use PXC::Loader;

PXC::Loader::load("pxc", "./perl.px");
hoge::f1();
my $v = hoge::f2(3, 29);
print "v=$v\n";
