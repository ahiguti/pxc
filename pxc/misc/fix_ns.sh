
for j in test_*; do
  pushd $j
  for i in *.px; do echo `basename $i .px`; done > t.txt
  for i in `cat t.txt`
    do perl -pi -e "s/namespace\\s+.+;/namespace $i;/" $i.px
  done 
  rm -f t.txt
  popd
done
