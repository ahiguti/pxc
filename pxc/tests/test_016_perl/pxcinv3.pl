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

my $sys = io::system();
# print "$sys\n";
my $f = io::file::O_RDONLY();
# print "$f\n";
my $o = $sys->open_st("hoge.txt", $f, 0);
# print "$o\n";
my $pkg_string = \%{"container::array::varray{meta::uchar}::"};
# print "pkg_string=$pkg_string\n";
my $nstr = *{$pkg_string->{new}}{CODE};
my $s = &$nstr();
use Scalar::Util 'blessed';
print "blessed=" . blessed($s) . "\n";
$s->append("qqqq");
my $s1 = perl::string_value($s);
print "s1=$s1\n";
my $s2 = perl::string_value("aa");
print "s2=$s2\n";

=pod
my $o = test1::hoge::new();
$o->x(123);
my $x = $o->x();
$o->z("xyz");
my $z = $o->z();
print "$x $z\n";

my $pkg_test1 = \%{test1::};
my $f = $pkg_test1->{hoge_get_x};
my $c = *$f{CODE};
# my $c = *{$pkg->{hoge_get_x}}{CODE};
my $x1 = &$c($o);
print "$x1\n";
=cut
