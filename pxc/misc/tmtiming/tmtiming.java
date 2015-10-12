
import java.util.*;

class tmtiming {
  static final int test_size = 10000000;
  static final int loop = 1;
  static private String[] aval = null;
  // static private TreeMap<String, String> mval = null;
  static private HashMap<String, String> mval = null;
  static long test1(long v) {
    long x = 1;
    int sz = aval.length;
    for (int i = 0; i < sz; ++i) {
      x = (x * 1664525 + 1013904223) & 0xffffffffL;
      String k = aval[(int)(x % (long)test_size)];
      // System.out.println(k);
      // String k = aval[i];
      String m = mval.get(k);
      // System.out.println(m);
      v += m.length();
    }
    return v;
  }
  static public void main(String args[]) {
    // mval = new TreeMap<String, String>();
    // mval = new HashMap<String, String>(test_size * 2, 0.3f);
    mval = new HashMap<String, String>(test_size * 2);
    aval = new String[test_size];
    for (int i = 0; i < test_size; ++i) {
      String s = Integer.toString(i);
      aval[i] = Integer.toString(i);
      // aval[i] = s;
      mval.put(s, s + "val");
    }
    long t1 = System.currentTimeMillis();
    long v = 0;
    for (int i = 0; i < loop; ++i) {
      v = test1(v);
    }
    long t2 = System.currentTimeMillis();
    System.out.println("value: " + v);
    System.out.println("time: " + (t2 - t1));
  }
}

