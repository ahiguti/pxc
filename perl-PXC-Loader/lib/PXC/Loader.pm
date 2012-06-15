package PXC::Loader;

# use 5.014002;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use PXC::Loader ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('PXC::Loader', $VERSION);

# Preloaded methods go here.

use IPC::Open3;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Symbol;


END {
    # print STDERR "begin END block\n";
    terminate_dl();
    # print STDERR "end END block\n";
}

sub load {
    local $SIG{'PIPE'} = 'IGNORE';
    my ($pxccmd, $filename) = @_;
    if (!$pxccmd) {
      $pxccmd = "pxc -p pxc_dynamic";
    }
    $pxccmd .= " --show-out $filename";
    my ($h0, $h1, $h2) = (gensym(), gensym(), gensym());
    my $pid = open3($h0, $h1, $h2, $pxccmd);
    eval {
    	_load_internal($h0, $h1, $h2);
    };
    my $err;
    if ($@) {
    	$err = $@;
    }
    close($h0) if $h0;
    close($h1) if $h1;
    close($h2) if $h2;
    waitpid($pid, 0);
    die($err) if defined($err);
}

sub _read_fd {
    my $buf = '';
    my $len = sysread($_[0], $buf, 4096);
    if ($len <= 0) {
	return undef;
    }
    return $buf;
}

sub _set_nonblocking {
    my $h = $_[0];
    my $flags = fcntl($h, F_GETFL, 0) or die;
    fcntl($h, F_SETFL, $flags | O_NONBLOCK) or die;
}

sub _load_internal {
    my ($h0, $h1, $h2) = @_;
    my $out1 = '';
    my $out2 = '';
    my ($filename, $symbol);
    _set_nonblocking($h1) if defined($h1);
    _set_nonblocking($h2) if defined($h2);
    while (defined($h1) || defined($h2)) {
    	my $rin = '';
	vec($rin, fileno($h1), 1) = 1 if defined($h1);
	vec($rin, fileno($h2), 1) = 1 if defined($h2);
	select($rin, undef, undef, 1);
	if (defined($h1) && vec($rin, fileno($h1), 1)) {
	    my $r = _read_fd($h1);
	    if (!defined($r)) {
		$h1 = undef;
	    } else {
		$out1 .= $r;
	    }
	}
	if (vec($rin, fileno($h2), 1)) {
	    my $r = _read_fd($h2);
	    if (!defined($r)) {
		$h2 = undef;
	    } else {
		$out2 .= $r;
	    }
	}
	my $idx = index($out1, "\n");
	if (!defined($filename) && $idx >= 0) {
	    my $s = substr($out1, 0, $idx);
	    $idx = index($s, "\t");
	    if ($idx >= 0) {
	    	$filename = substr($s, 0, $idx);
		$symbol = substr($s, $idx + 1);
		load_dl($filename, $symbol);
		close($h0);
		$h0 = undef;
	    }
	}
    }
    if ($out2) {
	die($out2);
    }
    if (!$filename || !$symbol) {
	die("failed to get symbol");
    }
    close($h0) if $h0;
    close($h1) if $h1;
    close($h2) if $h2;
}

1;
__END__
