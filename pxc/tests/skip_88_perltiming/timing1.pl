#!/usr/bin/perl
use DynaLoader;
use phoge;

my $pxccmd =
  "../../pxc --no-realpath -w=.pxc -p=../common/pxc_dynamic.profile -g -ne"
  . " ./test1_main.px";
system("$pxccmd") == 0 or die;
my $libref = DynaLoader::dl_load_file("./test1_main.so") or die;
my $symref = DynaLoader::dl_find_symbol($libref, "pxc_library_init") or die;
my $initfn = DynaLoader::dl_install_xsub(undef, $symref) or die;
&$initfn();

sub test_hoge {
  my $o = $_[0];
  my $len = $_[1];
  phoge::hoge_resize_w($o, $len);
  for (my $i = 0; $i < $len; ++$i) {
    phoge::hoge_set_w($o, $i, $i * 2);
  }
  my $v = 0;
  for (my $i = 0; $i < $len; ++$i) {
    for (my $j = 0; $j < $i; ++$j) {
      $v += phoge::hoge_get_w($o, $j);
    }
  }
  return $v;
}

my $o = phoge::new();
for (my $i = 0; $i < 500; ++$i) {
  my $v = test_hoge($o, $i);
  print "$i $v\n" if ($i % 100 == 99);
}

