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

