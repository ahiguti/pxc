
import java.lang.System.*;
import java.lang.Integer.*;

public class heaptest1 {
  public static class entry {
    public final int intval;
    public entry(int v) {
      intval = v;
    }
  }
  public static int test_main(int len, int loop) {
    entry [] arr = new entry[len];
    int v = 0;
    int x = 0;
    for (int i = 0; i < loop; ++i) {
      for (int j = 0; j < len; ++j) {
	entry e = arr[j];
	if (e != null) {
	  v += e.intval;
	}
	e = new entry(x);
	arr[j] = e;
	++x;
      }
    }
    return v;
  }
  public static void main(String [] args) {
  int len = args.length > 0 ? Integer.parseInt(args[0]) : 10000000;
  int loop = args.length > 1 ? Integer.parseInt(args[1]) : 10;
  int v = test_main(len, loop);
  System.out.println(v);
 }
}

//        /usr/bin/time java -server -verbose:gc \
//                -Xms1000m -Xmx1000m -XX:NewSize=800m -XX:MaxNewSize=800m \
//                -XX:MaxTenuringThreshold=15 \
//                -XX:SurvivorRatio=2 \
//                -XX:TargetSurvivorRatio=90 \
//                heaptest1 10000000 100

