
import java.util.*;

class tmtest {
  static int test1(TreeMap<Integer, Integer> m, int cnt, int v) {
    for (int i = 0; i < cnt; ++i) {
      v += m.get(i % 100);
    }
    return v;
  }
  static public void main(String args[]) {
    TreeMap<Integer, Integer> m = new TreeMap<Integer, Integer>();
    for (int i = 0; i < 100; ++i) {
      m.put(i, i);
    }
    long t1 = System.currentTimeMillis();
    int v = 0;
    for (int i = 0; i < 300; ++i) {
      v = test1(m, 1000000, v);
    }
    long t2 = System.currentTimeMillis();
    System.out.println(v);
    System.out.println(t2 - t1);
  }
}
