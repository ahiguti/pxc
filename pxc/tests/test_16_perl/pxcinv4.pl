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

sub pxc_string {
  my $pkg_string = \%{"container::array::vector{meta::uchar}::"};
  my $nstr = *{$pkg_string->{new}}{CODE};
  my $s = &$nstr();
  if ($_[0] ne "") {
    $s->append($_[0]);
  }
  return $s;
}

my $fp = io::system()->open("hoge.txt", io::file::O_RDONLY(), 0)->value();
my $buf = pxc_string();
$fp->read_all($buf);
my $str = perl::string_value($buf);
print $str;

