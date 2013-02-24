public class arrlist {
  arrlist() {
    arr = null;
    alloc_len = 0;
    valid_len = 0;
  }
  void append(char ch) {
    if (valid_len >= alloc_len) {
      int nlen = alloc_len;
      while (valid_len >= nlen) {
	nlen = nlen == 0 ? 1 : nlen * 2;
      }
      char [] narr = new char[nlen];
      for (int i = 0; i < valid_len; ++i) {
	narr[i] = arr[i];
      }
      arr = narr;
      alloc_len = nlen;
    }
    arr[valid_len] = ch;
    ++valid_len;
    return;
  }
  char charAt(int i) {
    return arr[i];
  }
  private char [] arr;
  private int valid_len;
  private int alloc_len;

  public static long test_arrlist(long len, boolean reserve_first)
  {
    if (len == 0) {
      len = 1000;
    }
    arrlist v = new arrlist();
    // StringBuilder v = new StringBuilder();
    for (int i = 0; i < len; ++i) {
      char x = (char)(i & 0xff);
      v.append(x);
    }
    long r = 0;
    for (int i = 0; i < len; ++i) {
      long x = v.charAt(i);
      r += x;
    }
    return r;
  }
  public static void main(String [] args)
  {
    long len = 0;
    long r = 0;
    for (long i = 0; i < 1000000; ++i) {
      r += test_arrlist(len, false);
    }
    System.out.println(r);
  }
}
