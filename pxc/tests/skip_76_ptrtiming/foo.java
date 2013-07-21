import java.util.ArrayList;
class foo {
  foo(int x0, int y0, int z0) {
    x = x0;
    y = y0;
    z = z0;
  }
  int x;
  int y;
  int z;
  private static int t1() {
    int r = 0;
    for (int i = 0; i < 10000; ++i) {
      for (int j = 0; j < 10000; ++j) {
	foo p = new foo(i, j, i + j); // placed on the stack
	r += p.x;
	r += p.y;
	r += p.z;
      }
    }
    return r;
  }
  private static int t2() {
    int r = 0;
    foo [] a = new foo[10000];
    for (int i = 0; i < 10000; ++i) {
      for (int j = 0; j < 10000; ++j) {
	foo p = new foo(i, j, i + j);
	a[j] = p;
      }
      for (int j = 0; j < 10000; ++j) {
	r += a[j].x;
	r += a[j].y;
	r += a[j].z;
      }
    }
    return r;
  }
  public static void main(String args[]) {
    for (int i = 0; i < 3; ++i) {
      long t1 = System.currentTimeMillis();
      int r = t1();
      long t2 = System.currentTimeMillis();
      System.out.println(r);
      System.out.println((double)(t2 - t1) / 1000.0);
    }
    for (int i = 0; i < 3; ++i) {
      long t1 = System.currentTimeMillis();
      int r = t2();
      long t2 = System.currentTimeMillis();
      System.out.println(r);
      System.out.println((double)(t2 - t1) / 1000.0);
    }
  }
}

