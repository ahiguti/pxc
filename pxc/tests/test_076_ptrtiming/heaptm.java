import java.util.ArrayList;
class heaptm {
  heaptm(int v, heaptm f, heaptm s) {
    value = 1; // v;
    fst = f;
    snd = s;
  }
  int value;
  heaptm fst;
  heaptm snd;
  int sum() {
    int r = value;
    if (fst != null) { r += fst.sum(); }
    if (snd != null) { r += snd.sum(); }
    return r;
  }
  static heaptm create_tree(int depth) {
    if (depth <= 0) {
      return null;
    }
    return new heaptm(depth, create_tree(depth - 1), create_tree(depth - 1));
  }
  static int t1() {
    int r = 0;
    heaptm [] a = new heaptm[200];
    for (int i = 0; i < 200; ++i) {
      heaptm t = create_tree(16);
      a[i] = t;
    }
    for (int i = 0; i < 200; ++i) {
      r += a[i].sum();
    }
    return r;
  }
  public static void main(String args[]) {
    for (int i = 0; i < 20; ++i) {
      long t1 = System.currentTimeMillis();
      int r = t1();
      long t2 = System.currentTimeMillis();
      String s = r + "\t" + ((double)(t2 - t1) / 1000.0);
      System.out.println(s);
      // System.out.println((double)(t2 - t1) / 1000.0);
    }
  }
}

