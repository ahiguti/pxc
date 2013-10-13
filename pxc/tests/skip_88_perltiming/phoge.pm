package phoge;

sub new {
  my $o = +[];
  return bless $o;
}

sub hoge_resize_w {
  # my $o = $_[0];
}

sub hoge_get_w {
  return $_[0]->[$_[1]];
  # my ($o, $i) = @_;
  # return $o->[$i];
}

sub hoge_set_w {
  $_[0]->[$_[1]] = $_[2];
  # my ($o, $i, $v) = @_;
  # $o->[$i] = $v;
}

1;
