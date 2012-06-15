#!/usr/bin/perl

use PXC::Loader;

my $pxccmd = "../../pxc --no-realpath -w .pxc -p ../common/pxc_dynamic.profile";
PXC::Loader::load($pxccmd, "./test1.px");
my $v1 = test1::f1("abc");
my $v2 = test1::f2(33, "29");
my $v3 = test1::f3(5, 10);
print "v1=$v1 v2=$v2 v3=$v3\n";
# print STDERR "pre f3\n";
# my $v3 = test1::f3(5, 10);
# print STDERR "post f3\n";

my $v4 = test1::f4();
print test1::f5($v4) . "\n";
print test1::f5($v4) . "\n";
print test1::f5($v4) . "\n";
print "done\n";

